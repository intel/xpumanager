//
// Copyright (C) 2025 Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0

package sysman

import (
	"log/slog"
	"strings"

	"go.opentelemetry.io/otel/attribute"
	"go.opentelemetry.io/otel/metric"

	l0sysman "github.com/intel/level-zero-go/sysman"
)

func init() {
	registerSubsystem("memory", newMemoryMetrics)
}

type sysmanMemory struct {
	*l0sysman.Mem
	attributes []attribute.KeyValue
}

type memoryMetrics struct {
	limit       metric.Int64ObservableGauge
	usage       metric.Int64ObservableGauge
	utilization metric.Float64ObservableGauge
	health      metric.Int64ObservableUpDownCounter
}

func newSysmanMemory(mem *l0sysman.Mem) (*sysmanMemory, error) {
	props, err := mem.GetProperties()
	if err != nil {
		return nil, err
	}
	attrs := []attribute.KeyValue{
		attribute.String("hw.gpu.memory.type", strings.ToLower(props.Type.String())),
		attribute.String("hw.gpu.memory.location", strings.ToLower(props.Location.String())),
		attribute.String("hw.gpu.memory.subdevice_id", subDeviceIdString(props.OnSubdevice, props.SubdeviceId)),
	}

	return &sysmanMemory{
		Mem:        mem,
		attributes: attrs,
	}, nil
}

func enumMemory(d *l0sysman.Device) []*sysmanMemory {
	mems, err := d.EnumMemoryModules()
	if err != nil {
		slog.Error("Failed to enumerate memory modules", "error", err)
		return nil
	}
	memory := make([]*sysmanMemory, len(mems))
	for i, mem := range mems {
		m, err := newSysmanMemory(mem)
		if err != nil {
			slog.Error("Failed to create sysman memory", "error", err)
			continue
		}
		memory[i] = m
	}
	return memory
}

func newMemoryMetrics(meter metric.Meter) (collector, error) {
	var err error
	m := &memoryMetrics{}

	m.limit, err = meter.Int64ObservableGauge(
		"hw.gpu.memory.limit",
		metric.WithDescription("Size of the GPU memory"),
		metric.WithUnit("By"),
	)
	if err != nil {
		return nil, err
	}
	m.usage, err = meter.Int64ObservableGauge(
		"hw.gpu.memory.usage",
		metric.WithDescription("GPU memory used"),
		metric.WithUnit("By"),
	)
	if err != nil {
		return nil, err
	}
	m.utilization, err = meter.Float64ObservableGauge(
		"hw.gpu.memory.utilization",
		metric.WithDescription("Fraction of GPU memory used"),
		metric.WithUnit("1"),
	)
	if err != nil {
		return nil, err
	}
	m.health, err = meter.Int64ObservableUpDownCounter(
		"hw.gpu.memory.health",
		metric.WithDescription("Health status of the GPU memory"),
	)
	if err != nil {
		return nil, err
	}

	return m, nil
}

func (m *memoryMetrics) getInstruments() []metric.Observable {
	return []metric.Observable{
		m.limit,
		m.usage,
		m.utilization,
		m.health,
	}
}

func (m *memoryMetrics) observeDevice(o metric.Observer, dev *sysmanDevice) {
	for _, mem := range dev.memory {
		attrs := append(dev.attributes, mem.attributes...)

		state, err := mem.GetState()
		if err != nil {
			slog.Error("Failed to get memory module state", "error", err, attrsToSlog(attrs))
			return
		}

		opt := metric.WithAttributes(attrs...)
		o.ObserveInt64(m.limit, int64(state.Size), opt)

		usage := int64(state.Size - state.Free)
		o.ObserveInt64(m.usage, usage, opt)

		o.ObserveFloat64(m.utilization, float64(usage)/float64(state.Size), opt)

		opt = metric.WithAttributes(append(attrs, attribute.String("hw.gpu.memory.health_status", strings.ToLower(state.Health.String())))...)
		o.ObserveInt64(m.health, 1, opt)
	}
}
