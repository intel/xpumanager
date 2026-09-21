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
	l0intel "github.com/intel/level-zero-go/sysman/exp/intel"
	"github.com/intel/xpumanager/xpumd/receiver/intelxpu/sysman/internal/metadata"
)

type deviceRegistry struct {
	drivers []*driver
}

// driver is one Sysman driver and everything enumerated from it. It is wrapped
// for the Intel experimental extensions that provide also the non-experimental
// Sysman functionality.
type driver struct {
	*l0intel.Driver
	devices []*device
	// infoLogs are the info logs of the driver whose records can be collected,
	// empty when it has none or when their collection is disabled.
	infoLogs []*infoLogSource
}

type device struct {
	sync.RWMutex
	*l0sysman.Device
	logger       *zap.SugaredLogger
	attributes   deviceAttributes
	constMetrics pciConstMetrics
	state        deviceState
	scrapers     []instanceScraper

	aggregatedMetricsBufferSize int
}

type pciState struct {
	stateDisabled bool
	statesSeen    map[l0sysman.PciLinkStatus]bool
	qualitySeen   l0sysman.PciLinkQualIssueFlags
	stabilitySeen l0sysman.PciLinkStabIssueFlags
	stats         *l0sysman.PciStats
}

// pciConstMetrics holds device PCI metric values that are determined once and do not change afterwards.
type pciConstMetrics struct {
	maxBandwidth int64 // zero = not available
}

type eccState struct {
	states       map[string]bool
	current      string
	configurable bool
}

// deviceState has values that can change after enumeration.
type deviceState struct {
	initialized      bool
	devStateDisabled bool
	pci              pciState
	ecc              *eccState
	stateExtSeen     l0sysman.DeviceStateExtFlags
}

// deviceAttributes fields for numeric items that are always reported,
// but which value may be unknown (e.g. PciLinkGen), are stored as
// strings instead of ints. That way it's clearer when info is missing
// (value = "" instead of "0").
type deviceAttributes struct {
	HwID                 string                            `json:"hw.id"`
	HwName               string                            `json:"hw.name"`
	hwNamePci            string                            // not exported as no corresponding device metric attribute
	PciBDF               string                            `json:"pci.bdf"`
	PciVendorID          string                            `json:"pci.vendor_id"`
	PciDeviceID          string                            `json:"pci.device_id"`
	HwModel              string                            `json:"hw.model"`
	HwSerialNumber       string                            `json:"hw.serial_number"`
	HwVendor             string                            `json:"hw.vendor"`
	HwFirmwareVersion    string                            `json:"hw.firmware_version"`
	HwGpuType            metadata.AttributeHwGpuType       `json:"hw.gpu.type"`
	SubdeviceCount       int64                             `json:"com.intel.subdevice_count"`
	PciLanes             string                            `json:"pci.lanes"`
	PciLinkGen           string                            `json:"pci.link_gen"`
	HwMemoryDemandPaging bool                              `json:"hw.memory.demand_paging"`
	HwGpuEccSupport      metadata.AttributeHwGpuEccSupport `json:"hw.gpu.ecc.support"`
}

func newDeviceRegistry(logger *zap.SugaredLogger, cfg *Config) (*deviceRegistry, error) {
	reg := &deviceRegistry{}

	drivers, err := l0sysman.DriverGet()
	if err != nil {
		return nil, fmt.Errorf("failed to get ZES drivers: %w", err)
	}
	logger.Debugw("Sysman drivers", "enumerated", len(drivers))

	for i, drv := range drivers {
		devs, err := enumDevices(drv, logger, cfg.aggregatedMetricsBufferSize)
		if err != nil {
			return nil, fmt.Errorf("failed to enumerate devices for driver %d/%d: %w", i+1, len(drivers), err)
		}

		d := &driver{Driver: l0intel.NewDriver(drv), devices: devs}
		// We only enumerate info logs if collection is enabled
		// TODO: consider simplifying (always enumerate) when the extension is stable
		if cfg.InfoLogs.Enabled {
			d.infoLogs = enumInfoLogs(d, i, cfg.InfoLogs, logger)
		}

		reg.drivers = append(reg.drivers, d)
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
	logger.Infow("Sysman devices", "enumerated", len(zesDevs))

	devs := make([]*device, 0, len(zesDevs))
	for i, d := range zesDevs {
		name := fmt.Sprintf("gpu-%d", i+1) // match error log index
		devs = append(devs, newDevice(name, d, logger, aggregatedMetricsBufferSize))
	}

	return devs, nil
}

// newDevice creates a new device. Use init() to scan the device.
func newDevice(name string, dev *l0sysman.Device, logger *zap.SugaredLogger, aggregatedMetricsBufferSize int) *device {
	d := &device{
		Device: dev,
		logger: logger,
		attributes: deviceAttributes{
			HwName:    name,
			hwNamePci: name + "-pci",
		},
		aggregatedMetricsBufferSize: aggregatedMetricsBufferSize,
	}

	if err := d.init(); err != nil {
		logger.Errorw("Failed to initialize Sysman device", "name", name, "error", err)
	}
	return d
}

// init scans and initializes the device attributes, state and scrapers. May be
// called multiple times for re-scanning. Transactional: on error the device is not altered.
// Must be called with device lock held.
func (d *device) init() error {
	// Intermediate startup timings
	d.logger.Debugw("Device init() called", "name", d.attributes.HwName)

	props, err := d.GetProperties()
	if err != nil {
		return fmt.Errorf("device GetProperties() failed: %w", err)
	}

	// Preserve name fields assigned at construction, refresh everything else.
	d.attributes = deviceAttributes{
		HwName:    d.attributes.HwName,
		hwNamePci: d.attributes.hwNamePci,
		// TODO: use (Sysman ext) props.Uuid.Id.String()?
		HwID:                 props.Core.Uuid.Id.String(),
		PciDeviceID:          fmt.Sprintf("%04x", props.Core.DeviceId),
		PciVendorID:          fmt.Sprintf("%04x", props.Core.VendorId),
		HwModel:              props.ModelName.String(),
		HwSerialNumber:       props.SerialNumber.String(),
		HwVendor:             props.VendorName.String(),
		SubdeviceCount:       int64(props.NumSubdevices),
		HwGpuType:            gpuType(props.Flags),
		HwMemoryDemandPaging: props.Flags&l0sysman.DevicePropertyFlags(l0sysman.DEVICE_PROPERTY_FLAG_ONDEMANDPAGING) != 0,
	}
	d.constMetrics = pciConstMetrics{}
	d.state = deviceState{
		ecc: &eccState{
			states: map[string]bool{},
		},
	}
	d.scrapers = nil

	_ = d.updateEccState()
	if d.state.ecc.configurable {
		d.attributes.HwGpuEccSupport = metadata.AttributeHwGpuEccSupportConfigurable
	} else {
		d.attributes.HwGpuEccSupport = metadata.MapAttributeHwGpuEccSupport[d.state.ecc.current]
	}

	d.logger.Debugw("Device init() props + ECC done", "name", d.attributes.HwName)

	// Get device PCI attributes
	if pci, err := d.PciGetProperties(); err != nil {
		d.logger.Errorw("Device PciGetProperties() failed: no PCI attributes", "error", err, "attributes", d.attributes)
	} else {
		d.attributes.PciBDF = pciBDF(pci.Address)
		if pci.MaxSpeed.Gen > 0 {
			d.attributes.PciLinkGen = fmt.Sprintf("%d", pci.MaxSpeed.Gen)
		}
		if pci.MaxSpeed.Width > 0 {
			d.attributes.PciLanes = fmt.Sprintf("%d", pci.MaxSpeed.Width)
		}
		if pci.MaxSpeed.MaxBandwidth > 0 {
			d.constMetrics = pciConstMetrics{maxBandwidth: pci.MaxSpeed.MaxBandwidth}
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
		d.logger.Infow("Device GetState() failed: device state not available", zap.Error(err), "attributes", d.attributes)
		d.state.devStateDisabled = true
	}
	if _, err := d.PciGetState(); err != nil {
		d.logger.Infow("Device PciGetState() failed: PCI state not available", zap.Error(err), "attributes", d.attributes)
		d.state.pci.stateDisabled = true
	}

	// TODO: report types & sizes of device PCI BARs?
	// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdevicepcigetbars

	// After this, enumerations do enough logging for timing info to be available at adequate granularity
	d.logger.Debugw("Device init() state + PCI props done", "name", d.attributes.HwName, "BDF", d.attributes.PciBDF)

	// Enumerate device firmwares into versions attribute
	fwInfos := []string{}
	for i, fw := range enumFirmwares(d) {
		fwInfos = append(fwInfos, fmt.Sprintf("%d:%s:%s:%s", i, fw.info.firmwareName, fw.info.subdeviceId, url.QueryEscape(fw.info.firmwareVersion)))
	}
	d.attributes.HwFirmwareVersion = strings.Join(fwInfos, ",")

	// Enumerate all device metrics
	for _, s := range subsystems {
		d.scrapers = append(d.scrapers, s.enumDevice(d)...)
	}

	d.logger.Debugw("Device init() done", "name", d.attributes.HwName, "BDF", d.attributes.PciBDF)
	d.state.initialized = true
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

// stateAggregator aggregates state from multiple sources.
type stateAggregator struct {
	isValid   bool
	hasIssues bool
}

func (s *stateAggregator) addSource(issue bool) {
	s.isValid = true
	s.hasIssues = s.hasIssues || issue
}

func (d *device) scrape(mb *metadata.MetricsBuilder, ts pcommon.Timestamp) {
	d.RLock()
	defer d.RUnlock()

	if !d.state.initialized {
		return
	}
	d.scrapeEccState(mb, ts)

	mb.RecordHwGpuInfoDataPoint(ts, 1,
		d.attributes.HwID,
		d.attributes.HwName,
		d.attributes.PciBDF,
		d.attributes.PciVendorID,
		d.attributes.PciDeviceID,
		d.attributes.HwModel,
		d.attributes.HwSerialNumber,
		d.attributes.HwVendor,
		d.attributes.HwFirmwareVersion,
		d.attributes.HwGpuType,
		d.attributes.SubdeviceCount,
		d.attributes.PciLanes,
		d.attributes.PciLinkGen,
		d.attributes.HwMemoryDemandPaging,
		d.attributes.HwGpuEccSupport,
	)

	status := &stateAggregator{}
	d.scrapeDevState(mb, ts, status)
	d.recordHwStatusGpuOk(mb, ts, status)

	d.scrapePciState(mb, ts)
	d.scrapePciStats(mb, ts)

	for _, s := range d.scrapers {
		s.scrape(mb, ts)
	}
}

// scrapeEccState reports the device memory ECC state(s). States are reported
// only if the ECC state is configurable, or if it has changed.
func (d *device) scrapeEccState(mb *metadata.MetricsBuilder, ts pcommon.Timestamp) {
	if d.state.ecc.configurable {
		err := d.updateEccState()
		if !d.state.ecc.configurable {
			d.logger.Errorw("ECC become non-configurable, disabling state querying", "states", d.state.ecc, zap.Error(err), "attributes", d.attributes)
		}
	}

	if !d.state.ecc.configurable && len(d.state.ecc.states) < 2 {
		return
	}

	for state := range d.state.ecc.states {
		value := int64(0)
		if state == d.state.ecc.current {
			value = 1
		}
		mb.RecordHwGpuEccStateDataPoint(ts, value,
			d.attributes.HwID,
			d.attributes.HwName,
			d.attributes.PciBDF,
			"", // not subdevice
			state,
		)
	}
}

// recordHwStatusGpuOk reports the aggregated hw.status{hw.type="gpu"} "ok" state.
func (d *device) recordHwStatusGpuOk(mb *metadata.MetricsBuilder, ts pcommon.Timestamp, status *stateAggregator) {
	if !status.isValid {
		return
	}

	value := int64(0)
	if !status.hasIssues {
		value = 1
	}

	mb.RecordHwStatusDataPoint(ts, value,
		d.attributes.HwID,
		d.attributes.HwName,
		d.attributes.PciBDF,
		"", // not subdevice
		"ok",
		metadata.AttributeHwTypeGpu,
	)
}

// scrapeDevState reports device (reset) state
func (d *device) scrapeDevState(mb *metadata.MetricsBuilder, ts pcommon.Timestamp, status *stateAggregator) {
	if d.state.devStateDisabled {
		return
	}

	// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zes-device-state-t
	state, err := d.GetState()
	if err != nil {
		d.logger.Errorw("Device GetState() failed: device reset state metric disabled", zap.Error(err), "attributes", d.attributes)
		d.state.devStateDisabled = true
		return
	}

	// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zes-reset-reason-flags-t
	resetNeeded := state.Reset != 0
	status.addSource(resetNeeded)

	reset := int64(0)
	if resetNeeded {
		reset = 1
	}

	mb.RecordHwStatusDataPoint(ts, reset,
		d.attributes.HwID,
		d.attributes.HwName,
		d.attributes.PciBDF,
		"", // not subdevice
		"reset_needed",
		metadata.AttributeHwTypeGpu,
	)

	// Extended (optional) device state
	// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zes-device-ext-state-t
	if state.ExtendedState != nil {
		d.state.stateExtSeen |= state.ExtendedState.Flags
		// "normal" is the absence of an issue, all the other flags mark a problem
		issues := state.ExtendedState.Flags & ^l0sysman.DeviceStateExtFlags(l0sysman.DEVICE_STATE_EXT_FLAG_NORMAL)
		status.addSource(issues != 0)

		// report status of currently active & previously seen extended states
		for _, bit := range d.state.stateExtSeen.Bits() {
			if bit == l0sysman.DEVICE_STATE_EXT_FLAG_NORMAL {
				// "normal" is reported indirectly by the aggregated "ok" state, instead
				continue
			}
			value := int64(0)
			if l0sysman.DeviceStateExtFlags(state.ExtendedState.Flags)&l0sysman.DeviceStateExtFlags(bit) != 0 {
				value = 1
			}
			mb.RecordHwStatusDataPoint(ts, value,
				d.attributes.HwID,
				d.attributes.HwName,
				d.attributes.PciBDF,
				"", // not subdevice
				strings.ToLower(bit.String()),
				metadata.AttributeHwTypeGpu,
			)
		}
	}
}

// scrapePciState reports device PCI link state(s)
func (d *device) scrapePciState(mb *metadata.MetricsBuilder, ts pcommon.Timestamp) {
	if d.state.pci.stateDisabled {
		return
	}

	// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zes-pci-state-t
	pci, err := d.PciGetState()
	if err != nil {
		d.logger.Errorw("Device PciGetState() failed: PCI link state metrics disabled", zap.Error(err), "attributes", d.attributes)
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
			d.attributes.HwID,
			d.attributes.hwNamePci,
			d.attributes.PciBDF,
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
		d.attributes.HwID,
		d.attributes.hwNamePci,
		d.attributes.PciBDF,
		metadata.AttributeNetworkIoDirectionReceive,
	)
	mb.RecordHwGpuIoDataPoint(
		ts, float64(stats.TxCounter),
		d.attributes.HwID,
		d.attributes.hwNamePci,
		d.attributes.PciBDF,
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
		d.attributes.HwID,
		d.attributes.hwNamePci,
		d.attributes.PciBDF,
	)

	if d.constMetrics.maxBandwidth > 0 {
		// TODO: verify that max is for read+write, not just one direction

		// max BW
		mb.RecordHwGpuBandwidthLimitDataPoint(
			ts, d.constMetrics.maxBandwidth,
			d.attributes.HwID,
			d.attributes.hwNamePci,
			d.attributes.PciBDF,
		)

		// BW utilization ratio
		mb.RecordHwGpuBandwidthUtilizationDataPoint(
			ts, rate/float64(d.constMetrics.maxBandwidth),
			d.attributes.HwID,
			d.attributes.hwNamePci,
			d.attributes.PciBDF,
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
