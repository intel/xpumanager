//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0

package sysman

import (
	"fmt"
	"net/url"
	"strings"

	"go.opentelemetry.io/collector/pdata/pcommon"
	"go.uber.org/zap"

	l0sysman "github.com/intel/level-zero-go/sysman"
	"github.com/intel/xpumanager/receiver/sysman/internal/metadata"
)

type deviceRegistry struct {
	devices []*device
}

type device struct {
	*l0sysman.Device
	logger     *zap.SugaredLogger
	attributes deviceAttributes
	state      deviceState
	scrapers   []instanceScraper

	aggregatedMetricsBufferSize int
}

type deviceState struct {
	pciStats         *l0sysman.PciStats
	maxBandwidth     int64
}

type deviceAttributes struct {
	hwID              string
	hwType            metadata.AttributeHwType
	hwName            string
	hwModel           string
	hwSerialNumber    string
	hwVendor          string
	hwFirmwareVersion string
	pciBDF            string
	pciDeviceID       string
	pciVendorID       string
	pciLanes          string
	pciLinkGen        string
}

func newDeviceRegistry(logger *zap.SugaredLogger, aggregatedMetricsBufferSize int) (*deviceRegistry, error) {
	reg := &deviceRegistry{}

	drivers, err := l0sysman.DriverGet()
	if err != nil {
		return nil, fmt.Errorf("failed to get ZES drivers: %w", err)
	}
	for i, driver := range drivers {
		devs, err := enumDevices(driver, logger, aggregatedMetricsBufferSize)
		if err != nil {
			return nil, fmt.Errorf("failed to enumerate devices for driver %d/%d: %w", i+1, len(drivers), err)
		}

		reg.devices = append(reg.devices, devs...)
	}

	return reg, nil
}

func (r *deviceRegistry) scrape(mb *metadata.MetricsBuilder, ts pcommon.Timestamp) {
	for _, dev := range r.devices {
		dev.scrape(mb, ts)
	}
}

func (r *deviceRegistry) pollAggregatedMetrics() {
	for _, dev := range r.devices {
		dev.pollAggregatedMetrics()
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
	return devs, nil
}

func newDevice(name string, dev *l0sysman.Device, logger *zap.SugaredLogger, aggregatedMetricsBufferSize int) (*device, error) {
	props, err := dev.GetProperties()
	if err != nil {
		return nil, fmt.Errorf("device GetProperties() failed: %w", err)
	}

	d := &device{
		Device: dev,
		logger: logger,
		attributes: deviceAttributes{
			// TODO: use extended props UUID?
			hwID:           props.Core.Uuid.Id.String(),
			hwType:         metadata.AttributeHwTypeGpu,
			hwName:         name,
			hwModel:        props.ModelName.String(),
			hwSerialNumber: props.SerialNumber.String(),
			hwVendor:       props.VendorName.String(),
			pciDeviceID:    fmt.Sprintf("%04x", props.Core.DeviceId),
			pciVendorID:    fmt.Sprintf("%04x", props.Core.VendorId),
		},
		aggregatedMetricsBufferSize: aggregatedMetricsBufferSize,
	}

	// Get firmware info
	fwInfos := []string{}
	for i, fw := range enumFirmwares(d) {
		fwInfos = append(fwInfos, fmt.Sprintf("%d:%s:%s:%s", i, fw.attributes.firmwareName, fw.attributes.subdeviceId, url.QueryEscape(fw.attributes.firmwareVersion)))
	}
	d.attributes.hwFirmwareVersion = strings.Join(fwInfos, ",")

	// get device PCI attributes
	if pci, err := dev.PciGetProperties(); err != nil {
		logger.Errorw("Device PciGetProperties() failed => no PCI attributes", "error", err, "deviceAttributes", d.attributes)
	} else {
		d.attributes.pciBDF = fmt.Sprintf("%04x:%02x:%02x.%x", pci.Address.Domain, pci.Address.Bus, pci.Address.Device, pci.Address.Function)
		if pci.MaxSpeed.Gen > 0 {
			d.attributes.pciLinkGen = fmt.Sprintf("%d", pci.MaxSpeed.Gen)
		}
		if pci.MaxSpeed.Width > 0 {
			d.attributes.pciLanes = fmt.Sprintf("%d", pci.MaxSpeed.Width)
		}
		if pci.MaxSpeed.MaxBandwidth > 0 {
			d.state.maxBandwidth = pci.MaxSpeed.MaxBandwidth
		} else {
			d.logger.Warnw("Device PciGetProperties(): no PCI max BW", "attributes", d.attributes)
		}
		if pci.HaveBandwidthCounters != 0 {
			if stats, err := dev.PciGetStats(); err == nil {
				d.state.pciStats = &stats
			} else {
				d.logger.Warnw("Device PciGetStats() failed => no PCI BW metrics", zap.Error(err), "attributes", d.attributes)
			}
		} else {
			d.logger.Warnw("Device PciGetProperties(): no PCI BW", "attributes", d.attributes)
		}
	}

	// TODO: query device ext props:
	// - https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zes-device-ext-properties-t
	// Especially device props:
	// - https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zes-device-property-flags-t
	// (discrete/integrated, ECC support...)

	for _, s := range subsystems {
		d.scrapers = append(d.scrapers, s.enumDevice(d)...)
	}

	return d, nil
}

func (d *device) scrape(mb *metadata.MetricsBuilder, ts pcommon.Timestamp) {
	mb.RecordHwGpuInfoDataPoint(ts, 1,
		d.attributes.hwID,
		d.attributes.hwName,
		d.attributes.hwModel,
		d.attributes.hwSerialNumber,
		d.attributes.hwVendor,
		d.attributes.hwFirmwareVersion,
		d.attributes.pciBDF,
		d.attributes.pciDeviceID,
		d.attributes.pciVendorID,
		d.attributes.pciLanes,
		d.attributes.pciLinkGen,
	)

	d.scrapePciStats(mb, ts)

	for _, s := range d.scrapers {
		s.scrape(mb, ts)
	}
}

func (d *device) scrapePciStats(mb *metadata.MetricsBuilder, ts pcommon.Timestamp) {
	if d.state.pciStats == nil {
		return
	}

	stats, err := d.PciGetStats()
	if err != nil {
		d.logger.Errorw("Device PciGetStats() failed => no further PCI BW metrics", zap.Error(err), "attributes", d.attributes)
		d.state.pciStats = nil
		return
	}

	hwName := d.attributes.hwName + "-pci"

	// TODO: check from visualization that change rate for
	// PCI counters matches BW rate values calculated below
	// (if not, caller's timestamps do not match KMD timestamps)

	// read / write counters
	mb.RecordHwGpuIoDataPoint(
		ts, float64(stats.RxCounter),
		d.attributes.hwID,
		hwName,
		metadata.AttributeNetworkIoDirectionReceive,
	)
	mb.RecordHwGpuIoDataPoint(
		ts, float64(stats.TxCounter),
		d.attributes.hwID,
		hwName,
		metadata.AttributeNetworkIoDirectionTransmit,
	)

	// TODO: Sysman spec states neither timestamp nor counter bits,
	// so their values are assumed to wrap at full type width
	timeDiff := u64CounterDiff(stats.Timestamp, d.state.pciStats.Timestamp)
	rxDiff := u64CounterDiff(stats.RxCounter, d.state.pciStats.RxCounter)
	txDiff := u64CounterDiff(stats.TxCounter, d.state.pciStats.TxCounter)
	d.state.pciStats = &stats

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
		hwName,
	)

	if d.state.maxBandwidth > 0 {
		// TODO: verify that max is for read+write, not just one direction

		// max BW
		mb.RecordHwGpuBandwidthLimitDataPoint(
			ts, d.state.maxBandwidth,
			d.attributes.hwID,
			hwName,
		)

		// BW utilization ratio
		mb.RecordHwGpuBandwidthUtilizationDataPoint(
			ts, rate/float64(d.state.maxBandwidth),
			d.attributes.hwID,
			hwName,
		)
	}
}

func (d *device) pollAggregatedMetrics() {
	for _, s := range d.scrapers {
		s.pollAggregatedMetrics()
	}
}
