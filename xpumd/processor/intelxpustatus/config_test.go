//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package intelxpustatus

import (
	"math"
	"testing"

	"github.com/stretchr/testify/assert"
	"go.opentelemetry.io/collector/confmap"
	"go.opentelemetry.io/collector/pdata/pcommon"

	"github.com/intel/xpumanager/xpumd/common"
)

func TestConfigValidate(t *testing.T) {
	tests := []struct {
		name      string
		rule      HealthRule
		expectErr string
	}{
		{
			name: "valid rule",
			rule: HealthRule{
				Name:         "gpu-health",
				SourceMetric: "hw.temperature",
				States:       []StateRule{{StateName: "warning", Conditions: []ConditionRule{{Value: 85}}}},
			},
		},
		{
			name: "empty rule name",
			rule: HealthRule{
				SourceMetric: "hw.temperature",
				States:       []StateRule{{StateName: "warning", Conditions: []ConditionRule{{Value: 85}}}},
			},
			expectErr: "rule name cannot be empty",
		},
		{
			name: "missing source metric",
			rule: HealthRule{
				Name:   "gpu-health",
				States: []StateRule{{StateName: "warning", Conditions: []ConditionRule{{Value: 85}}}},
			},
			expectErr: "source_metric is required",
		},
		{
			name: "no states",
			rule: HealthRule{
				Name:         "gpu-health",
				SourceMetric: "hw.temperature",
			},
			expectErr: "at least one state is required",
		},
		{
			name: "empty state name",
			rule: HealthRule{
				Name:         "gpu-health",
				SourceMetric: "hw.temperature",
				States:       []StateRule{{Conditions: []ConditionRule{{Value: 85}}}},
			},
			expectErr: "state_name cannot be empty",
		},
		{
			name: "NaN condition value",
			rule: HealthRule{
				Name:         "gpu-health",
				SourceMetric: "hw.temperature",
				States:       []StateRule{{StateName: "warning", Conditions: []ConditionRule{{Value: math.NaN()}}}},
			},
			expectErr: "value cannot be NaN",
		},
		// Basic tests to check that the recursive validation applies to the attribute filters as well
		{
			name: "invalid component filter",
			rule: HealthRule{
				Name:             "gpu-health",
				SourceMetric:     "hw.temperature",
				States:           []StateRule{{StateName: "warning", Conditions: []ConditionRule{{Value: 85}}}},
				ComponentFilters: common.AttributeFilterList{{Key: ""}},
			},
			expectErr: "filter key is required",
		},
		{
			name: "invalid parent filter",
			rule: HealthRule{
				Name:          "gpu-health",
				SourceMetric:  "hw.temperature",
				States:        []StateRule{{StateName: "warning", Conditions: []ConditionRule{{Value: 85}}}},
				ParentFilters: common.AttributeFilterList{{Key: "hw.type", Values: []string{""}}},
			},
			expectErr: `filter value cannot be empty for key "hw.type"`,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			cfg := &Config{Rules: []HealthRule{tt.rule}}

			// Validate through confmap as the collector does (recursive validation nested types)
			err := confmap.Validate(cfg)

			if tt.expectErr == "" {
				assert.NoError(t, err)
			} else {
				assert.ErrorContains(t, err, tt.expectErr)
			}
		})
	}
}

func TestConditionRule(t *testing.T) {
	tests := []struct {
		name        string
		rule        ConditionRule
		value       float64
		parentAttrs map[string]string
		expectMatch bool
	}{
		{
			name: "value matches, no parent filters",
			rule: ConditionRule{
				Value: 50.0,
			},
			value:       75.0,
			expectMatch: true,
		},
		{
			name: "value matches on threshold",
			rule: ConditionRule{
				Value: 50.0,
			},
			value:       50.0,
			expectMatch: true,
		},
		{
			name: "value matches, negative values",
			rule: ConditionRule{
				Value: -50.0,
			},
			value:       -25.0,
			expectMatch: true,
		},
		{
			name: "no match, value below threshold",
			rule: ConditionRule{
				Value: 50.0,
			},
			value:       25.0,
			expectMatch: false,
		},
		{
			name: "value matches, parent filter matches",
			rule: ConditionRule{
				Value: 50.0,
				ParentFilters: common.AttributeFilterList{
					{
						Key:    "hw.type",
						Values: []string{"gpu"},
					},
				},
			},
			value: 75.0,
			parentAttrs: map[string]string{
				"hw.type": "gpu",
			},
			expectMatch: true,
		},
		{
			name: "value matches, parent filter does not match",
			rule: ConditionRule{
				Value: 50.0,
				ParentFilters: common.AttributeFilterList{
					{
						Key:    "hw.type",
						Values: []string{"gpu"},
					},
				},
			},
			value: 75.0,
			parentAttrs: map[string]string{
				"hw.type": "cpu",
			},
			expectMatch: false,
		},
		{
			name: "value below threshold and parent filter matches",
			rule: ConditionRule{
				Value: 50.0,
				ParentFilters: common.AttributeFilterList{
					{
						Key:    "hw.type",
						Values: []string{"gpu"},
					},
				},
			},
			value: 25.0,
			parentAttrs: map[string]string{
				"hw.type": "gpu",
			},
			expectMatch: false,
		},
		{
			name: "value matches, multiple parent filters all match",
			rule: ConditionRule{
				Value: 50.0,
				ParentFilters: common.AttributeFilterList{
					{
						Key:    "hw.type",
						Values: []string{"gpu"},
					},
					{
						Key:    "vendor",
						Values: []string{"acme", "contoso"},
					},
				},
			},
			value: 75.0,
			parentAttrs: map[string]string{
				"hw.type": "gpu",
				"vendor":  "acme",
			},
			expectMatch: true,
		},
		{
			name: "value matches, one parent filter does not match",
			rule: ConditionRule{
				Value: 50.0,
				ParentFilters: common.AttributeFilterList{
					{
						Key:    "hw.type",
						Values: []string{"gpu"},
					},
					{
						Key:    "vendor",
						Values: []string{"acme"},
					},
				},
			},
			value: 75.0,
			parentAttrs: map[string]string{
				"hw.type": "gpu",
				"vendor":  "contoso",
			},
			expectMatch: false,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			parentAttrs := pcommon.NewMap()
			for k, v := range tt.parentAttrs {
				parentAttrs.PutStr(k, v)
			}
			got := tt.rule.match(tt.value, parentAttrs)
			if got != tt.expectMatch {
				t.Errorf("ConditionRule.match() = %v, want %v", got, tt.expectMatch)
			}
		})
	}
}
