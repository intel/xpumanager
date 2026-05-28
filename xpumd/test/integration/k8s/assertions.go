//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package k8s

import (
	"fmt"
	"os"
	"testing"

	"go.yaml.in/yaml/v4"
)

// scenarioConfig groups the assertions of one sub-test scenario.
type scenarioConfig struct {
	MetricsSentinel   metricAssertion     `yaml:"metrics_sentinel"`
	MetricsAssertions metricAssertionList `yaml:"metrics_assertions"`
}

// loadAssertionsFrom reads and parses an assertions config file.
func loadAssertionsFrom(t *testing.T, path string) (map[string]scenarioConfig, error) {
	t.Helper()
	data, err := os.ReadFile(path)
	if err != nil {
		return nil, fmt.Errorf("failed to read assertions file %q: %w", path, err)
	}
	var af map[string]scenarioConfig
	if err := yaml.Unmarshal(data, &af); err != nil {
		return nil, fmt.Errorf("failed to parse assertions file %q: %w", path, err)
	}
	return af, nil
}

// loadAssertions is like loadAssertionsFrom but derives the path from test name.
func loadAssertions(t *testing.T) (map[string]scenarioConfig, error) {
	t.Helper()
	return loadAssertionsFrom(t, suite.testdataFile(t, "assertions.yaml"))
}

// requireScenarioConfig looks up scenarioConfig by name and fails if not found.
func requireScenarioConfig(t *testing.T, assertConfig map[string]scenarioConfig, key string) scenarioConfig {
	t.Helper()
	cfg, ok := assertConfig[key]
	if !ok {
		t.Fatalf("assertions missing required key %q", key)
	}
	return cfg
}
