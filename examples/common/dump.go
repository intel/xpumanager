// Copyright (C) 2019-2025 Intel Corporation
//
// SPDX-License-Identifier: MIT
//

package common

import (
	"fmt"
	"log"
	"reflect"
	"strings"

	"go.yaml.in/yaml/v4"
)

// Dump prints an object in human-readable YAML format with the specified
// indentation.
func Dump(obj interface{}, indent int) {
	prefix := strings.Repeat(" ", indent)

	transformed, err := stringify(reflect.ValueOf(obj))
	if err != nil {
		log.Printf("ERROR: Failed to transform object: %v", err)
		return
	}
	data, err := yaml.Marshal(transformed)
	if err != nil {
		log.Printf("ERROR: Failed to marshal object: %v", err)
		return
	}
	lines := strings.Split(string(data), "\n")
	for _, line := range lines {
		if line != "" {
			fmt.Printf("%s%s\n", prefix, line)
		}
	}
}

// stringify transforms an object into more human-readable form by recursively
// converting types that implement fmt.Stringer into strings.
func stringify(v reflect.Value) (any, error) {
	if !v.IsValid() {
		return nil, nil
	}

	// Resolve interfaces and pointers
	for v.Kind() == reflect.Interface || v.Kind() == reflect.Ptr {
		if v.IsNil() {
			return nil, nil
		}
		v = v.Elem()
	}

	// Return string if type implements fmt.Stringer
	if v.Type().Implements(reflect.TypeOf((*fmt.Stringer)(nil)).Elem()) {
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
		for i := 0; i < l; i++ {
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
