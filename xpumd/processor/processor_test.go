//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package processor

import (
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"go.opentelemetry.io/collector/pdata/pcommon"
	"go.opentelemetry.io/collector/pdata/pmetric"
	"go.uber.org/zap"

	"github.com/intel/xpumanager/xpumd/common"
)

func TestRuleProcessorEvaluatestates(t *testing.T) {
	tests := []struct {
		name           string
		states         []StateRule
		value          float64
		parentID       string
		parentAttrs    map[string]any
		expectedStates map[string]uint64
	}{
		{
			name: "single state with matching condition",
			states: []StateRule{
				{
					StateName: "ok",
					Conditions: []ConditionRule{
						{Value: 50.0},
					},
				},
			},
			value:    75.0,
			parentID: "parent1",
			expectedStates: map[string]uint64{
				"ok": 1,
			},
		},
		{
			name: "single state with non-matching condition",
			states: []StateRule{
				{
					StateName: "ok",
					Conditions: []ConditionRule{
						{Value: 50.0},
					},
				},
			},
			value:    25.0,
			parentID: "parent1",
			expectedStates: map[string]uint64{
				"ok": 0,
			},
		},
		{
			name: "multiple states - first matches",
			states: []StateRule{
				{
					StateName: "ok",
					Conditions: []ConditionRule{
						{Value: 30.0},
					},
				},
				{
					StateName: "warning",
					Conditions: []ConditionRule{
						{Value: 70.0},
					},
				},
			},
			value:    50.0,
			parentID: "parent1",
			expectedStates: map[string]uint64{
				"ok":      1,
				"warning": 0,
			},
		},
		{
			name: "multiple states - last matches",
			states: []StateRule{
				{
					StateName: "ok",
					Conditions: []ConditionRule{
						{Value: 30.0},
					},
				},
				{
					StateName: "warning",
					Conditions: []ConditionRule{
						{Value: 50.0},
					},
				},
			},
			value:    75.0,
			parentID: "parent1",
			expectedStates: map[string]uint64{
				"ok":      0,
				"warning": 1,
			},
		},
		{
			name: "multiple states - none match",
			states: []StateRule{
				{
					StateName: "ok",
					Conditions: []ConditionRule{
						{Value: 80.0},
					},
				},
				{
					StateName: "warning",
					Conditions: []ConditionRule{
						{Value: 90.0},
					},
				},
			},
			value:    50.0,
			parentID: "parent1",
			expectedStates: map[string]uint64{
				"ok":      0,
				"warning": 0,
			},
		},
		{
			name: "unconditional state (last)",
			states: []StateRule{
				{
					StateName: "ok",
					Conditions: []ConditionRule{
						{Value: 50.0},
					},
				},
				{
					StateName: "default",
				},
			},
			value:    75.0,
			parentID: "parent1",
			expectedStates: map[string]uint64{
				"ok":      0,
				"default": 1,
			},
		},
		{
			name: "multiple conditions, one matches",
			states: []StateRule{
				{
					StateName: "degraded",
					Conditions: []ConditionRule{
						{Value: 90.0},
						{Value: 60.0},
					},
				},
			},
			value:    70.0,
			parentID: "parent1",
			expectedStates: map[string]uint64{
				"degraded": 1,
			},
		},
		{
			name: "state with parent filter - filter matches",
			states: []StateRule{
				{
					StateName: "ok",
					Conditions: []ConditionRule{
						{
							Value: 50.0,
							ParentFilters: common.AttributeFilterList{
								{
									Key:    "hw.type",
									Values: []string{"gpu"},
								},
							},
						},
					},
				},
			},
			value:    75.0,
			parentID: "parent1",
			parentAttrs: map[string]any{
				"hw.type": "gpu",
			},
			expectedStates: map[string]uint64{
				"ok": 1,
			},
		},
		{
			name: "state with parent filter - filter does not match",
			states: []StateRule{
				{
					StateName: "not-ok",
				},
				{
					StateName: "ok",
					Conditions: []ConditionRule{
						{
							Value: 50.0,
							ParentFilters: common.AttributeFilterList{
								{
									Key:    "hw.type",
									Values: []string{"gpu"},
								},
							},
						},
					},
				},
			},
			value:    75.0,
			parentID: "parent1",
			parentAttrs: map[string]any{
				"hw.type": "cpu",
			},
			expectedStates: map[string]uint64{
				"not-ok": 1,
				"ok":     0,
			},
		},
		{
			name: "complex scenario - multiple states with increasing severity",
			states: []StateRule{
				{
					StateName: "ok",
					Conditions: []ConditionRule{
						{Value: 0.0},
					},
				},
				{
					StateName: "warning",
					Conditions: []ConditionRule{
						{Value: 50.0},
					},
				},
				{
					StateName: "critical",
					Conditions: []ConditionRule{
						{Value: 80.0},
					},
				},
			},
			value:    85.0,
			parentID: "parent1",
			expectedStates: map[string]uint64{
				"ok":       0,
				"warning":  0,
				"critical": 1,
			},
		},
		{
			name:           "empty states slice",
			states:         []StateRule{},
			value:          50.0,
			parentID:       "parent1",
			expectedStates: map[string]uint64{},
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			parentAttrs := pcommon.NewMap()
			_ = parentAttrs.FromRaw(tt.parentAttrs)
			rp := &ruleProcessor{
				HealthRule: &HealthRule{
					States: tt.states,
				},
				logger:      zap.NewNop().Sugar(),
				parentAttrs: map[string]pcommon.Map{tt.parentID: parentAttrs},
			}

			states := rp.evaluateStates(tt.value, tt.parentID)

			assert.Equal(t, tt.expectedStates, states)
		})
	}
}

func TestRuleProcessorUpdateMetrics(t *testing.T) {
	type dataPoint struct {
		value      float64
		attributes map[string]any
	}

	tests := []struct {
		name             string
		rule             HealthRule
		sourceMetric     string
		sourceDataPoints []dataPoint
		parentAttributes map[string]map[string]any
		expectMetric     string
		expectDataPoints []dataPoint
	}{
		{
			name: "status metric with single state",
			rule: HealthRule{
				SourceMetric:   "gpu.temperature",
				StatusMetric:   "hw.status",
				StateAttribute: "hw.state",
				CopyAttributes: []string{"hw.id", "hw.type"},
				AddAttributes: map[string]string{
					"vendor.name": "acme",
					"location":    "datacenter1",
				},
				States: []StateRule{
					{
						StateName: "ok",
					},
				},
			},
			sourceMetric: "gpu.temperature",
			sourceDataPoints: []dataPoint{
				{
					value: 50.0,
					attributes: map[string]any{
						"hw.id":   "gpu0",
						"hw.name": "GPU 0",
						"hw.type": "gpu",
					},
				},
			},
			expectMetric: "hw.status",
			expectDataPoints: []dataPoint{
				{
					value: 1,
					attributes: map[string]any{
						"hw.id":       "gpu0",
						"hw.type":     "gpu",
						"hw.state":    "ok",
						"vendor.name": "acme",
						"location":    "datacenter1",
					},
				},
			},
		},
		{
			name: "source metric not found, no status metric",
			rule: HealthRule{
				SourceMetric:   "gpu.temperature",
				StatusMetric:   "hw.status",
				StateAttribute: "hw.state",
				States: []StateRule{
					{
						StateName: "ok",
					},
				},
			},
			sourceMetric: "cpu.temperature",
			sourceDataPoints: []dataPoint{
				{
					value: 50.0,
					attributes: map[string]any{
						"hw.id": "cpu0",
					},
				},
			},
			expectMetric: "",
		},
		{
			name: "component and parent filters match",
			rule: HealthRule{
				SourceMetric:       "gpu.temperature",
				StatusMetric:       "hw.status",
				StateAttribute:     "hw.state",
				ParentMetric:       "system.info",
				ParentRefAttribute: "hw.parent",
				ComponentFilters: common.AttributeFilterList{
					{
						Key:    "hw.type",
						Values: []string{"gpu"},
					},
				},
				ParentFilters: common.AttributeFilterList{
					{
						Key:    "hw.vendor",
						Values: []string{"acme"},
					},
				},
				States: []StateRule{
					{
						StateName: "ok",
					},
				},
			},
			sourceMetric: "gpu.temperature",
			sourceDataPoints: []dataPoint{
				{
					value: 50.0,
					attributes: map[string]any{
						"hw.id":     "gpu0",
						"hw.type":   "gpu",
						"hw.parent": "system1",
					},
				},
			},
			parentAttributes: map[string]map[string]any{
				"system1": {
					"hw.id":     "system1",
					"hw.vendor": "acme",
				},
			},
			expectMetric: "hw.status",
			expectDataPoints: []dataPoint{
				{
					value: 1,
					attributes: map[string]any{
						"hw.state": "ok",
					},
				},
			},
		},
		{
			name: "component filter does not match",
			rule: HealthRule{
				SourceMetric:   "gpu.temperature",
				StatusMetric:   "hw.status",
				StateAttribute: "hw.state",
				ComponentFilters: common.AttributeFilterList{
					{
						Key:    "hw.type",
						Values: []string{"cpu"},
					},
				},
				States: []StateRule{
					{
						StateName: "ok",
					},
				},
			},
			sourceMetric: "gpu.temperature",
			sourceDataPoints: []dataPoint{
				{
					value: 50.0,
					attributes: map[string]any{
						"hw.id":   "gpu0",
						"hw.type": "gpu",
					},
				},
			},
			expectMetric:     "hw.status",
			expectDataPoints: nil,
		},
		{
			name: "parent filter does not match",
			rule: HealthRule{
				SourceMetric:       "gpu.temperature",
				StatusMetric:       "hw.status",
				StateAttribute:     "hw.state",
				ParentMetric:       "system.info",
				ParentRefAttribute: "hw.parent",
				ParentFilters: common.AttributeFilterList{
					{
						Key:    "hw.type",
						Values: []string{"gpu"},
					},
				},
				States: []StateRule{
					{
						StateName: "ok",
					},
				},
			},
			sourceMetric: "gpu.temperature",
			sourceDataPoints: []dataPoint{
				{
					value: 50.0,
					attributes: map[string]any{
						"hw.type":   "gpu",
						"hw.parent": "system1",
					},
				},
			},
			parentAttributes: map[string]map[string]any{
				"system1": {
					"hw.id":   "system1",
					"hw.type": "cpu",
				},
			},
			expectMetric:     "hw.status",
			expectDataPoints: nil,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			tt.rule.setDefaults()

			// Initialize source data
			parentAttrsMap := make(map[string]pcommon.Map)
			for parentID, attrs := range tt.parentAttributes {
				parentAttrs := pcommon.NewMap()
				_ = parentAttrs.FromRaw(attrs)
				parentAttrsMap[parentID] = parentAttrs
			}

			rp := &ruleProcessor{
				HealthRule:  &tt.rule,
				logger:      zap.NewNop().Sugar(),
				parentAttrs: parentAttrsMap,
			}

			sm := pmetric.NewScopeMetrics()
			sourceMetric := sm.Metrics().AppendEmpty()
			sourceMetric.SetName(tt.sourceMetric)
			sourceMetric.SetEmptyGauge()
			sourceDps := sourceMetric.Gauge().DataPoints()

			for _, sdp := range tt.sourceDataPoints {
				dp := sourceDps.AppendEmpty()
				dp.SetDoubleValue(sdp.value)
				_ = dp.Attributes().FromRaw(sdp.attributes)
			}

			// Call updateMetrics
			rp.updateMetrics(sm)

			// Verify status metric
			if tt.expectMetric == "" {
				// Should not create status metric
				_, found := findMetricByName(sm, rp.StatusMetric)
				assert.False(t, found)
				return
			}

			statusMetric, found := findMetricByName(sm, tt.expectMetric)
			require.True(t, found)

			statusDps, ok := getNumberDatapoints(statusMetric)
			require.True(t, ok)

			require.Equal(t, len(tt.expectDataPoints), statusDps.Len())
			for i, expectedDp := range tt.expectDataPoints {
				dp := statusDps.At(i)

				assert.Equal(t, expectedDp.value, float64(dp.IntValue()))

				attrs := dp.Attributes()
				for k, v := range expectedDp.attributes {
					val, ok := attrs.Get(k)
					assert.True(t, ok)
					assert.Equal(t, v, val.Str())
				}
			}
		})
	}
}
