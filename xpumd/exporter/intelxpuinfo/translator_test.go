//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package intelxpuinfo

import (
	"testing"

	"github.com/google/go-cmp/cmp"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"go.opentelemetry.io/collector/pdata/pmetric"
	"go.uber.org/zap"
	"google.golang.org/protobuf/testing/protocmp"

	"github.com/intel/xpumanager/xpumd/common"
	pb "github.com/intel/xpumanager/xpumd/exporter/intelxpuinfo/api/deviceinfo/v1alpha1"
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
			translator := newMetricsTranslator(zap.NewNop().Sugar(), nil)

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

func TestTranslate(t *testing.T) {
	md := pmetric.NewMetrics()
	metrics := md.ResourceMetrics().AppendEmpty().ScopeMetrics().AppendEmpty().Metrics()

	info := metrics.AppendEmpty()
	info.SetName("hw.gpu.info")
	infoDPs := info.SetEmptyGauge().DataPoints()

	infoDP := infoDPs.AppendEmpty()
	infoDP.SetIntValue(1)
	infoDP.Attributes().PutStr("hw.id", "gpu0")
	infoDP.Attributes().PutStr("hw.model", "ACME 1000")
	infoDP.Attributes().PutStr("pci.bdf", "0000:3a:00.0")
	infoDP.Attributes().PutStr("pci.device_id", "0x0bd5")
	infoDP.Attributes().PutStr("pci.vendor_id", "0x8086")
	infoDP.Attributes().PutStr("hw.firmware_version", "0:gfx::1.2.3")

	// A data point without hw.id cannot be attributed to a device.
	orphanDP := infoDPs.AppendEmpty()
	orphanDP.SetIntValue(1)
	orphanDP.Attributes().PutStr("hw.model", "no identifier")

	// Two memory modules, appended in reverse order to exercise the sorting.
	mem := metrics.AppendEmpty()
	mem.SetName("hw.memory.size")
	memDPs := mem.SetEmptySum().DataPoints()
	for i, subdeviceID := range []string{"1", "0"} {
		dp := memDPs.AppendEmpty()
		dp.SetIntValue(1000 + int64(i))
		dp.Attributes().PutStr("hw.parent", "gpu0")
		dp.Attributes().PutStr("hw.memory.location", "device")
		dp.Attributes().PutStr("hw.memory.type", "hbm")
		dp.Attributes().PutStr("subdevice_id", subdeviceID)
	}

	// Metric types other than gauge and sum are skipped (but don't cause panic/errors)
	histogram := metrics.AppendEmpty()
	histogram.SetName("hw.gpu.info")
	histogram.SetEmptyHistogram().DataPoints().AppendEmpty()

	translator := newMetricsTranslator(zap.NewNop().Sugar(), nil)

	resp := translator.translate(md)

	require.Len(t, resp.Devices, 1)
	got := resp.Devices[0].Info

	want := &pb.DeviceInformation{
		Uuid:  "gpu0",
		Model: "ACME 1000",
		Pci: &pb.PciInfo{
			Bdf:      "0000:3a:00.0",
			DeviceId: "0x0bd5",
			VendorId: "0x8086",
		},
		Firmwares: []*pb.FirmwareInfo{
			{Name: "gfx", Version: "1.2.3"},
		},
		Memory: []*pb.MemoryInfo{
			{Type: "hbm", SubdeviceId: "0", Size: 1001},
			{Type: "hbm", SubdeviceId: "1", Size: 1000},
		},
	}

	if diff := cmp.Diff(want, got, protocmp.Transform()); diff != "" {
		t.Errorf("unexpected device info (-want +got):\n%s", diff)
	}
}

func TestTranslateWithoutDeviceMetadata(t *testing.T) {
	translator := newMetricsTranslator(zap.NewNop().Sugar(), nil)
	translator.health["gpu0"] = healthStatuses{
		{domain: "gpu.health"}: {Name: "gpu.health"},
	}
	translator.memory["gpu0"] = []*pb.MemoryInfo{{Type: "hbm", Size: 8e9}}

	resp := translator.translate(pmetric.NewMetrics())

	assert.Empty(t, resp.Devices)
}

func TestUpdateHealthStatus(t *testing.T) {
	addStatusDP := func(dps pmetric.NumberDataPointSlice, hwParent, hwState string, active bool) {
		dp := dps.AppendEmpty()
		if active {
			dp.SetIntValue(1)
		} else {
			dp.SetIntValue(0)
		}
		dp.Attributes().PutStr("hw.parent", hwParent)
		dp.Attributes().PutStr("hw.state", hwState)
	}

	makeStateMapping := func(state, severity string) HwStateMapping {
		return HwStateMapping{
			Severity:      severity,
			severityLevel: severityNames[severity],
		}
	}

	makeStatusMapping := func(domain string, showAllStates bool, stateMapping map[string]HwStateMapping) HwStatusMapping {
		return HwStatusMapping{
			HealthDomain:     domain,
			healthDomainTmpl: mustParseTemplate(domain),
			ShowAllStates:    showAllStates,
			StateMapping:     stateMapping,
		}
	}

	tests := []struct {
		name           string
		statusMappings []HwStatusMapping
		setup          func(pmetric.NumberDataPointSlice)
		wantStatuses   map[string]healthStatuses
	}{
		{
			name: "zero data points",
			statusMappings: []HwStatusMapping{
				makeStatusMapping("gpu.health", false, map[string]HwStateMapping{
					"degraded": makeStateMapping("degraded", "warning"),
				}),
			},
			setup:        func(dps pmetric.NumberDataPointSlice) {},
			wantStatuses: nil,
		},
		{
			name: "no mapping matches hw.state",
			statusMappings: []HwStatusMapping{
				makeStatusMapping("gpu.health", false, map[string]HwStateMapping{
					"degraded": makeStateMapping("degraded", "warning"),
				}),
			},
			setup: func(dps pmetric.NumberDataPointSlice) {
				addStatusDP(dps, "gpu0", "unknown_state", true)
			},
			wantStatuses: map[string]healthStatuses{
				"gpu0": {},
			},
		},
		{
			name: "multiple active states collapsed to worst severity",
			statusMappings: []HwStatusMapping{
				makeStatusMapping("gpu.health", false, map[string]HwStateMapping{
					"degraded": makeStateMapping("degraded", "warning"),
					"failed":   makeStateMapping("failed", "critical"),
				}),
			},
			setup: func(dps pmetric.NumberDataPointSlice) {
				addStatusDP(dps, "gpu0", "degraded", true)
				addStatusDP(dps, "gpu0", "failed", true)
				addStatusDP(dps, "gpu0", "degraded", false) // inactive, should be skipped
			},
			wantStatuses: map[string]healthStatuses{
				"gpu0": {
					{domain: "gpu.health"}: {
						Name:     "gpu.health",
						Severity: pb.SeverityLevel_SEVERITY_LEVEL_CRITICAL,
						Reason:   "failed"},
				},
			},
		},
		{
			name: "inactive states are ignored",
			statusMappings: []HwStatusMapping{
				makeStatusMapping("gpu.health", false, map[string]HwStateMapping{
					"degraded": makeStateMapping("degraded", "warning"),
					"failed":   makeStateMapping("failed", "critical"),
				}),
			},
			setup: func(dps pmetric.NumberDataPointSlice) {
				addStatusDP(dps, "gpu0", "degraded", true)
				addStatusDP(dps, "gpu0", "failed", false) // inactive, should be skipped
			},
			wantStatuses: map[string]healthStatuses{
				"gpu0": {
					{domain: "gpu.health"}: {
						Name:     "gpu.health",
						Severity: pb.SeverityLevel_SEVERITY_LEVEL_WARNING,
						Reason:   "degraded"},
				},
			},
		},
		{
			name: "show_all_states: multiple active states",
			statusMappings: []HwStatusMapping{
				makeStatusMapping("gpu.health", true, map[string]HwStateMapping{
					"degraded": makeStateMapping("degraded", "warning"),
					"failed":   makeStateMapping("failed", "critical"),
				}),
			},
			setup: func(dps pmetric.NumberDataPointSlice) {
				addStatusDP(dps, "gpu0", "degraded", true)
				addStatusDP(dps, "gpu0", "failed", true)
			},
			wantStatuses: map[string]healthStatuses{
				"gpu0": {
					{domain: "gpu.health", state: "degraded"}: {
						Name:     "gpu.health",
						Severity: pb.SeverityLevel_SEVERITY_LEVEL_WARNING,
						Reason:   "degraded"},
					{domain: "gpu.health", state: "failed"}: {
						Name:     "gpu.health",
						Severity: pb.SeverityLevel_SEVERITY_LEVEL_CRITICAL,
						Reason:   "failed"},
				},
			},
		},
		{
			name: "show_all_states: single active state",
			statusMappings: []HwStatusMapping{
				makeStatusMapping("gpu.health", true, map[string]HwStateMapping{
					"degraded": makeStateMapping("degraded", "warning"),
					"failed":   makeStateMapping("failed", "critical"),
				}),
			},
			setup: func(dps pmetric.NumberDataPointSlice) {
				addStatusDP(dps, "gpu0", "failed", true)
			},
			wantStatuses: map[string]healthStatuses{
				"gpu0": {
					{domain: "gpu.health", state: "failed"}: {
						Name:     "gpu.health",
						Severity: pb.SeverityLevel_SEVERITY_LEVEL_CRITICAL,
						Reason:   "failed"},
				},
			},
		},
		{
			name: "non-integer data point value",
			statusMappings: []HwStatusMapping{
				makeStatusMapping("gpu.health", false, map[string]HwStateMapping{
					"degraded": makeStateMapping("degraded", "warning"),
				}),
			},
			setup: func(dps pmetric.NumberDataPointSlice) {
				dp := dps.AppendEmpty()
				dp.SetDoubleValue(1)
				dp.Attributes().PutStr("hw.parent", "gpu0")
				dp.Attributes().PutStr("hw.state", "degraded")
			},
			wantStatuses: nil,
		},
		{
			name: "missing hw.parent and hw.id",
			statusMappings: []HwStatusMapping{
				makeStatusMapping("gpu.health", false, map[string]HwStateMapping{
					"degraded": makeStateMapping("degraded", "warning"),
				}),
			},
			setup: func(dps pmetric.NumberDataPointSlice) {
				addStatusDP(dps, "", "degraded", true)
			},
			wantStatuses: nil,
		},
		{
			name: "no mapping matches the attributes",
			statusMappings: []HwStatusMapping{
				{
					Filters:          common.AttributeFilterList{{Key: "hw.type", Values: []string{"cpu"}}},
					HealthDomain:     "cpu.health",
					healthDomainTmpl: mustParseTemplate("cpu.health"),
					StateMapping: map[string]HwStateMapping{
						"degraded": makeStateMapping("degraded", "warning"),
					},
				},
			},
			setup: func(dps pmetric.NumberDataPointSlice) {
				addStatusDP(dps, "gpu0", "degraded", true)
			},
			wantStatuses: map[string]healthStatuses{
				"gpu0": {},
			},
		},
		{
			// The template refers to a non-existent attribute, so it renders empty and the status is skipped
			name: "health domain cannot be determined",
			statusMappings: []HwStatusMapping{
				makeStatusMapping("{{.hw_type}}", false, map[string]HwStateMapping{
					"degraded": makeStateMapping("degraded", "warning"),
				}),
			},
			setup: func(dps pmetric.NumberDataPointSlice) {
				addStatusDP(dps, "gpu0", "degraded", true)
			},
			wantStatuses: map[string]healthStatuses{
				"gpu0": {},
			},
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			translator := newMetricsTranslator(zap.NewNop().Sugar(), nil)
			translator.mappings = tt.statusMappings

			dps := pmetric.NewNumberDataPointSlice()
			tt.setup(dps)

			translator.updateHealthStatus("hw.status", dps)

			require.Len(t, translator.health, len(tt.wantStatuses))
			for devID, wantDevStatuses := range tt.wantStatuses {
				gotDevStatuses := translator.health[devID]
				require.Len(t, gotDevStatuses, len(wantDevStatuses), "unexpected number of health entries for device %q", devID)
				for key, wantStatus := range wantDevStatuses {
					gotStatus := gotDevStatuses[key]
					require.NotNil(t, gotStatus, "expected health status for key %q not found", key)
					if diff := cmp.Diff(wantStatus, gotStatus, protocmp.Transform()); diff != "" {
						t.Errorf("unexpected health status for %q (-want +got):\n%s", key, diff)
					}
				}
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
		{
			name: "data point without a value should be skipped",
			setup: func(dps pmetric.NumberDataPointSlice) {
				addDataPoint(dps, "gpu0", "hbm", "device", "0", 10)
				// No value set at all, i.e. an "empty" value type.
				dp := dps.AppendEmpty()
				dp.Attributes().PutStr("hw.parent", "gpu0")
				dp.Attributes().PutStr("hw.memory.type", "hbm")
				dp.Attributes().PutStr("hw.memory.location", "device")
			},
			expected: map[string][]*pb.MemoryInfo{
				"gpu0": {
					{Type: "hbm", SubdeviceId: "0", Size: 10},
				},
			},
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			translator := newMetricsTranslator(zap.NewNop().Sugar(), nil)

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
