//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package processor

import (
	"testing"

	"go.opentelemetry.io/collector/pdata/pcommon"

	"github.com/intel/xpumanager/xpumd/common"
)

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
