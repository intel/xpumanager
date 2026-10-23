//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package intelxpustatus

import (
	"fmt"
	"math"
	"slices"

	"go.opentelemetry.io/collector/component"
	"go.opentelemetry.io/collector/pdata/pcommon"

	"github.com/intel/xpumanager/xpumd/common"
)

// Config defines configuration for the processor.
type Config struct {
	Rules []HealthRule `mapstructure:"rules"`
}

// HealthRule defines a health state rule.
type HealthRule struct {
	// Name of the rule
	Name string `mapstructure:"name"`
	// Source metric to evaluate the state rules against.
	SourceMetric string `mapstructure:"source_metric"`
	// Parent metric to get the parent attributes from. If empty, no parent
	// filtering is applied.
	ParentMetric string `mapstructure:"parent_metric"`
	// Metric to set the status on. If empty, "hw.status" is used.
	StatusMetric string `mapstructure:"status_metric"`
	// Attribute of the SourceMetric that identifies the parent of the
	// hardware component. If empty, "hw.parent" is used
	ParentRefAttribute string `mapstructure:"parent_ref_attribute"`
	// Attribute of the parent metric that uniquely identifies the parent and
	// to which the ParentRefAttribute points to. If empty, "hw.id" is used.
	ParentIDAttribute string `mapstructure:"parent_id_attribute"`
	// Attribute of the StatusMetric to set the state on. If empty, "hw.state" is used.
	StateAttribute string `mapstructure:"state_attribute"`
	// List of attributes to copy from SourceMetric to StatusMetric.
	CopyAttributes []string `mapstructure:"copy_attributes"`
	// Additional attributes to set on the StatusMetric.
	AddAttributes map[string]string `mapstructure:"add_attributes"`
	// Filters to apply on the source metric.
	ComponentFilters common.AttributeFilterList `mapstructure:"component_filters"`
	// Filters to apply on the parent metric.
	ParentFilters common.AttributeFilterList `mapstructure:"parent_filters"`
	// Aggregate the (filtered) source data points before evaluating the
	// states. If not set, each data point is evaluated separately.
	Aggregate *AggregateRule `mapstructure:"aggregate"`
	// Ordered list of state rules to evaluate. All rules are evaluated, and
	// the last matching one will be active. Thus, the rules should be ordered
	// by increasing severity.
	States []StateRule `mapstructure:"states"`
}

// AggregateFunction is a function for aggregating data point values.
type AggregateFunction string

const (
	AggregateFunctionSum AggregateFunction = "Sum"
	AggregateFunctionMin AggregateFunction = "Min"
	AggregateFunctionMax AggregateFunction = "Max"
)

// AggregateRule defines how source data points are aggregated.
type AggregateRule struct {
	// Function to apply to the data points.
	Function AggregateFunction `mapstructure:"function"`
	// Source metric attributes to group the data points by. Data points are
	// also implicitly grouped by their parent (ParentRefAttribute).
	GroupBy []string `mapstructure:"group_by"`
}

// StateRule defines when to apply a specific state.
type StateRule struct {
	// Conditions to evaluate on whether to apply this state.
	// If condition includes a matching [ConditionRule.ParentFilters] filter,
	// no further conditions are considered.
	// Missing filter matches always i.e. acts as a default.
	// Conditions are evaluated in order, so default should be the last one.
	Conditions []ConditionRule `mapstructure:"conditions"`
	// Value to set to the state attribute (HealthRule.StateAttribute) of the
	// generated metric (HealthRule.StatusMetric).
	StateName string `mapstructure:"state_name"`
}

// ConditionRule defines conditions to check whether a state should be applied.
type ConditionRule struct {
	// Value threshold
	Value float64 `mapstructure:"value"`
	// Filter to apply on parent attributes.
	ParentFilters common.AttributeFilterList `mapstructure:"parent_filters"`
}

// defaultConfig returns the default configuration for the processor.
func defaultConfig() component.Config {
	return &Config{}
}

// setDefaults fills in defaults for empty configuration fields.
func (c *Config) setDefaults() {
	for i := range c.Rules {
		c.Rules[i].setDefaults()
	}
}

func (r *HealthRule) setDefaults() {
	if r.StatusMetric == "" {
		r.StatusMetric = "hw.status"
	}
	if r.ParentRefAttribute == "" {
		r.ParentRefAttribute = "hw.parent"
	}
	if r.ParentIDAttribute == "" {
		r.ParentIDAttribute = "hw.id"
	}
	if r.StateAttribute == "" {
		r.StateAttribute = "hw.state"
	}
}

// Validate checks if the configuration is valid.
// NOTE: Does not validate the nested fields implementing Validate() (the
// attribute filters).  The method is expected to be called by the
// otel-collector validator (confmap.Validate()) which recursively validates
// the nested types.
func (r *HealthRule) Validate() error {
	if r.Name == "" {
		return fmt.Errorf("rule name cannot be empty")
	}
	if r.SourceMetric == "" {
		return fmt.Errorf("source_metric is required")
	}
	if len(r.States) == 0 {
		return fmt.Errorf("at least one state is required")
	}
	if r.ParentMetric == "" {
		if len(r.ParentFilters) > 0 {
			return fmt.Errorf("parent_filters requires parent_metric")
		}
		for _, state := range r.States {
			for _, cond := range state.Conditions {
				if len(cond.ParentFilters) > 0 {
					return fmt.Errorf("condition parent_filters of state %q requires parent_metric", state.StateName)
				}
			}
		}
	}
	return nil
}

// Validate checks if the configuration is valid.
func (a *AggregateRule) Validate() error {
	if slices.Contains(a.GroupBy, "") {
		return fmt.Errorf("group_by attribute cannot be empty")
	}
	switch a.Function {
	case AggregateFunctionSum, AggregateFunctionMin, AggregateFunctionMax:
		return nil
	case "":
		return fmt.Errorf("aggregate function is required")
	default:
		return fmt.Errorf("invalid aggregate function %q", a.Function)
	}
}

func (a *AggregateRule) apply(acc, value float64) float64 {
	switch a.Function {
	case AggregateFunctionSum:
		return acc + value
	case AggregateFunctionMin:
		return math.Min(acc, value)
	case AggregateFunctionMax:
		return math.Max(acc, value)
	default:
		return math.NaN()
	}
}

// Validate checks if the configuration is valid.
func (t *StateRule) Validate() error {
	if t.StateName == "" {
		return fmt.Errorf("state_name cannot be empty")
	}
	hasDefault := false
	for _, cond := range t.Conditions {
		if hasDefault {
			return fmt.Errorf("default condition not last one")
		}
		if len(cond.ParentFilters) == 0 {
			hasDefault = true
		}
	}
	return nil
}

// Validate checks if the configuration is valid.
func (r *ConditionRule) Validate() error {
	if math.IsNaN(r.Value) {
		return fmt.Errorf("value cannot be NaN")
	}
	return nil
}

func (r *ConditionRule) matchValue(value float64) bool {
	return value >= r.Value
}

func (r *ConditionRule) matchParent(parentAttrs pcommon.Map) bool {
	return r.ParentFilters.Match(parentAttrs)
}
