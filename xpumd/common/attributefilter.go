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

// AttributeFilter defines filtering criteria for metric attributes.
type AttributeFilter struct {
	// Key is the attribute key to filter on.
	Key string `mapstructure:"key"`
	// Values is the set of attribute values to match. A match occurs when the
	// attribute value equals any of the listed values.
	Values []string `mapstructure:"values"`
}

// Validate checks if the filter is valid.
func (f *AttributeFilter) Validate() error {
	if f.Key == "" {
		return fmt.Errorf("filter key is required")
	}
	if slices.Contains(f.Values, "") {
		return fmt.Errorf("filter value cannot be empty for key %q", f.Key)
	}
	return nil
}

// Match reports whether attrs contains f.Key with a value that equals any of
// f.Values.
func (f *AttributeFilter) Match(attrs pcommon.Map) bool {
	val, ok := attrs.Get(f.Key)
	if !ok {
		return false
	}
	return slices.Contains(f.Values, val.Str())
}
