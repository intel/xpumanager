//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package common

import (
	"testing"

	"go.opentelemetry.io/collector/pdata/pcommon"
)

func TestAttributeFilter(t *testing.T) {
	tests := []struct {
		name        string
		filter      AttributeFilter
		attrs       map[string]string
		expectMatch bool
	}{
		{
			name: "exact match single value",
			filter: AttributeFilter{
				Key:    "hw.type",
				Values: []string{"gpu"},
			},
			attrs: map[string]string{
				"hw.type": "gpu",
			},
			expectMatch: true,
		},
		{
			name: "exact match multiple values - first",
			filter: AttributeFilter{
				Key:    "hw.type",
				Values: []string{"gpu", "cpu", "memory"},
			},
			attrs: map[string]string{
				"hw.type": "cpu",
			},
			expectMatch: true,
		},
		{
			name: "no match - wrong value",
			filter: AttributeFilter{
				Key:    "hw.type",
				Values: []string{"gpu"},
			},
			attrs: map[string]string{
				"hw.type": "cpu",
			},
			expectMatch: false,
		},
		{
			name: "no match - key not present",
			filter: AttributeFilter{
				Key:    "hw.type",
				Values: []string{"gpu"},
			},
			attrs: map[string]string{
				"other.key": "value",
			},
			expectMatch: false,
		},
		{
			name: "no match - wrong case",
			filter: AttributeFilter{
				Key:    "hw.type",
				Values: []string{"GPU"},
			},
			attrs: map[string]string{
				"hw.type": "gpu",
			},
			expectMatch: false,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			attrs := pcommon.NewMap()
			for k, v := range tt.attrs {
				attrs.PutStr(k, v)
			}
			got := tt.filter.Match(attrs)
			if got != tt.expectMatch {
				t.Errorf("AttributeFilter.Match() = %v, want %v", got, tt.expectMatch)
			}
		})
	}
}
