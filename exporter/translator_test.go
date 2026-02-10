//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0

package exporter

import (
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"go.uber.org/zap"

	pb "github.com/intel/xpumanager/exporter/api/deviceinfo/v1alpha1"
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
