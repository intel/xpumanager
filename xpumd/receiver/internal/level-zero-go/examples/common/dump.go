// Copyright (C) 2026-2026 Intel Corporation
//
// SPDX-License-Identifier: MIT
//

package common

import (
	"encoding/json"
	"fmt"
	"log"
	"maps"
	"os"
	"reflect"

	"go.yaml.in/yaml/v4"
)

// DumpJSON prints an object in JSON format with with human-readable string
// representation for all types that implement fmt.Stringer.
func DumpJSON(obj any) {
	transformed, err := stringify(reflect.ValueOf(obj))
	if err != nil {
		log.Fatalf("Failed to transform object: %v", err)
	}

	encoder := json.NewEncoder(os.Stdout)
	encoder.SetIndent("", "  ")
	if err := encoder.Encode(transformed); err != nil {
		log.Fatalf("Failed to encode JSON: %v", err)
	}
}

// DumpYAML prints an object in YAML format with human-readable string
// representation for all types that implement fmt.Stringer.
func DumpYAML(obj any) {
	transformed, err := stringify(reflect.ValueOf(obj))
	if err != nil {
		log.Fatalf("Failed to transform object: %v", err)
	}

	encoder := yaml.NewEncoder(os.Stdout)
	if err := encoder.Encode(transformed); err != nil {
		log.Fatalf("Failed to encode YAML: %v", err)
	}
}

// stringify transforms an object into more human-readable form by recursively
// converting types that implement fmt.Stringer into strings.
func stringify(v reflect.Value) (any, error) {
	if !v.IsValid() {
		return nil, nil
	}

	// Resolve interfaces and pointers
	for v.Kind() == reflect.Interface || v.Kind() == reflect.Pointer {
		if v.IsNil() {
			return nil, nil
		}
		v = v.Elem()
	}

	// Return _unaliased_ basic types as-is without string conversion
	// Aliased types (like type FreqDomain int32) have a non-empty PkgPath and should be converted to strings
	switch v.Kind() {
	case reflect.Bool, reflect.String,
		reflect.Int, reflect.Int8, reflect.Int16, reflect.Int32, reflect.Int64,
		reflect.Uint, reflect.Uint8, reflect.Uint16, reflect.Uint32, reflect.Uint64,
		reflect.Float32, reflect.Float64,
		reflect.Complex64, reflect.Complex128:
		// Only return as-is if it's truly a basic type (no package path = not aliased)
		if v.Type().PkgPath() == "" {
			return v.Interface(), nil
		}
	}

	// Return string if type implements fmt.Stringer (includes aliased types)
	if v.Type().Implements(reflect.TypeFor[fmt.Stringer]()) {
		s := v.Interface().(fmt.Stringer)
		return s.String(), nil
	}

	switch v.Kind() {
	case reflect.Struct:
		// Convert struct to map[<field name>]value
		m := make(map[string]any)

		t := v.Type()
		for i := 0; i < v.NumField(); i++ {
			f := t.Field(i)
			if f.PkgPath != "" {
				// Skip unexported fields
				continue
			}

			// Handle anonymous (embedded) fields by promoting their fields
			if f.Anonymous {
				cv, err := stringify(v.Field(i))
				if err != nil {
					return nil, err
				}
				// Merge embedded struct fields into parent
				if embeddedMap, ok := cv.(map[string]any); ok {
					maps.Copy(m, embeddedMap)
				}
				continue
			}

			// Transform recursively. Value will be string if the type
			// implements Stringer.
			cv, err := stringify(v.Field(i))
			if err != nil {
				return nil, err
			}
			m[f.Name] = cv
		}
		return m, nil

	case reflect.Map:
		m := make(map[string]any)

		iter := v.MapRange()
		for iter.Next() {
			k := iter.Key()
			kk, err := stringify(k)
			if err != nil {
				return nil, err
			}
			strKey := fmt.Sprint(kk)
			// Transform recursively. Value will be string if the type
			// implements Stringer.
			cv, err := stringify(iter.Value())
			if err != nil {
				return nil, err
			}
			m[strKey] = cv
		}
		return m, nil

	case reflect.Slice, reflect.Array:
		l := v.Len()
		s := make([]any, l)
		for i := range l {
			// Transform recursively. Value will be string if the type
			// implements Stringer.
			cv, err := stringify(v.Index(i))
			if err != nil {
				return nil, err
			}
			s[i] = cv
		}
		return s, nil

	default:
		return fmt.Sprint(v.Interface()), nil
	}
}
