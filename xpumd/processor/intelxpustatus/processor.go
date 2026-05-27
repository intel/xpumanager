//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package intelxpustatus

import (
	"context"

	"go.opentelemetry.io/collector/pdata/pcommon"
	"go.opentelemetry.io/collector/pdata/pmetric"
	"go.opentelemetry.io/collector/processor"
	"go.uber.org/zap"
)

type xpuHealthProcessor struct {
	cfg      *Config
	logger   *zap.SugaredLogger
	settings processor.Settings
}

func newXpuHealthProcessor(cfg *Config, settings processor.Settings) *xpuHealthProcessor {
	return &xpuHealthProcessor{
		cfg:      cfg,
		logger:   settings.Logger.Sugar(),
		settings: settings,
	}
}

func (p *xpuHealthProcessor) processMetrics(_ context.Context, md pmetric.Metrics) (pmetric.Metrics, error) {
	for _, rule := range p.cfg.Rules {
		rp := newRuleProcessor(p.logger, &rule)
		rp.process(md)
	}
	return md, nil
}

type ruleProcessor struct {
	*HealthRule
	logger      *zap.SugaredLogger
	parentAttrs map[string]pcommon.Map
}

func newRuleProcessor(logger *zap.SugaredLogger, rule *HealthRule) *ruleProcessor {
	return &ruleProcessor{
		HealthRule:  rule,
		logger:      logger,
		parentAttrs: make(map[string]pcommon.Map),
	}
}

func (p *ruleProcessor) process(md pmetric.Metrics) {
	// First pass: collect parent metadata
	for _, rm := range md.ResourceMetrics().All() {
		for _, sm := range rm.ScopeMetrics().All() {
			p.collectParentMetadata(sm)
		}
	}

	// Second pass: create health metrics
	for _, rm := range md.ResourceMetrics().All() {
		for _, sm := range rm.ScopeMetrics().All() {
			p.updateMetrics(sm)
		}
	}
}

func (p *ruleProcessor) collectParentMetadata(sm pmetric.ScopeMetrics) {
	if p.ParentMetric == "" {
		return
	}
	for _, m := range sm.Metrics().All() {
		metricName := m.Name()
		if metricName != p.ParentMetric {
			continue
		}

		dps, ok := getNumberDatapoints(m)
		if !ok {
			p.logger.Warnw("parent metric has unsupported type", "metric", metricName, "type", m.Type())
			continue
		}

		for _, dp := range dps.All() {
			attrs := dp.Attributes()
			idVal, _ := attrs.Get(p.ParentIDAttribute)
			id := idVal.Str()
			if id == "" {
				p.logger.Debugw("empty or missing ID attribute of parent (parent_id_attribute)", "metric", metricName, "ParentIDAttribute", p.ParentIDAttribute, "attributes", attrs.AsRaw())
				continue
			}

			if a, exists := p.parentAttrs[id]; exists {
				p.logger.Warnw("duplicate ID attribute of parent (parent_id_attribute)", "metric", metricName, "ParentIDAttribute", p.ParentIDAttribute, "attributes", attrs.AsRaw(), "duplicateAttributes", a.AsRaw())
				continue
			}
			p.parentAttrs[id] = attrs
		}
	}
}

func (p *ruleProcessor) updateMetrics(sm pmetric.ScopeMetrics) {
	// Prepare the source metric
	sourceMetric, ok := findMetricByName(sm, p.SourceMetric)
	if !ok {
		return
	}
	sourceDps, ok := getNumberDatapoints(sourceMetric)
	if !ok {
		p.logger.Errorw("source metric has unsupported type", "metric", sourceMetric.Name(), "type", sourceMetric.Type())
		return
	}

	// Prepare the health status metric
	statusMetric, ok := findMetricByName(sm, p.StatusMetric)
	if !ok {
		// Create new "UpDownCounter" metric
		statusMetric = sm.Metrics().AppendEmpty()
		statusMetric.SetName(p.StatusMetric)
		statusMetric.SetEmptySum()
		statusMetric.Sum().SetIsMonotonic(false)
		statusMetric.Sum().SetAggregationTemporality(pmetric.AggregationTemporalityCumulative)
	}

	statusDps, ok := getNumberDatapoints(statusMetric)
	if !ok {
		p.logger.Errorw("status metric has unsupported type", "metric", statusMetric.Name(), "type", statusMetric.Type())
		return
	}

	// Iterate over source metric data points and evaluate each against the state rules
	for _, dp := range sourceDps.All() {
		sourceAttrs := dp.Attributes()

		// Apply rule-level filters
		if !p.ComponentFilters.Match(sourceAttrs) {
			continue
		}

		// Apply parent filters
		parentID := ""
		if p.ParentRefAttribute != "" {
			parentIDVal, _ := sourceAttrs.Get(p.ParentRefAttribute)
			parentID = parentIDVal.Str()
			if parentID == "" {
				p.logger.Debugw("empty or missing parent ref attribute (parent_ref_attribute)", "metric", p.SourceMetric, "ParentRefAttribute", p.ParentRefAttribute, "attributes", sourceAttrs.AsRaw())
			}

			parentAttrs, exists := p.parentAttrs[parentID]
			if !exists {
				p.logger.Debugw("missing parent attributes", "metric", p.SourceMetric, "parentID", parentID, "attributes", sourceAttrs.AsRaw())
				parentAttrs = pcommon.Map{}
			}
			if !p.ParentFilters.Match(parentAttrs) {
				continue
			}
		}

		var value float64
		switch t := dp.ValueType(); t {
		case pmetric.NumberDataPointValueTypeInt:
			value = float64(dp.IntValue())
		case pmetric.NumberDataPointValueTypeDouble:
			value = dp.DoubleValue()
		default:
			p.logger.Errorw("unsupported data point value type", "metric", p.SourceMetric, "valueType", t, "attributes", sourceAttrs.AsRaw())
			continue
		}

		// Evaluate state rules to determine states and their values
		states := p.evaluateStates(value, parentID)

		// Create health status metric
		statusAttrs := pcommon.NewMap()
		// Copy relevant attributes (semconv) from the source metric
		for _, attrKey := range p.CopyAttributes {
			if attrVal, ok := sourceAttrs.Get(attrKey); ok {
				statusAttrs.PutStr(attrKey, attrVal.Str())
			}
		}
		for key, val := range p.AddAttributes {
			statusAttrs.PutStr(key, val)
		}

		p.createHealthMetric(statusDps, statusAttrs, states, dp.Timestamp())
	}
}

func (p *ruleProcessor) evaluateStates(value float64, parentID string) map[string]uint64 {
	parentAttrs := p.parentAttrs[parentID]

	// Iterate over all state rules. Set the last matching one as the active one.
	states := make(map[string]uint64, len(p.States))
	activeState := ""
	for _, rule := range p.States {
		states[rule.StateName] = uint64(0)

		// No conditions means unconditional match
		if len(rule.Conditions) == 0 {
			activeState = rule.StateName
			continue
		}

		for _, cond := range rule.Conditions {
			if cond.match(value, parentAttrs) {
				activeState = rule.StateName
				break // continue to next state rule
			}
		}
	}

	if activeState != "" {
		states[activeState] = uint64(1)
	}
	return states
}

func (p *ruleProcessor) createHealthMetric(dps pmetric.NumberDataPointSlice, baseAttrs pcommon.Map, states map[string]uint64, timestamp pcommon.Timestamp) {
	for state, value := range states {
		dp := dps.AppendEmpty()
		dp.SetIntValue(int64(value))
		dp.SetTimestamp(timestamp)

		attrs := dp.Attributes()
		baseAttrs.CopyTo(attrs)

		attrs.PutStr(p.StateAttribute, state)
	}
}

func findMetricByName(sm pmetric.ScopeMetrics, name string) (pmetric.Metric, bool) {
	for _, statusMetric := range sm.Metrics().All() {
		if statusMetric.Name() == name {
			return statusMetric, true
		}
	}
	return pmetric.Metric{}, false
}

func getNumberDatapoints(metric pmetric.Metric) (pmetric.NumberDataPointSlice, bool) {
	switch metric.Type() {
	case pmetric.MetricTypeGauge:
		return metric.Gauge().DataPoints(), true
	case pmetric.MetricTypeSum:
		return metric.Sum().DataPoints(), true
	default:
		return pmetric.NewNumberDataPointSlice(), false
	}
}
