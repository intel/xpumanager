//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package intelxpustatus

import (
	"fmt"
	"math"

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
	// Ordered list of state rules to evaluate. All rules are evaluated, and
	// the last matching one will be active. Thus, the rules should be ordered
	// by increasing severity.
	States []StateRule `mapstructure:"states"`
}

// StateRule defines when to apply a specific state.
type StateRule struct {
	// Conditions to evaluate to apply this state. A logical OR is applied i.e.
	// the rule matches if any of the conditions match.
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

// Validate checks if the processor configuration is valid.
func (c *Config) Validate() error {
	for _, rule := range c.Rules {
		if err := rule.validate(); err != nil {
			return fmt.Errorf("invalid health rule %q: %w", rule.Name, err)
		}
	}
	return nil
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

func (r *HealthRule) validate() error {
	if r.Name == "" {
		return fmt.Errorf("rule name cannot be empty")
	}
	if r.SourceMetric == "" {
		return fmt.Errorf("source_metric is required")
	}
	if len(r.States) == 0 {
		return fmt.Errorf("at least one state is required")
	}
	for _, t := range r.States {
		if err := t.validate(); err != nil {
			return fmt.Errorf("invalid state rule: %w", err)
		}
	}
	if err := r.ComponentFilters.Validate(); err != nil {
		return fmt.Errorf("invalid component filter: %w", err)
	}
	if err := r.ParentFilters.Validate(); err != nil {
		return fmt.Errorf("invalid parent filter: %w", err)
	}
	return nil
}

func (t *StateRule) validate() error {
	if t.StateName == "" {
		return fmt.Errorf("state_name cannot be empty")
	}
	for _, rule := range t.Conditions {
		if err := rule.validate(); err != nil {
			return fmt.Errorf("invalid condition rule: %w", err)
		}
	}
	return nil
}

func (r *ConditionRule) validate() error {
	if math.IsNaN(r.Value) {
		return fmt.Errorf("value cannot be NaN")
	}
	if err := r.ParentFilters.Validate(); err != nil {
		return fmt.Errorf("invalid parent filter: %w", err)
	}
	return nil
}

func (r *ConditionRule) match(value float64, parentAttrs pcommon.Map) bool {
	return value >= r.Value && r.ParentFilters.Match(parentAttrs)
}
