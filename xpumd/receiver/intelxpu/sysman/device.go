//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package sysman

import (
	"fmt"
	"iter"
	"net/url"
	"strings"
	"sync"

	"go.opentelemetry.io/collector/pdata/pcommon"
	"go.uber.org/zap"

	l0sysman "github.com/intel/level-zero-go/sysman"
	"github.com/intel/xpumanager/xpumd/receiver/intelxpu/sysman/internal/metadata"
)

type deviceRegistry struct {
	drivers []*driver
}

type driver struct {
	driver  *l0sysman.Driver
	devices []*device
}

type device struct {
	sync.RWMutex
	*l0sysman.Device
	logger     *zap.SugaredLogger
	attributes deviceAttributes
	state      deviceState
	scrapers   []instanceScraper

	aggregatedMetricsBufferSize int
}

type pciState struct {
	stateDisabled bool
	statesSeen    map[l0sysman.PciLinkStatus]bool
	qualitySeen   l0sysman.PciLinkQualIssueFlags
	stabilitySeen l0sysman.PciLinkStabIssueFlags
	stats         *l0sysman.PciStats
}

type eccState struct {
	states       map[string]bool
	current      string
	configurable bool
}

// deviceState has values that can change after enumeration.
type deviceState struct {
	devStateDisabled bool
	pci              pciState
	ecc              *eccState
}

// deviceAttributes fields for numeric items that are always reported,
// but which value may be unknown (e.g. pciLinkGen), are stored as
// strings instead of ints. That way it's clearer when info is missing
// (value = "" instead of "0").
type deviceAttributes struct {
	hwID              string
	hwName            string
	hwNamePci         string
	pciBDF            string
	pciVendorID       string
	pciDeviceID       string
	hwModel           string
	hwSerialNumber    string
	hwVendor          string
	hwFirmwareVersion string
	hwGpuType         metadata.AttributeHwGpuType
	subDevCount       int64
	maxBandwidth      int64 // zero = not available, skip PCI BW max/ratio metrics
	pciLanes          string
	pciLinkGen        string
	demandPaging      bool
	eccSupport        metadata.AttributeHwMemoryEcc
}

func newDeviceRegistry(logger *zap.SugaredLogger, aggregatedMetricsBufferSize int) (*deviceRegistry, error) {
	reg := &deviceRegistry{}

	drivers, err := l0sysman.DriverGet()
	if err != nil {
		return nil, fmt.Errorf("failed to get ZES drivers: %w", err)
	}
	for i, drv := range drivers {
		devs, err := enumDevices(drv, logger, aggregatedMetricsBufferSize)
		if err != nil {
			return nil, fmt.Errorf("failed to enumerate devices for driver %d/%d: %w", i+1, len(drivers), err)
		}

		reg.drivers = append(reg.drivers, &driver{driver: drv, devices: devs})
	}

	return reg, nil
}

func (r *deviceRegistry) scrape(mb *metadata.MetricsBuilder, ts pcommon.Timestamp) {
	for dev := range r.devices() {
		dev.scrape(mb, ts)
	}
}

func (r *deviceRegistry) pollAggregatedMetrics() {
	for dev := range r.devices() {
		dev.pollAggregatedMetrics()
	}
}

func (r *deviceRegistry) devices() iter.Seq[*device] {
	return func(yield func(*device) bool) {
		for _, drv := range r.drivers {
			for _, dev := range drv.devices {
				if !yield(dev) {
					return
				}
			}
		}
	}
}

func enumDevices(driver *l0sysman.Driver, logger *zap.SugaredLogger, aggregatedMetricsBufferSize int) ([]*device, error) {
	zesDevs, err := driver.DeviceGet()
	if err != nil {
		return nil, fmt.Errorf("failed to get ZES devices: %w", err)
	}
	devs := make([]*device, len(zesDevs))
	for i, d := range zesDevs {
		name := fmt.Sprintf("gpu-%d", i+1) // match error log index
		dev, err := newDevice(name, d, logger, aggregatedMetricsBufferSize)
		if err != nil {
			return nil, fmt.Errorf("failed to create Sysman device %d/%d: %w", i+1, len(devs), err)
		}

		devs[i] = dev
	}

	logger.Infow("Sysman devices", "created", len(devs), "enumerated", len(zesDevs))
	return devs, nil
}

// newDevice creates a new device. Use init() to scan the device.
func newDevice(name string, dev *l0sysman.Device, logger *zap.SugaredLogger, aggregatedMetricsBufferSize int) (*device, error) {
	d := &device{
		Device: dev,
		logger: logger,
		attributes: deviceAttributes{
			hwName:    name,
			hwNamePci: name + "-pci",
		},
		aggregatedMetricsBufferSize: aggregatedMetricsBufferSize,
	}

	if err := d.init(); err != nil {
		return nil, err
	}
	return d, nil
}

// init scans and initializes the device attributes, state and scrapers. May be
// called multiple times for re-scanning. Transactional: on error the device is not altered.
// Must be called with device lock held.
func (d *device) init() error {
	props, err := d.GetProperties()
	if err != nil {
		return fmt.Errorf("device GetProperties() failed: %w", err)
	}

	// Preserve name fields assigned at construction, refresh everything else.
	d.attributes = deviceAttributes{
		hwName:    d.attributes.hwName,
		hwNamePci: d.attributes.hwNamePci,
		// TODO: use (Sysman ext) props.Uuid.Id.String()?
		hwID:           props.Core.Uuid.Id.String(),
		pciDeviceID:    fmt.Sprintf("%04x", props.Core.DeviceId),
		pciVendorID:    fmt.Sprintf("%04x", props.Core.VendorId),
		hwModel:        props.ModelName.String(),
		hwSerialNumber: props.SerialNumber.String(),
		hwVendor:       props.VendorName.String(),
		subDevCount:    int64(props.NumSubdevices),
		hwGpuType:      gpuType(props.Flags),
		demandPaging:   props.Flags&l0sysman.DevicePropertyFlags(l0sysman.DEVICE_PROPERTY_FLAG_ONDEMANDPAGING) != 0,
	}
	d.state = deviceState{
		ecc: &eccState{
			states: map[string]bool{},
		},
	}
	d.scrapers = nil

	_ = d.updateEccState()
	if d.state.ecc.configurable {
		d.attributes.eccSupport = metadata.AttributeHwMemoryEccConfigurable
	} else {
		d.attributes.eccSupport = metadata.MapAttributeHwMemoryEcc[d.state.ecc.current]
	}

	// Get firmware info
	fwInfos := []string{}
	for i, fw := range enumFirmwares(d) {
		fwInfos = append(fwInfos, fmt.Sprintf("%d:%s:%s:%s", i, fw.attributes.firmwareName, fw.attributes.subdeviceId, url.QueryEscape(fw.attributes.firmwareVersion)))
	}
	d.attributes.hwFirmwareVersion = strings.Join(fwInfos, ",")

	// Get device PCI attributes
	if pci, err := d.PciGetProperties(); err != nil {
		d.logger.Errorw("Device PciGetProperties() failed: no PCI attributes", "error", err, "deviceAttributes", d.attributes)
	} else {
		d.attributes.pciBDF = fmt.Sprintf("%04x:%02x:%02x.%x", pci.Address.Domain, pci.Address.Bus, pci.Address.Device, pci.Address.Function)
		if pci.MaxSpeed.Gen > 0 {
			d.attributes.pciLinkGen = fmt.Sprintf("%d", pci.MaxSpeed.Gen)
		}
		if pci.MaxSpeed.Width > 0 {
			d.attributes.pciLanes = fmt.Sprintf("%d", pci.MaxSpeed.Width)
		}
		if pci.MaxSpeed.MaxBandwidth > 0 {
			d.attributes.maxBandwidth = pci.MaxSpeed.MaxBandwidth
		} else {
			d.logger.Infow("Device PciGetProperties(): PCI max BW not available", "attributes", d.attributes)
		}
		if pci.HaveBandwidthCounters != 0 {
			if stats, err := d.PciGetStats(); err == nil {
				d.state.pci.stats = &stats
			} else {
				d.logger.Infow("Device PciGetStats() failed: PCI BW not available", zap.Error(err), "attributes", d.attributes)
			}
		} else {
			d.logger.Infow("Device PciGetProperties(): PCI BW counters not available", "attributes", d.attributes)
		}
	}

	// Check device + PCI state availability
	if _, err := d.GetState(); err != nil {
		d.logger.Infow("Device GetState() failed: device state not available", zap.Error(err), "deviceAttributes", d.attributes)
		d.state.devStateDisabled = true
	}
	if _, err := d.PciGetState(); err != nil {
		d.logger.Infow("Device PciGetState() failed: PCI state not available", zap.Error(err), "deviceAttributes", d.attributes)
		d.state.pci.stateDisabled = true
	}

	// TODO: report types & sizes of device PCI BARs?
	// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdevicepcigetbars

	for _, s := range subsystems {
		d.scrapers = append(d.scrapers, s.enumDevice(d)...)
	}

	return nil
}

func gpuType(flags l0sysman.DevicePropertyFlags) metadata.AttributeHwGpuType {

	// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zes-device-property-flags-t
	switch {
	case flags&l0sysman.DevicePropertyFlags(l0sysman.DEVICE_PROPERTY_FLAG_SUBDEVICE) != 0:
		return metadata.AttributeHwGpuTypeSubdevice
	case flags&l0sysman.DevicePropertyFlags(l0sysman.DEVICE_PROPERTY_FLAG_INTEGRATED) != 0:
		return metadata.AttributeHwGpuTypeIntegrated
	default:
		return metadata.AttributeHwGpuTypeDiscrete
	}
}

// updateEccState updates the ECC state of the device. Must be called with device lock held.
func (d *device) updateEccState() error {
	configurable := false
	retval := error(nil)
	var name string

	// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#ecc
	if available, err := d.EccAvailable(); err != nil {
		name = "unknown"
		retval = err
	} else if !available {
		name = "unavailable" // same as from GetEccState()
	} else if isConfigurable, err := d.EccConfigurable(); err != nil {
		name = "unknown"
		retval = err
	} else if !isConfigurable {
		name = "available"
	} else if state, err := d.GetEccState(); err != nil {
		name = "unknown"
		retval = err
	} else {
		name = strings.ToLower(state.CurrentState.String())
		if state.CurrentState != l0sysman.DEVICE_ECC_STATE_UNAVAILABLE {
			configurable = true
		}
	}

	d.state.ecc.configurable = configurable
	d.state.ecc.states[name] = true
	d.state.ecc.current = name

	return retval
}

func (d *device) scrape(mb *metadata.MetricsBuilder, ts pcommon.Timestamp) {
	d.RLock()
	defer d.RUnlock()

	if d.state.ecc.configurable {
		err := d.updateEccState()
		if !d.state.ecc.configurable {
			d.logger.Errorw("ECC become non-configurable, disabling state querying", "states", d.state.ecc, zap.Error(err), "attributes", d.attributes)
		}
	}

	// Report ECC state(s) if state is configurable or changed
	if d.state.ecc.configurable || len(d.state.ecc.states) > 1 {
		for state := range d.state.ecc.states {
			value := int64(0)
			if state == d.state.ecc.current {
				value = 1
			}
			mb.RecordHwStatusDataPoint(ts, value,
				d.attributes.hwID,
				d.attributes.hwName,
				d.attributes.pciBDF,
				"", // not subdevice
				"ecc_"+state,
				metadata.AttributeHwTypeGpu,
			)
		}
	}

	mb.RecordHwGpuInfoDataPoint(ts, 1,
		d.attributes.hwID,
		d.attributes.hwName,
		d.attributes.pciBDF,
		d.attributes.pciVendorID,
		d.attributes.pciDeviceID,
		d.attributes.hwModel,
		d.attributes.hwSerialNumber,
		d.attributes.hwVendor,
		d.attributes.hwFirmwareVersion,
		d.attributes.hwGpuType,
		d.attributes.subDevCount,
		d.attributes.pciLanes,
		d.attributes.pciLinkGen,
		d.attributes.demandPaging,
		d.attributes.eccSupport,
	)

	d.scrapeDevState(mb, ts)
	d.scrapePciState(mb, ts)
	d.scrapePciStats(mb, ts)

	for _, s := range d.scrapers {
		s.scrape(mb, ts)
	}
}

// scrapeDevState reports device (reset) state
func (d *device) scrapeDevState(mb *metadata.MetricsBuilder, ts pcommon.Timestamp) {
	if d.state.devStateDisabled {
		return
	}

	// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zes-device-state-t
	state, err := d.GetState()
	if err != nil {
		d.logger.Errorw("Device GetState() failed: device reset state metric disabled", zap.Error(err), "deviceAttributes", d.attributes)
		d.state.devStateDisabled = true
		return
	}

	reset := int64(0)
	// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zes-reset-reason-flags-t
	if state.Reset != 0 {
		reset = 1
	}

	mb.RecordHwStatusDataPoint(ts, reset,
		d.attributes.hwID,
		d.attributes.hwName,
		d.attributes.pciBDF,
		"", // not subdevice
		"reset_needed",
		metadata.AttributeHwTypeGpu,
	)
}

// scrapePciState reports device PCI link state(s)
func (d *device) scrapePciState(mb *metadata.MetricsBuilder, ts pcommon.Timestamp) {
	if d.state.pci.stateDisabled {
		return
	}

	// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zes-pci-state-t
	pci, err := d.PciGetState()
	if err != nil {
		d.logger.Errorw("Device PciGetState() failed: PCI link state metrics disabled", zap.Error(err), "deviceAttributes", d.attributes)
		d.state.pci.stateDisabled = true
		return
	}

	if d.state.pci.statesSeen == nil {
		// skip unknown PCI state reporting until state has been known
		if pci.Status == l0sysman.PCI_LINK_STATUS_UNKNOWN {
			return
		}
		d.state.pci.statesSeen = map[l0sysman.PciLinkStatus]bool{}
	}

	// Collect states that are either active, or have been reported before.
	// Because names of the PCI statuses and quality & stability issues do not overlap,
	// single mapping is enough for all of them
	states := map[string]bool{}
	for flag := range d.state.pci.statesSeen {
		states[flag.String()] = false
	}

	// status is enumerator, not a bitmask, so only one of these is relevant
	switch pci.Status {
	case l0sysman.PCI_LINK_STATUS_QUALITY_ISSUES:
		d.state.pci.qualitySeen |= pci.QualityIssues
	case l0sysman.PCI_LINK_STATUS_STABILITY_ISSUES:
		d.state.pci.stabilitySeen |= pci.StabilityIssues
	default:
		switch pci.Status {
		// recognized values
		case l0sysman.PCI_LINK_STATUS_GOOD:
		case l0sysman.PCI_LINK_STATUS_UNKNOWN:
		default:
			// report each unrecognized value from Sysman backend only once
			if !d.state.pci.statesSeen[pci.Status] {
				d.logger.Errorw("Unrecognized PciGetState() value", "pci.Status", pci.Status, "attributes", d.attributes)
			}
		}
		d.state.pci.statesSeen[pci.Status] = true
		states[pci.Status.String()] = true
	}

	// add (issue) states from bitmasks
	for _, bit := range d.state.pci.qualitySeen.Bits() {
		issue := bit.String() + "_issue"
		states[issue] = (l0sysman.PciLinkQualIssueFlag(pci.QualityIssues) & bit) != 0
	}
	for _, bit := range d.state.pci.stabilitySeen.Bits() {
		issue := bit.String() + "_issue"
		states[issue] = (l0sysman.PciLinkStabIssueFlag(pci.StabilityIssues) & bit) != 0
	}

	// report status of currently active & previously seen PCI link states
	for state, active := range states {
		value := int64(0)
		if active {
			value = 1
		}
		state = strings.ToLower(state)
		if state == "good" {
			state = "ok"
		}
		mb.RecordHwStatusDataPoint(ts, value,
			d.attributes.hwID,
			d.attributes.hwNamePci,
			d.attributes.pciBDF,
			"", // not subdevice
			state,
			metadata.AttributeHwTypePciLink,
		)
	}
}

func (d *device) scrapePciStats(mb *metadata.MetricsBuilder, ts pcommon.Timestamp) {
	if d.state.pci.stats == nil {
		return
	}

	stats, err := d.PciGetStats()
	if err != nil {
		d.logger.Errorw("Device PciGetStats() failed: PCI BW metrics disabled", zap.Error(err), "attributes", d.attributes)
		d.state.pci.stats = nil
		return
	}

	// TODO: check from visualization that change rate for
	// PCI counters matches BW rate values calculated below
	// (if not, caller's timestamps do not match KMD timestamps)

	// read / write counters
	mb.RecordHwGpuIoDataPoint(
		ts, float64(stats.RxCounter),
		d.attributes.hwID,
		d.attributes.hwNamePci,
		d.attributes.pciBDF,
		metadata.AttributeNetworkIoDirectionReceive,
	)
	mb.RecordHwGpuIoDataPoint(
		ts, float64(stats.TxCounter),
		d.attributes.hwID,
		d.attributes.hwNamePci,
		d.attributes.pciBDF,
		metadata.AttributeNetworkIoDirectionTransmit,
	)

	// TODO: Sysman spec states neither timestamp nor counter bits,
	// so their values are assumed to wrap at full type width
	timeDiff := u64CounterDiff(d.state.pci.stats.Timestamp, stats.Timestamp)
	rxDiff := u64CounterDiff(d.state.pci.stats.RxCounter, stats.RxCounter)
	txDiff := u64CounterDiff(d.state.pci.stats.TxCounter, stats.TxCounter)
	d.state.pci.stats = &stats

	if timeDiff == 0 {
		return
	}

	// total BW rate, b/us -> b/s
	rate := float64(rxDiff+txDiff) / float64(timeDiff) * 1e6

	// TODO: OTel spec prefers utilization ratio instead of rate, but that
	// can be provided only when limit is known, and counters can be more
	// easily validated against rate in dashboard.
	// => drop later on, if limit is available on all relevant HW
	mb.RecordHwGpuIoRateDataPoint(
		ts, rate,
		d.attributes.hwID,
		d.attributes.hwNamePci,
		d.attributes.pciBDF,
	)

	if d.attributes.maxBandwidth > 0 {
		// TODO: verify that max is for read+write, not just one direction

		// max BW
		mb.RecordHwGpuBandwidthLimitDataPoint(
			ts, d.attributes.maxBandwidth,
			d.attributes.hwID,
			d.attributes.hwNamePci,
			d.attributes.pciBDF,
		)

		// BW utilization ratio
		mb.RecordHwGpuBandwidthUtilizationDataPoint(
			ts, rate/float64(d.attributes.maxBandwidth),
			d.attributes.hwID,
			d.attributes.hwNamePci,
			d.attributes.pciBDF,
		)
	}
}

func (d *device) pollAggregatedMetrics() {
	d.RLock()
	defer d.RUnlock()
	for _, s := range d.scrapers {
		s.pollAggregatedMetrics()
	}
}
