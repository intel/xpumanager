//
// Copyright (C) 2025 Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0

package sysman

import (
	"fmt"

	"go.opentelemetry.io/otel/attribute"
	"go.opentelemetry.io/otel/metric"

	l0sysman "github.com/intel/level-zero-go/sysman"
)

type deviceRegistry struct {
	devices []*sysmanDevice
}

type sysmanDevice struct {
	*l0sysman.Device
	attributes []attribute.KeyValue

	frequency   []*sysmanFrequency
	memory      []*sysmanMemory
	temperature []*sysmanTemperature
}

func newDeviceRegistry() (*deviceRegistry, error) {
	reg := &deviceRegistry{}

	drivers, err := l0sysman.DriverGet()
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

func enumDevices(driver *l0sysman.Driver) ([]*sysmanDevice, error) {
	zesDevs, err := driver.DeviceGet()
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

func newSysmanDevice(device *l0sysman.Device) (*sysmanDevice, error) {
	props, err := device.GetProperties()
	if err != nil {
		return nil, err
	}
	attrs := []attribute.KeyValue{
		attribute.String("hw.id", props.Core.Uuid.Id.String()),
		attribute.String("hw.model", props.ModelName.String()),
		attribute.String("hw.serial_number", props.SerialNumber.String()),
		attribute.String("hw.vendor", props.VendorName.String()),
	}

	d := &sysmanDevice{
		Device:     device,
		attributes: attrs,
	}

	d.enumFrequency()
	d.enumMemory()
	d.enumTemperature()

	return d, nil
}

func (d *sysmanDevice) enumFrequency() {
	d.frequency = enumFrequency(d.Device)
}

func (d *sysmanDevice) enumMemory() {
	d.memory = enumMemory(d.Device)
}

func (d *sysmanDevice) enumTemperature() {
	d.temperature = enumTemperature(d.Device)
}

func (d *sysmanDevice) observe(o metric.Observer, registry *metricsRegistry) {
	for _, c := range registry.collectors {
		c.observeDevice(o, d)
	}
}
