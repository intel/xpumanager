//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package common

import (
	"fmt"
	"slices"

	"go.opentelemetry.io/collector/pdata/pcommon"
)

// FilterOperator defines how the filter is matched against the attributes.
type FilterOperator string

const (
	// FilterOperatorIn matches when the attribute exists and its value is
	// one of the listed values.
	FilterOperatorIn FilterOperator = "In"
	// FilterOperatorNotIn matches when the attribute does not exist or its
	// value is none of the listed values.
	FilterOperatorNotIn FilterOperator = "NotIn"
	// TODO: add other operators like Exists, DoesNotExist (no values) and Gt,
	// Lt, GtE, LtE (single value) operators if needed.
)

// AttributeFilter defines filtering criteria for metric attributes.
type AttributeFilter struct {
	// Key is the attribute key to filter on.
	Key string `mapstructure:"key"`
	// Operator defines how Values are matched. If empty, "In" is used.
	Operator FilterOperator `mapstructure:"operator"`
	// Values is the set of attribute values to match against.
	Values []string `mapstructure:"values"`
}

// Validate checks if the filter is valid.
func (f *AttributeFilter) Validate() error {
	if f.Key == "" {
		return fmt.Errorf("filter key is required")
	}
	switch f.Operator {
	case "", FilterOperatorIn, FilterOperatorNotIn:
	default:
		return fmt.Errorf("invalid filter operator %q for key %q", f.Operator, f.Key)
	}
	if len(f.Values) == 0 {
		return fmt.Errorf("filter values are required for key %q", f.Key)
	}
	return nil
}

// Match reports whether attributes match the filter.
func (f *AttributeFilter) Match(attrs pcommon.Map) bool {
	val, ok := attrs.Get(f.Key)
	switch f.Operator {
	case "", FilterOperatorIn:
		return ok && slices.Contains(f.Values, val.AsString())
	case FilterOperatorNotIn:
		return !ok || !slices.Contains(f.Values, val.AsString())
	default:
		return false
	}
}

// AttributeFilterList is a list of AttributeFilter items.
type AttributeFilterList []AttributeFilter

// Match reports whether attrs satisfies all filters in the list. All filters
// must match for the list to match (AND semantics). An empty list matches
// unconditionally.
func (l AttributeFilterList) Match(attrs pcommon.Map) bool {
	for _, f := range l {
		if !f.Match(attrs) {
			return false
		}
	}
	return true
}
