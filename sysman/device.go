//
// Copyright (C) 2025 Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0

package sysman

import (
	"fmt"

	"github.com/intel/level-zero-go/levelzero"
	"go.opentelemetry.io/otel/attribute"
	"go.opentelemetry.io/otel/metric"
)

type deviceRegistry struct {
	devices []*sysmanDevice
}

type sysmanDevice struct {
	*levelzero.ZeDevice
	attributes []attribute.KeyValue

	frequency   []*sysmanFrequency
	memory      []*sysmanMemory
	temperature []*sysmanTemperature
}

func newDeviceRegistry() (*deviceRegistry, error) {
	reg := &deviceRegistry{}

	drivers, err := levelzero.ZesDriverGet()
	if err != nil {
		return nil, fmt.Errorf("failed to get ZES drivers: %w", err)
	}
	for _, driver := range drivers {
		devs, err := enumDevices(driver)
		if err != nil {
			return nil, err
		}

		reg.devices = append(reg.devices, devs...)
	}

	return reg, nil
}

func enumDevices(driver *levelzero.ZeDriver) ([]*sysmanDevice, error) {
	zesDevs, err := driver.ZesDeviceGet()
	if err != nil {
		return nil, fmt.Errorf("failed to get ZES devices: %w", err)
	}
	devs := make([]*sysmanDevice, len(zesDevs))
	for i, d := range zesDevs {
		dev, err := newSysmanDevice(d)
		if err != nil {
			return nil, err
		}
		devs[i] = dev
	}
	return devs, nil
}

func newSysmanDevice(device *levelzero.ZeDevice) (*sysmanDevice, error) {
	props, err := device.GetProperties()
	if err != nil {
		return nil, err
	}
	attrs := []attribute.KeyValue{
		attribute.String("hw.id", props.Core.Uuid.String()),
		attribute.String("hw.model", props.ModelName),
		attribute.String("hw.serial_number", props.SerialNumber),
		attribute.String("hw.vendor", props.VendorName),
	}

	d := &sysmanDevice{
		ZeDevice:   device,
		attributes: attrs,
	}

	d.enumFrequency()
	d.enumMemory()
	d.enumTemperature()

	return d, nil
}

func (d *sysmanDevice) enumFrequency() {
	d.frequency = enumFrequency(d.ZeDevice)
}

func (d *sysmanDevice) enumMemory() {
	d.memory = enumMemory(d.ZeDevice)
}

func (d *sysmanDevice) enumTemperature() {
	d.temperature = enumTemperature(d.ZeDevice)
}

func (d *sysmanDevice) observe(o metric.Observer, registry *metricsRegistry) {
	for _, freq := range d.frequency {
		registry.frequency.observe(o, freq, d.attributes)
	}
	for _, mem := range d.memory {
		registry.memory.observe(o, mem, d.attributes)
	}
	for _, temp := range d.temperature {
		registry.temperature.observe(o, temp, d.attributes)
	}
}
