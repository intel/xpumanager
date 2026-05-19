//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package exporter

import (
	"bytes"
	"fmt"
	"maps"
	"regexp"
	"slices"
	"strings"
	"text/template"

	"go.opentelemetry.io/collector/component"
	"go.opentelemetry.io/collector/config/configgrpc"
	"go.opentelemetry.io/collector/config/confignet"
	"go.opentelemetry.io/collector/pdata/pcommon"

	"github.com/intel/xpumanager/xpumd/common"
	pb "github.com/intel/xpumanager/xpumd/exporter/api/deviceinfo/v1alpha1"
)

// severityNames maps configuration severity name strings to pb.SeverityLevel
// values. It is used both for config validation and for building translators.
var severityNames = map[string]pb.SeverityLevel{
	"unknown":  pb.SeverityLevel_SEVERITY_LEVEL_UNKNOWN,
	"ok":       pb.SeverityLevel_SEVERITY_LEVEL_OK,
	"warning":  pb.SeverityLevel_SEVERITY_LEVEL_WARNING,
	"critical": pb.SeverityLevel_SEVERITY_LEVEL_CRITICAL,
	"failed":   pb.SeverityLevel_SEVERITY_LEVEL_FAILED,
}

// validReasonRe matches a single-word reason: alphanumerics plus '.', '-', '_'.
var validReasonRe = regexp.MustCompile(`^[A-Za-z0-9._-]+$`)

// HwStateMapping maps a single hw.state attribute value to health status.
// The map key in HwStatusMapping.StateMapping is the hw.state value to match.
type HwStateMapping struct {
	// Severity is the resulting health severity level. Valid values are:
	// "unknown", "ok", "warning", "critical", "failed".
	Severity string `mapstructure:"severity"`
	// Reason is a short (single word) reason for the status. Defaults to the
	// hw.state attribute value (the map key) when empty.
	Reason string `mapstructure:"reason"`
	// Message is an optional detailed human-readable message included in the
	// health status. Defaults to empty string (no message) when not set.
	Message string `mapstructure:"message"`

	// severityLevel is compiled from Severity during Validate.
	severityLevel pb.SeverityLevel
}

// HwStatusMapping maps a set of attribute filters to a map of state mappings.
// All filters must match for the mapping to apply (AND semantics). An empty
// Filters slice matches unconditionally and acts as a default/catch-all.
type HwStatusMapping struct {
	// Filters are the attribute filters that must all match for this
	// mapping to apply. An empty slice matches any data point.
	Filters common.AttributeFilterList `mapstructure:"filters"`
	// HealthDomain is a Go template for the health domain name. Attribute
	// values are available as template variables (with dots in the name
	// replaced by underscores, e.g.  "hw.type" -> .hw_type).
	HealthDomain string `mapstructure:"health_domain"`
	// StateMapping maps hw.state attribute values to their health status
	// output. The map key is the hw.state value to match. The special key
	// "*" acts as a default catch-all for any state not matched by an
	// exact key.
	StateMapping map[string]HwStateMapping `mapstructure:"state_mapping"`
	// ShowAllStates, if true, includes all active hw.state values in the
	// output, not just the one with the highest severity for each health domain.
	ShowAllStates bool `mapstructure:"show_all_states"`

	// healthDomainTmpl is compiled from HealthDomain during Validate.
	healthDomainTmpl *template.Template
}

// healthDomainFor returns the health domain name for the given attrs. If a
// HealthDomain template is configured it is rendered with attributes as
// variables (dots in names replaced by underscores). When no template is
// configured the default "{hw.type}[.{hw.sensor_location}]" is used.
func (m *HwStatusMapping) healthDomainFor(attrs pcommon.Map) (string, error) {
	data := make(map[string]string, attrs.Len())
	attrs.Range(func(k string, v pcommon.Value) bool {
		data[strings.ReplaceAll(k, ".", "_")] = v.Str()
		return true
	})
	var buf bytes.Buffer
	if err := m.healthDomainTmpl.Execute(&buf, data); err != nil {
		return "", fmt.Errorf("failed to render health domain template: %w", err)
	}
	if buf.Len() == 0 {
		return "", fmt.Errorf("health domain name is empty")
	}

	return buf.String(), nil
}

// Config defines configuration for the exporter.
type Config struct {
	configgrpc.ServerConfig `mapstructure:",squash"`

	// HwStatusMappings specifies hw.status to health status mappings.
	// Order matters: the first entry that applies (the attribute filters
	// match) is used. The default case (catch-all entry with empty Filters),
	// if any, should be the last entry.
	HwStatusMappings []HwStatusMapping `mapstructure:"hw_status_mappings"`
}

// Validate checks if the configuration is valid.
func (cfg *Config) Validate() error {
	if cfg.NetAddr.Transport != confignet.TransportTypeUnix {
		return fmt.Errorf("unsupported transport type: %q, only %q is supported", cfg.NetAddr.Transport, confignet.TransportTypeUnix)
	}
	for i, hwTypeMapping := range cfg.HwStatusMappings {
		if err := hwTypeMapping.Validate(); err != nil {
			return fmt.Errorf("hw_status_mappings[%d]: %w", i, err)
		}
	}
	return nil
}

// Validate checks if the mapping is valid and compiles derived fields.
func (m *HwStatusMapping) Validate() error {
	if err := m.Filters.Validate(); err != nil {
		return fmt.Errorf("invalid filter: %w", err)
	}

	if m.HealthDomain == "" {
		return fmt.Errorf("health_domain must be specified")

	}
	tmpl, err := template.New("health_domain").Option("missingkey=zero").Parse(m.HealthDomain)
	if err != nil {
		return fmt.Errorf("invalid health_domain template: %w", err)
	}
	m.healthDomainTmpl = tmpl

	for hwState, sm := range m.StateMapping {
		if err := sm.Validate(hwState); err != nil {
			return err
		}
		sm.severityLevel = severityNames[sm.Severity]
		m.StateMapping[hwState] = sm
	}
	return nil
}

// healthStatusFor looks up the health status for the given hw.state value.
// Exact keys are tried first, then the special key "*" acts as a default
// catch-all. Returns the zero HwStateMapping and false if nothing matches.
func (m *HwStatusMapping) healthStatusFor(hwState string) (HwStateMapping, bool) {
	if sm, ok := m.StateMapping[hwState]; ok {
		return sm, true
	}
	if sm, ok := m.StateMapping["*"]; ok {
		return sm, true
	}
	return HwStateMapping{}, false
}

// Validate checks if the state mapping is valid. hwState is the map key and
// is used only in error messages.
func (m *HwStateMapping) Validate(hwState string) error {
	if _, ok := severityNames[m.Severity]; !ok {
		return fmt.Errorf("state_mapping[%q]: invalid severity %q, valid values are: %s",
			hwState, m.Severity, slices.Sorted(maps.Keys(severityNames)))
	}
	if m.Reason != "" {
		if len(m.Reason) >= 64 {
			return fmt.Errorf("state_mapping[%q]: reason %q must be shorter than 64 characters",
				hwState, m.Reason)
		}
		if !validReasonRe.MatchString(m.Reason) {
			return fmt.Errorf("state_mapping[%q]: reason %q must match regex %q",
				hwState, m.Reason, validReasonRe.String())
		}
	}
	return nil
}

// defaultConfig provides default configuration values.
func defaultConfig() component.Config {
	cfg := &Config{
		ServerConfig: configgrpc.NewDefaultServerConfig(),
	}

	// Set transport to Unix Domain Sockets by default.
	cfg.NetAddr.Transport = confignet.TransportTypeUnix

	return cfg
}
