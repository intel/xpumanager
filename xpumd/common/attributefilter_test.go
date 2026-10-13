//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package common

import (
	"testing"

	"github.com/stretchr/testify/assert"
	"go.opentelemetry.io/collector/confmap"
	"go.opentelemetry.io/collector/pdata/pcommon"
)

func TestAttributeFilter(t *testing.T) {
	tests := []struct {
		name        string
		filter      AttributeFilter
		attrs       map[string]any
		expectMatch bool
	}{
		{
			name: "exact match single value",
			filter: AttributeFilter{
				Key:    "hw.type",
				Values: []string{"gpu"},
			},
			attrs: map[string]any{
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
			attrs: map[string]any{
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
			attrs: map[string]any{
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
			attrs: map[string]any{
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
			attrs: map[string]any{
				"hw.type": "gpu",
			},
			expectMatch: false,
		},
		{
			name: "match, multiple attrs",
			filter: AttributeFilter{
				Key:    "hw.vendor",
				Values: []string{"acme"},
			},
			attrs: map[string]any{
				"hw.type":            "gpu",
				"hw.vendor":          "acme",
				"hw.sensor_location": "memory",
			},
			expectMatch: true,
		},
		{
			name: "no match, multiple attrs",
			filter: AttributeFilter{
				Key:    "hw.vendor",
				Values: []string{"memory"}, // another attr has the wanted value, but on the wrong key
			},
			attrs: map[string]any{
				"hw.type":            "cpu",
				"hw.vendor":          "acme",
				"hw.sensor_location": "memory",
			},
			expectMatch: false,
		},
		{
			name: "empty value",
			filter: AttributeFilter{
				Key:    "com.intel.subdevice_id",
				Values: []string{""},
			},
			attrs:       map[string]any{"com.intel.subdevice_id": ""},
			expectMatch: true,
		},
		{
			name: "empty value, key not present",
			filter: AttributeFilter{
				Key:    "com.intel.subdevice_id",
				Values: []string{""},
			},
			attrs:       map[string]any{"hw.type": "gpu"},
			expectMatch: false,
		},
		{
			name: "explicit In operator",
			filter: AttributeFilter{
				Key:      "hw.type",
				Operator: FilterOperatorIn,
				Values:   []string{"gpu"},
			},
			attrs:       map[string]any{"hw.type": "gpu"},
			expectMatch: true,
		},
		{
			name: "NotIn, match",
			filter: AttributeFilter{
				Key:      "hw.type",
				Operator: FilterOperatorNotIn,
				Values:   []string{"cpu", "memory"},
			},
			attrs:       map[string]any{"hw.type": "gpu"},
			expectMatch: true,
		},
		{
			name: "NotIn, no match",
			filter: AttributeFilter{
				Key:      "hw.type",
				Operator: FilterOperatorNotIn,
				Values:   []string{"cpu", "gpu"},
			},
			attrs:       map[string]any{"hw.type": "gpu"},
			expectMatch: false,
		},
		{
			name: "NotIn, key not present",
			filter: AttributeFilter{
				Key:      "hw.type",
				Operator: FilterOperatorNotIn,
				Values:   []string{"gpu"},
			},
			attrs:       map[string]any{"other.key": "value"},
			expectMatch: true,
		},
		{
			name: "int attribute matching value as string",
			filter: AttributeFilter{
				Key:    "com.intel.subdevice_count",
				Values: []string{"2"},
			},
			attrs:       map[string]any{"com.intel.subdevice_count": 2},
			expectMatch: true,
		},
		{
			name: "unknown operator never matches",
			filter: AttributeFilter{
				Key:      "hw.type",
				Operator: "Foo",
				Values:   []string{"gpu"},
			},
			attrs:       map[string]any{"hw.type": "gpu"},
			expectMatch: false,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			attrs := pcommon.NewMap()
			assert.NoError(t, attrs.FromRaw(tt.attrs))
			got := tt.filter.Match(attrs)
			if got != tt.expectMatch {
				t.Errorf("AttributeFilter.Match() = %v, want %v", got, tt.expectMatch)
			}
		})
	}
}

func TestAttributeFilterList_Match(t *testing.T) {
	tests := []struct {
		name        string
		list        AttributeFilterList
		attrs       map[string]string
		expectMatch bool
	}{
		{
			name:        "empty list matches unconditionally",
			list:        AttributeFilterList{},
			attrs:       map[string]string{"hw.type": "gpu"},
			expectMatch: true,
		},
		{
			name: "all filters match",
			list: AttributeFilterList{
				{
					Key:    "hw.type",
					Values: []string{"memory"},
				},
				{
					Key:    "hw.vendor",
					Values: []string{"acme"},
				},
			},
			attrs:       map[string]string{"hw.type": "memory", "hw.vendor": "acme"},
			expectMatch: true,
		},
		{
			name: "one filter does not match",
			list: AttributeFilterList{
				{
					Key:    "hw.type",
					Values: []string{"memory"},
				},
				{
					Key:    "hw.vendor",
					Values: []string{"amd"},
				},
			},
			attrs:       map[string]string{"hw.type": "memory", "hw.vendor": "acme"},
			expectMatch: false,
		},
		{
			name: "all filters match among many attrs",
			list: AttributeFilterList{
				{Key: "hw.type", Values: []string{"gpu"}},
				{Key: "hw.vendor", Values: []string{"acme"}},
			},
			attrs: map[string]string{
				"hw.type":            "gpu",
				"hw.vendor":          "acme",
				"hw.sensor_location": "memory",
				"hw.state":           "ok",
			},
			expectMatch: true,
		},
		{
			name: "one filter does not match among many attrs",
			list: AttributeFilterList{
				{Key: "hw.type", Values: []string{"gpu"}},
				{Key: "hw.vendor", Values: []string{"good"}},
			},
			attrs: map[string]string{
				"hw.type":   "gpu",
				"hw.vendor": "acme",
				"hw.state":  "good",
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
			got := tt.list.Match(attrs)
			if got != tt.expectMatch {
				t.Errorf("AttributeFilterList.Match() = %v, want %v", got, tt.expectMatch)
			}
		})
	}
}

func TestAttributeFilterList_Validate(t *testing.T) {
	tests := []struct {
		name    string
		filters AttributeFilterList
		wantErr string
	}{
		{
			name:    "single filter, valid",
			filters: AttributeFilterList{{Key: "hw.type", Values: []string{"gpu"}}},
		},
		{
			name:    "empty key",
			filters: AttributeFilterList{{Key: "", Values: []string{"gpu"}}},
			wantErr: "filter key is required",
		},
		{
			name:    "empty value",
			filters: AttributeFilterList{{Key: "hw.type", Values: []string{""}}},
		},
		{
			name:    "no values",
			filters: AttributeFilterList{{Key: "hw.type"}},
			wantErr: `filter values are required for key "hw.type"`,
		},
		{
			name:    "NotIn operator",
			filters: AttributeFilterList{{Key: "hw.type", Operator: FilterOperatorNotIn, Values: []string{"gpu"}}},
		},
		{
			name:    "invalid operator",
			filters: AttributeFilterList{{Key: "hw.type", Operator: "Exists", Values: []string{"gpu"}}},
			wantErr: `invalid filter operator "Exists" for key "hw.type"`,
		},
		{
			name: "multiple filters, all valid",
			filters: AttributeFilterList{
				{Key: "hw.type", Values: []string{"gpu"}},
				{Key: "hw.vendor", Values: []string{"acme"}},
			},
		},
		{
			name: "multiple filters, one invalid",
			filters: AttributeFilterList{
				{Key: "hw.type", Values: []string{"gpu"}},
				{Key: "", Values: []string{"acme"}},
			},
			wantErr: "filter key is required",
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			err := confmap.Validate(&tt.filters)

			if tt.wantErr == "" {
				assert.NoError(t, err)
			} else {
				assert.ErrorContains(t, err, tt.wantErr)
			}
		})
	}
}
