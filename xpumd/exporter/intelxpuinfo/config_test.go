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
	"go.opentelemetry.io/collector/pdata/pcommon"
)

func mustParseTemplate(text string) *template.Template {
	return template.Must(
		template.New("health_domain").Option("missingkey=zero").Parse(text),
	)
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
