// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT
//

package testhelper

import (
	"testing"

	"github.com/intel/level-zero-go/core"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

// ConfigDefault is the stub driver config used by the test helpers (relative to the package directory of the test).
const ConfigDefault = "testdata/default.yaml"

// LoadConfig reloads the stub with the given driver config file.
func LoadConfig(t *testing.T, path string) {
	t.Helper()
	require.NoError(t, stubReload(path))
}

// FixedStringProperty is the set of fixed size string types of the Level Zero API.
type FixedStringProperty interface {
	core.StringProperty64 | core.StringProperty256
}

// StringProperty converts a string into a fixed size string property.
func StringProperty[T FixedStringProperty](value string) T {
	var prop T
	switch p := any(&prop).(type) {
	case *core.StringProperty64:
		copy(p[:], value)
	case *core.StringProperty256:
		copy(p[:], value)
	default:
		panic("unsupported string property type")
	}
	return prop
}

// GetIndexed enumerates objects and returns the one at given index.
func GetIndexed[T any](t *testing.T, what string, idx int, enum func() ([]T, error)) T {
	t.Helper()
	items, err := enum()
	require.NoError(t, err)
	require.Greater(t, len(items), idx, "%s index out of range", what)
	return items[idx]
}

// Config holds the configuration of one test case: the stub driver config to
// use, the expected outcome and the indices addressing the object under test.
type Config struct {
	name       string
	configFile string
	wantErr    error
	DrvIdx     int
	DevIdx     int
	CompIdx    int
}

// Opt is a functional option for tuning a Config.
type Opt func(*Config)

func WithConfig(cfg string) Opt { return func(c *Config) { c.configFile = cfg } }
func WithError(err error) Opt   { return func(c *Config) { c.wantErr = err } }
func WithDrvIdx(idx int) Opt    { return func(c *Config) { c.DrvIdx = idx } }
func WithDevIdx(idx int) Opt    { return func(c *Config) { c.DevIdx = idx } }
func WithCompIdx(idx int) Opt   { return func(c *Config) { c.CompIdx = idx } }
func WithName(name string) Opt  { return func(c *Config) { c.name = name } }

// NewConfig returns a Config for a test case.
func NewConfig(name string, wantErr error, opts ...Opt) *Config {
	cfg := &Config{name: name, wantErr: wantErr}
	for _, o := range opts {
		o(cfg)
	}
	return cfg
}

// Getter is a base helper for testing getter-like functions (that return a value and an error).
func Getter[T, R any](t *testing.T, cfg *Config, getObj func(*testing.T) T, method func(T) (R, error), check func(*testing.T, R)) {
	t.Helper()
	t.Run(cfg.name, func(t *testing.T) {
		LoadConfig(t, cfg.config())
		got, err := method(getObj(t))
		cfg.requireErr(t, err)
		if check != nil {
			check(t, got)
		}
	})
}

// Action is a base helper for testing action-like functions (that only return an error).
func Action[T any](t *testing.T, cfg *Config, getObj func(*testing.T) T, action func(T) error) {
	t.Helper()
	t.Run(cfg.name, func(t *testing.T) {
		LoadConfig(t, cfg.config())
		cfg.requireErr(t, action(getObj(t)))
	})
}

// config returns the stub driver config of the test case.
func (c *Config) config() string {
	if c.configFile == "" {
		return ConfigDefault
	}
	return c.configFile
}

// requireErr asserts that err matches the expected outcome of the test case.
func (c *Config) requireErr(t *testing.T, err error) {
	t.Helper()
	if c.wantErr != nil {
		require.ErrorIs(t, err, c.wantErr)
	} else {
		require.NoError(t, err)
	}
}

// CheckValue returns a check function that asserts a value.
func CheckValue[T any](want T) func(*testing.T, T) {
	return func(t *testing.T, got T) {
		t.Helper()
		assert.Equal(t, want, got)
	}
}

// CheckValueExported returns a check function that asserts exported fields of a value.
func CheckValueExported[T any](want T) func(*testing.T, T) {
	return func(t *testing.T, got T) {
		t.Helper()
		assert.EqualExportedValues(t, want, got)
	}
}

// CheckLen returns a check function that asserts the length of a slice.
func CheckLen[T any](n int) func(*testing.T, []T) {
	return func(t *testing.T, v []T) {
		t.Helper()
		require.Len(t, v, n)
	}
}
