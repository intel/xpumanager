//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package exporter

import (
	"testing"

	"github.com/google/go-cmp/cmp"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"go.opentelemetry.io/collector/pdata/pmetric"
	"go.uber.org/zap"
	"google.golang.org/protobuf/testing/protocmp"

	pb "github.com/intel/xpumanager/xpumd/exporter/api/deviceinfo/v1alpha1"
)

func TestParseFirmwares(t *testing.T) {
	tests := []struct {
		name     string
		input    string
		expected []*pb.FirmwareInfo
	}{
		{
			name:  "single firmware entry",
			input: "0:foo::1.2.3",
			expected: []*pb.FirmwareInfo{
				{Name: "foo", SubdeviceId: "", Version: "1.2.3"},
			},
		},
		{
			name:  "multiple firmware entries",
			input: "0:foo::1.2.3,1:bar:1:4.5.6,2:baz:2:7.8.9",
			expected: []*pb.FirmwareInfo{
				{Name: "foo", SubdeviceId: "", Version: "1.2.3"},
				{Name: "bar", SubdeviceId: "1", Version: "4.5.6"},
				{Name: "baz", SubdeviceId: "2", Version: "7.8.9"},
			},
		},
		{
			name:     "empty string",
			input:    "",
			expected: nil,
		},
		{
			name:     "invalid format - missing fields",
			input:    "0:foo",
			expected: nil,
		},
		{
			name:  "mixed valid and invalid entries",
			input: "0:foo:0:1.2.3,invalid,2:bar:2:4.5.6",
			expected: []*pb.FirmwareInfo{
				{Name: "foo", SubdeviceId: "0", Version: "1.2.3"},
				{Name: "bar", SubdeviceId: "2", Version: "4.5.6"},
			},
		},
		{
			name:  "version with colons",
			input: "0:foo:0:1.2.3:extra:data",
			expected: []*pb.FirmwareInfo{
				{Name: "foo", SubdeviceId: "0", Version: "1.2.3:extra:data"},
			},
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			translator := newMetricsTranslator(zap.NewNop().Sugar())

			result := translator.parseFirmwares(tt.input)

			require.Len(t, result, len(tt.expected))
			for i, expected := range tt.expected {
				assert.Equal(t, expected.Name, result[i].Name)
				assert.Equal(t, expected.SubdeviceId, result[i].SubdeviceId)
				assert.Equal(t, expected.Version, result[i].Version)
			}
		})
	}
}

func TestUpdateMemoryInfo(t *testing.T) {
	addDataPoint := func(dps pmetric.NumberDataPointSlice, hwParent, memType, memLocation, subdeviceID string, size int64) {
		dp := dps.AppendEmpty()
		dp.SetIntValue(size)
		dp.Attributes().PutStr("hw.parent", hwParent)
		dp.Attributes().PutStr("hw.memory.type", memType)
		dp.Attributes().PutStr("hw.memory.location", memLocation)
		if subdeviceID != "" {
			dp.Attributes().PutStr("subdevice_id", subdeviceID)
		}
	}

	tests := []struct {
		name     string
		setup    func(pmetric.NumberDataPointSlice)
		expected map[string][]*pb.MemoryInfo
	}{
		{
			name: "single memory module",
			setup: func(dps pmetric.NumberDataPointSlice) {
				addDataPoint(dps, "gpu0", "hbm", "device", "", 8e9)
			},
			expected: map[string][]*pb.MemoryInfo{
				"gpu0": {
					{Type: "hbm", SubdeviceId: "", Size: 8e9},
				},
			},
		},
		{
			name: "multiple memory modules same type",
			setup: func(dps pmetric.NumberDataPointSlice) {
				addDataPoint(dps, "gpu0", "hbm", "device", "0", 4e9)
				addDataPoint(dps, "gpu0", "hbm", "device", "1", 4e9)
			},
			expected: map[string][]*pb.MemoryInfo{
				"gpu0": {
					{Type: "hbm", SubdeviceId: "0", Size: 4e9},
					{Type: "hbm", SubdeviceId: "1", Size: 4e9},
				},
			},
		},
		{
			name: "aggregation by type and subdevice",
			setup: func(dps pmetric.NumberDataPointSlice) {
				addDataPoint(dps, "gpu0", "hbm", "device", "0", 2e9)
				addDataPoint(dps, "gpu0", "hbm", "device", "0", 2e9)
			},
			expected: map[string][]*pb.MemoryInfo{
				"gpu0": {
					{Type: "hbm", SubdeviceId: "0", Size: 4e9},
				},
			},
		},
		{
			name: "filter non-device memory",
			setup: func(dps pmetric.NumberDataPointSlice) {
				addDataPoint(dps, "gpu0", "hbm", "device", "", 8e9)
				addDataPoint(dps, "gpu0", "ddr", "host", "", 16e9)
			},
			expected: map[string][]*pb.MemoryInfo{
				"gpu0": {
					{Type: "hbm", SubdeviceId: "", Size: 8e9},
				},
			},
		},
		{
			name: "multiple memory types",
			setup: func(dps pmetric.NumberDataPointSlice) {
				addDataPoint(dps, "gpu0", "hbm", "device", "", 8e9)
				addDataPoint(dps, "gpu0", "sram", "device", "", 1e6)
			},
			expected: map[string][]*pb.MemoryInfo{
				"gpu0": {
					{Type: "hbm", SubdeviceId: "", Size: 8e9},
					{Type: "sram", SubdeviceId: "", Size: 1e6},
				},
			},
		},
		{
			name: "multiple devices",
			setup: func(dps pmetric.NumberDataPointSlice) {
				addDataPoint(dps, "gpu0", "hbm", "device", "0", 4e9)
				addDataPoint(dps, "gpu0", "hbm", "device", "0", 4e9)
				addDataPoint(dps, "gpu1", "hbm", "device", "", 16e9)
			},
			expected: map[string][]*pb.MemoryInfo{
				"gpu0": {
					{Type: "hbm", SubdeviceId: "0", Size: 8e9},
				},
				"gpu1": {
					{Type: "hbm", SubdeviceId: "", Size: 16e9},
				},
			},
		},
		{
			name: "missing hw.parent attribute",
			setup: func(dps pmetric.NumberDataPointSlice) {
				// Valid metric
				addDataPoint(dps, "gpu0", "hbm", "device", "", 8e9)
				// Missing hw.parent attribute
				addDataPoint(dps, "", "hbm", "device", "", 4e9)
			},
			expected: map[string][]*pb.MemoryInfo{
				"gpu0": {
					{Type: "hbm", SubdeviceId: "", Size: 8e9},
				},
			},
		},
		{
			name: "double value type",
			setup: func(dps pmetric.NumberDataPointSlice) {
				// Add double value
				dp := dps.AppendEmpty()
				dp.SetDoubleValue(8e9)
				dp.Attributes().PutStr("hw.parent", "gpu0")
				dp.Attributes().PutStr("hw.memory.type", "hbm")
				dp.Attributes().PutStr("hw.memory.location", "device")
				// Mix with int value
				addDataPoint(dps, "gpu0", "hbm", "device", "1", 4e9)
			},
			expected: map[string][]*pb.MemoryInfo{
				"gpu0": {
					{Type: "hbm", SubdeviceId: "", Size: 8e9},
					{Type: "hbm", SubdeviceId: "1", Size: 4e9},
				},
			},
		},
		{
			name: "negative size should be skipped",
			setup: func(dps pmetric.NumberDataPointSlice) {
				addDataPoint(dps, "gpu0", "hbm", "device", "0", 10)
				addDataPoint(dps, "gpu0", "hbm", "device", "1", -1)
				addDataPoint(dps, "gpu0", "sram", "device", "1", 20)
			},
			expected: map[string][]*pb.MemoryInfo{
				"gpu0": {
					{Type: "hbm", SubdeviceId: "0", Size: 10},
					{Type: "sram", SubdeviceId: "1", Size: 20},
				},
			},
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			translator := newMetricsTranslator(zap.NewNop().Sugar())

			dps := pmetric.NewNumberDataPointSlice()
			tt.setup(dps)

			translator.updateMemoryInfo("hw.memory.size", dps)

			require.Equal(t, len(tt.expected), len(translator.memory))
			for deviceID, expectedMemList := range tt.expected {
				actualMemList, exists := translator.memory[deviceID]
				require.True(t, exists, "device %s not found in memory map", deviceID)
				require.Len(t, actualMemList, len(expectedMemList))

				for i, expected := range expectedMemList {
					actual := actualMemList[i]
					if diff := cmp.Diff(expected, actual, protocmp.Transform()); diff != "" {
						t.Errorf("memory info mismatch at index %d (-want +got):\n%s", i, diff)
					}
				}
			}
		})
	}
}
