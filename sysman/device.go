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
	logger                      *zap.SugaredLogger
	attributes                  map[string]string
	aggregatedMetricsBufferSize int

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

	attrs := map[string]string{
		attrHwID:           props.Core.Uuid.Id.String(),
		attrHwModel:        props.ModelName.String(),
		attrHwName:         name,
		attrHwSerialNumber: props.SerialNumber.String(),
		attrHwVendor:       props.VendorName.String(),
	}

	d := &sysmanDevice{
		Device:                      device,
		logger:                      logger,
		attributes:                  attrs,
		aggregatedMetricsBufferSize: aggregatedMetricsBufferSize,
	}

	d.enumFrequency()
	d.enumMemory()
	d.enumTemperature()

	return d, nil
}

func (d *sysmanDevice) enumFrequency() {
	freqs, err := d.EnumFrequencyDomains()
	if err != nil {
		d.logger.Errorw("Failed to enumerate frequency domains", zap.Error(err))
		d.frequency = nil
		return
	}
	frequency := make([]*sysmanFrequency, len(freqs))
	for i, freq := range freqs {
		name := fmt.Sprintf("freq_%d", i)
		f, err := newSysmanFrequency(name, freq, d)
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
		name := fmt.Sprintf("mem_%d", i)
		m, err := newSysmanMemory(name, mem, d)
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
		name := fmt.Sprintf("temp_%d", i)
		t, err := newSysmanTemperature(name, temp, d)
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

func (d *sysmanDevice) pickAttributes(keys ...string) pcommon.Map {
	return pickAttributes(d.attributes, keys...)
}
