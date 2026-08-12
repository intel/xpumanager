//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package intelxpuinfo

import (
	"testing"
	"text/template"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"go.opentelemetry.io/collector/confmap"
	"go.opentelemetry.io/collector/pdata/pcommon"

	"github.com/intel/xpumanager/xpumd/common"
	pb "github.com/intel/xpumanager/xpumd/exporter/intelxpuinfo/api/deviceinfo/v1alpha1"
)

func mustParseTemplate(text string) *template.Template {
	return template.Must(
		template.New("health_domain").Option("missingkey=zero").Parse(text),
	)
}

// TestConfigValidateCompilesDerivedFields verifies that the recursive config
// validation works as expected and in the process fills the derived
// (unexported) fields into the configuration (itself, not into a copy of it).
func TestConfigValidateCompilesDerivedFields(t *testing.T) {
	cfg := defaultConfig().(*Config)
	cfg.HwStatusMappings = []HwStatusMapping{
		{
			HealthDomain: "{{.hw_type}}",
			StateMapping: map[string]HwStateMapping{
				"warning": {Severity: "warning"},
			},
		},
	}

	require.NoError(t, confmap.Validate(cfg))

	m := &cfg.HwStatusMappings[0]
	require.NotNil(t, m.healthDomainTmpl)
	assert.Equal(t, pb.SeverityLevel_SEVERITY_LEVEL_WARNING, m.StateMapping["warning"].severityLevel)

	attrs := pcommon.NewMap()
	attrs.PutStr("hw.type", "gpu")
	domain, err := m.healthDomainFor(attrs)
	require.NoError(t, err)
	assert.Equal(t, "gpu", domain)
}

// TestConfigValidate verifies that validation errors from the
// nested hw status mappings are propagated by the collector's recursive
// configuration validation.
func TestConfigValidate(t *testing.T) {
	tests := []struct {
		name      string
		mapping   HwStatusMapping
		expectErr string
	}{
		{
			name: "valid mapping",
			mapping: HwStatusMapping{
				HealthDomain: "gpu.health",
				Filters:      common.AttributeFilterList{{Key: "hw.type", Values: []string{"gpu"}}},
				StateMapping: map[string]HwStateMapping{"degraded": {Severity: "warning"}},
			},
		},
		{
			name:      "missing health domain",
			mapping:   HwStatusMapping{},
			expectErr: "health_domain must be specified",
		},
		{
			name:      "invalid health domain template",
			mapping:   HwStatusMapping{HealthDomain: "{{.hw_type"},
			expectErr: "invalid health_domain template",
		},
		{
			name: "invalid severity",
			mapping: HwStatusMapping{
				HealthDomain: "gpu.health",
				StateMapping: map[string]HwStateMapping{"degraded": {Severity: "bogus"}},
			},
			expectErr: `state_mapping["degraded"]: invalid severity "bogus"`,
		},
		// Basic test to check that the recursive validation applies to the attribute filters as well
		{
			name: "invalid filter",
			mapping: HwStatusMapping{
				HealthDomain: "gpu.health",
				Filters:      common.AttributeFilterList{{Key: ""}},
			},
			expectErr: "filter key is required",
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			cfg, ok := defaultConfig().(*Config)
			require.True(t, ok)
			cfg.HwStatusMappings = []HwStatusMapping{tt.mapping}

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

func TestHealthDomainFor(t *testing.T) {
	tests := []struct {
		name         string
		tmplText     string
		attrs        map[string]string
		expectErr    bool
		expectDomain string
	}{
		{
			name:         "render static string",
			tmplText:     "static_domain",
			attrs:        map[string]string{"hw.type": "gpu", "hw.sensor_location": "memory", "hw.vendor": "acme"},
			expectDomain: "static_domain",
		},
		{
			name:         "render single attribute",
			tmplText:     "{{.hw_type}}",
			attrs:        map[string]string{"hw.type": "gpu", "hw.sensor_location": "memory", "hw.vendor": "acme"},
			expectDomain: "gpu",
		},
		{
			name:         "render multiple attributes",
			tmplText:     "{{.hw_type}}.{{.hw_sensor_location}}",
			attrs:        map[string]string{"hw.type": "gpu", "hw.sensor_location": "memory", "hw.vendor": "acme"},
			expectDomain: "gpu.memory",
		},
		{
			name:      "error, empty string",
			tmplText:  "",
			attrs:     map[string]string{"hw.type": ""},
			expectErr: true,
		},
		{
			name:      "error, missing attribute",
			tmplText:  "{{.hw_type}}",
			attrs:     map[string]string{"hw.vendor": "acme"},
			expectErr: true,
		},
		{
			name:      "error, execution error",
			tmplText:  `{{slice .hw_type 999}}`, // out-of-range at runtime
			attrs:     map[string]string{"hw.type": "gpu"},
			expectErr: true,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			attrs := pcommon.NewMap()
			for k, v := range tt.attrs {
				attrs.PutStr(k, v)
			}
			m := HwStatusMapping{
				HealthDomain:     tt.tmplText,
				healthDomainTmpl: mustParseTemplate(tt.tmplText),
			}
			got, err := m.healthDomainFor(attrs)
			if tt.expectErr {
				require.Error(t, err)
			} else {
				require.NoError(t, err)
			}
			assert.Equal(t, tt.expectDomain, got)
		})
	}
}
