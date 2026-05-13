//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package k8s

import (
	"bytes"
	"fmt"
	"io"
	"maps"
	"math"
	"net/http"
	"slices"
	"strings"
	"testing"
	"time"

	dto "github.com/prometheus/client_model/go"
	"github.com/prometheus/common/expfmt"
	"github.com/prometheus/common/model"
)

const (
	metricPollInterval = 500 * time.Millisecond
	metricFetchTimeout = 5 * time.Second
)

// labels is a Prometheus label set. Use with() to derive new sets without
// mutating the original.
type labels map[string]string

// with returns a new labels set that combines the original with the given extra entries.
func (l labels) with(extra labels) labels {
	result := maps.Clone(l)
	maps.Copy(result, extra)
	return result
}

func (l labels) String() string {
	keys := slices.Sorted(maps.Keys(l))
	parts := make([]string, len(keys))
	for i, k := range keys {
		parts[i] = fmt.Sprintf("%s=%q", k, l[k])
	}
	return strings.Join(parts, ", ")
}

// metricAssertion describes a single expected Prometheus metric.
type metricAssertion struct {
	name   string
	labels labels
	value  float64
	// skip the value check, used for metrics whose value varies with timing
	skipValue bool
}

// assert is a test helper that looks up the metric and reports any errors.
func (a *metricAssertion) assert(t *testing.T, families map[string]*dto.MetricFamily) {
	t.Helper()
	if _, err := a.findMetric(families); err != nil {
		t.Error(err)
	}
}

// findMetric looks up the metric by name and labels and checks the value
// (unless skipValue is set).
func (a *metricAssertion) findMetric(families map[string]*dto.MetricFamily) (*dto.Metric, error) {
	family := families[a.name]
	if family == nil {
		return nil, fmt.Errorf("metric %q not found", a.name)
	}

	for _, metric := range family.Metric {
		if !labelsMatch(metric.GetLabel(), a.labels) {
			continue
		}
		v := metricValue(metric)
		if !a.skipValue && !floatAlmostEqual(v, a.value) {
			return nil, fmt.Errorf("unexpected value %v (expected %v) for metric %s{%s}", v, a.value, a.name, a.labels)
		}
		return metric, nil
	}

	var sb strings.Builder
	fmt.Fprintf(&sb, "metric %s{%s} not found, %d candidate(s):",
		a.name, a.labels, len(family.Metric))
	for _, m := range family.Metric {
		fmt.Fprintf(&sb, "\n  {%s}", labelPairsToLabels(m.GetLabel()))
	}
	return nil, fmt.Errorf("%s", sb.String())
}

// waitForMetric polls the metrics endpoint until the given assertion passes.
// Returns all the metrics for further assertions.
func waitForMetric(t *testing.T, endpoint string, wantMetric metricAssertion, timeout time.Duration) map[string]*dto.MetricFamily {
	t.Helper()

	var lastErr error
	deadline := time.Now().Add(timeout)
	for time.Now().Before(deadline) {
		families, err := fetchMetrics(endpoint)
		if err == nil {
			_, lastErr = wantMetric.findMetric(families)
			if lastErr == nil {
				return families
			}
		} else {
			lastErr = err
		}
		time.Sleep(metricPollInterval)
	}

	t.Fatalf("timed out after %v waiting for metric: %v", timeout, lastErr)
	return nil
}

func fetchMetrics(endpoint string) (map[string]*dto.MetricFamily, error) {
	client := http.Client{Timeout: metricFetchTimeout}
	resp, err := client.Get(fmt.Sprintf("http://%s/metrics", endpoint))
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close() //nolint:errcheck // read-only, best-effort cleanup

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}
	if resp.StatusCode != http.StatusOK {
		return nil, fmt.Errorf("HTTP GET for '%s' failed: status %s", endpoint, resp.Status)
	}

	parser := expfmt.NewTextParser(model.LegacyValidation)
	return parser.TextToMetricFamilies(bytes.NewReader(body))
}

func labelsMatch(pairs []*dto.LabelPair, want labels) bool {
	got := labelPairsToLabels(pairs)
	for key, value := range want {
		if got[key] != value {
			return false
		}
	}
	return true
}

func labelPairsToLabels(pairs []*dto.LabelPair) labels {
	l := make(labels, len(pairs))
	for _, p := range pairs {
		l[p.GetName()] = p.GetValue()
	}
	return l
}

func metricValue(metric *dto.Metric) float64 {
	switch {
	case metric.GetGauge() != nil:
		return metric.GetGauge().GetValue()
	case metric.GetCounter() != nil:
		return metric.GetCounter().GetValue()
	case metric.GetUntyped() != nil:
		return metric.GetUntyped().GetValue()
	default:
		return math.NaN()
	}
}

func floatAlmostEqual(got, want float64) bool {
	if math.IsNaN(got) || math.IsNaN(want) {
		return false
	}
	diff := math.Abs(got - want)
	scale := math.Max(1, math.Abs(want))
	return diff <= 1e-9*scale
}
