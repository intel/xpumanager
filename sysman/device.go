//
// Copyright (C) 2025 Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0

package sysman

import (
	"fmt"

	"go.opentelemetry.io/collector/pdata/pcommon"
	"go.uber.org/zap"

	l0sysman "github.com/intel/level-zero-go/sysman"
)

type deviceRegistry struct {
	devices []*sysmanDevice
}

type sysmanDevice struct {
	*l0sysman.Device
	logger     *zap.SugaredLogger
	attributes pcommon.Map

	frequency   []*sysmanFrequency
	memory      []*sysmanMemory
	temperature []*sysmanTemperature
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
		dev, err := newSysmanDevice(d, logger, aggregatedMetricsBufferSize)
		if err != nil {
			return nil, err
		}
		devs[i] = dev
	}
	return devs, nil
}

func newSysmanDevice(device *l0sysman.Device, logger *zap.SugaredLogger, aggregatedMetricsBufferSize int) (*sysmanDevice, error) {
	props, err := device.GetProperties()
	if err != nil {
		return nil, err
	}
	attrs := pcommon.NewMap()
	attrs.PutStr("hw.id", props.Core.Uuid.Id.String())
	attrs.PutStr("hw.model", props.ModelName.String())
	attrs.PutStr("hw.serial_number", props.SerialNumber.String())
	attrs.PutStr("hw.vendor", props.VendorName.String())

	d := &sysmanDevice{
		Device:     device,
		logger:     logger,
		attributes: attrs,
	}

	d.enumFrequency(aggregatedMetricsBufferSize)
	d.enumMemory()
	d.enumTemperature()

	return d, nil
}

func (d *sysmanDevice) enumFrequency(aggregatedMetricsBufferSize int) {
	freqs, err := d.EnumFrequencyDomains()
	if err != nil {
		d.logger.Errorw("Failed to enumerate frequency domains", zap.Error(err))
		d.frequency = nil
		return
	}
	frequency := make([]*sysmanFrequency, len(freqs))
	for i, freq := range freqs {
		f, err := newSysmanFrequency(freq, d.logger, d.attributes, aggregatedMetricsBufferSize)
		if err != nil {
			d.logger.Errorw("Failed to create sysman frequency domain", zap.Error(err))
			continue
		}
		frequency[i] = f
	}
	d.frequency = frequency
}

func (d *sysmanDevice) enumMemory() {
	mems, err := d.EnumMemoryModules()
	if err != nil {
		d.logger.Errorw("Failed to enumerate memory modules", zap.Error(err))
		d.memory = nil
		return
	}
	memory := make([]*sysmanMemory, len(mems))
	for i, mem := range mems {
		m, err := newSysmanMemory(mem, d.logger, d.attributes)
		if err != nil {
			d.logger.Errorw("Failed to create sysman memory module", zap.Error(err))
			continue
		}
		memory[i] = m
	}
	d.memory = memory
}

func (d *sysmanDevice) enumTemperature() {
	temps, err := d.EnumTemperatureSensors()
	if err != nil {
		d.logger.Errorw("Failed to enumerate temperature sensors", zap.Error(err))
		d.temperature = nil
		return
	}
	temperature := make([]*sysmanTemperature, len(temps))
	for i, temp := range temps {
		t, err := newSysmanTemperature(temp, d.logger, d.attributes)
		if err != nil {
			d.logger.Errorw("Failed to create sysman temperature sensor", zap.Error(err))
			continue
		}
		temperature[i] = t
	}
	d.temperature = temperature
}

func (d *sysmanDevice) pollAggregatedMetrics() {
	for _, f := range d.frequency {
		f.pollAggregatedMetrics()
	}
}
