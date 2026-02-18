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
	devices []*sysmanDevice
}

type sysmanDevice struct {
	*l0sysman.Device
	logger                      *zap.SugaredLogger
	attributes                  deviceAttributes
	aggregatedMetricsBufferSize int
	scrapers                    []instanceScraper
}

type deviceAttributes struct {
	hwID              string
	hwModel           string
	hwName            string
	hwSerialNumber    string
	hwVendor          string
	hwFirmwareVersion string
	pciBDF            string
	pciDeviceID       string
	pciVendorID       string
}

func newDeviceRegistry(logger *zap.SugaredLogger, aggregatedMetricsBufferSize int) (*deviceRegistry, error) {
	reg := &deviceRegistry{}

	drivers, err := l0sysman.DriverGet()
	if err != nil {
		return nil, fmt.Errorf("failed to get ZES drivers: %w", err)
	}
	for _, driver := range drivers {
		devs, err := enumDevices(driver, logger, aggregatedMetricsBufferSize)
		if err != nil {
			return nil, err
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

func enumDevices(driver *l0sysman.Driver, logger *zap.SugaredLogger, aggregatedMetricsBufferSize int) ([]*sysmanDevice, error) {
	zesDevs, err := driver.DeviceGet()
	if err != nil {
		return nil, fmt.Errorf("failed to get ZES devices: %w", err)
	}
	devs := make([]*sysmanDevice, len(zesDevs))
	for i, d := range zesDevs {
		name := fmt.Sprintf("gpu_%d", i)
		dev, err := newSysmanDevice(name, d, logger, aggregatedMetricsBufferSize)
		if err != nil {
			return nil, err
		}

		devs[i] = dev
	}
	return devs, nil
}

func newSysmanDevice(name string, device *l0sysman.Device, logger *zap.SugaredLogger, aggregatedMetricsBufferSize int) (*sysmanDevice, error) {
	props, err := device.GetProperties()
	if err != nil {
		return nil, err
	}

	d := &sysmanDevice{
		Device: device,
		logger: logger,
		attributes: deviceAttributes{
			hwID:           props.Core.Uuid.Id.String(),
			hwModel:        props.ModelName.String(),
			hwName:         name,
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

	if pci, err := device.PciGetProperties(); err != nil {
		logger.Errorw("failed to get PCI properties", "error", err, "deviceAttributes", d.attributes)
	} else {
		d.attributes.pciBDF = fmt.Sprintf("%04x:%02x:%02x.%x", pci.Address.Domain, pci.Address.Bus, pci.Address.Device, pci.Address.Function)
	}

	for _, s := range subsystems {
		d.scrapers = append(d.scrapers, s.enumDevice(d)...)
	}

	return d, nil
}

func (d *sysmanDevice) scrape(mb *metadata.MetricsBuilder, ts pcommon.Timestamp) {
	mb.RecordHwGpuInfoDataPoint(ts, 1,
		d.attributes.hwID,
		d.attributes.hwModel,
		d.attributes.hwName,
		d.attributes.hwSerialNumber,
		d.attributes.hwVendor,
		d.attributes.hwFirmwareVersion,
		d.attributes.pciBDF,
		d.attributes.pciDeviceID,
		d.attributes.pciVendorID,
	)

	for _, s := range d.scrapers {
		s.scrape(mb, ts)
	}
}

func (d *sysmanDevice) pollAggregatedMetrics() {
	for _, s := range d.scrapers {
		s.pollAggregatedMetrics()
	}
}
