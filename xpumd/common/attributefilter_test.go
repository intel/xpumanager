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
		{
			name: "match, multiple attrs",
			filter: AttributeFilter{
				Key:    "hw.vendor",
				Values: []string{"acme"},
			},
			attrs: map[string]string{
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
			attrs: map[string]string{
				"hw.type":            "cpu",
				"hw.vendor":          "acme",
				"hw.sensor_location": "memory",
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
		wantErr bool
	}{
		{
			name:    "single filter, valid",
			filters: AttributeFilterList{{Key: "hw.type", Values: []string{"gpu"}}},
			wantErr: false,
		},
		{
			name:    "empty key",
			filters: AttributeFilterList{{Key: "", Values: []string{"gpu"}}},
			wantErr: true,
		},
		{
			name: "multiple filters, all valid",
			filters: AttributeFilterList{
				{Key: "hw.type", Values: []string{"gpu"}},
				{Key: "hw.vendor", Values: []string{"acme"}},
			},
			wantErr: false,
		},
		{
			name: "multiple filters, one invalid",
			filters: AttributeFilterList{
				{Key: "hw.type", Values: []string{"gpu"}},
				{Key: "", Values: []string{"acme"}},
			},
			wantErr: true,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			err := tt.filters.Validate()
			if (err != nil) != tt.wantErr {
				t.Errorf("AttributeFilterList.Validate() error = %v, wantErr %v", err, tt.wantErr)
			}
		})
	}
}
