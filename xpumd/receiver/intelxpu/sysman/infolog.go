//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package sysman

import (
	"errors"
	"fmt"

	"go.uber.org/zap"

	"github.com/intel/level-zero-go/core"
	l0intel "github.com/intel/level-zero-go/sysman/exp/intel"
)

// infoLogSource represents one InfoLog instance of a Sysman driver.
type infoLogSource struct {
	*l0intel.InfoLog
	logger *zap.SugaredLogger
	// name identifies the info log in the logged messages.
	name string
	// instance is the name of the collection instance to use.
	instance string
	// enabled tells whether record collection has been successfully enabled.
	enabled bool
}

func enumInfoLogs(drv *driver, driverIndex int, cfg InfoLogsConfig, logger *zap.SugaredLogger) []*infoLogSource {
	infoLogs, err := drv.EnumInfoLogs()
	if err != nil {
		logger.Infow("Driver EnumInfoLogs() failed: info logs not available",
			zap.Error(err), "driverIndex", driverIndex)
		return nil
	}

	var sources []*infoLogSource
	for i, infoLog := range infoLogs {
		name := fmt.Sprintf("infolog-%d-%d", driverIndex+1, i+1)

		props, err := infoLog.GetProperties()
		if err != nil {
			logger.Errorw("InfoLog GetProperties() failed: info log skipped",
				zap.Error(err), "infoLog", name)
			continue
		}
		// Only device-scoped CPER records are supported, skip anything else.
		// Logging just in case, we should never see anything else for now.
		if props.InfoLogFormat != l0intel.INFO_LOG_FORMAT_CPER {
			logger.Warnw("Unsupported info log format: info log skipped",
				"format", props.InfoLogFormat, "infoLog", name)
			continue
		}
		if props.InfoLogType != l0intel.INFO_LOG_TYPE_EXP_DEVICE {
			logger.Warnw("Unsupported info log type: info log skipped",
				"type", props.InfoLogType, "infoLog", name)
			continue
		}
		logger.Infow("Sysman info log found", "infoLog", name, "type", props.InfoLogType,
			"format", props.InfoLogFormat, "maxSizeKiB", props.MaxSize,
			"instancedCollection", props.IsInstancedCollectionSupported != 0)

		// Without support for a named collection instance only the default buffer can be used
		instance := cfg.InstanceName
		if props.IsInstancedCollectionSupported == 0 {
			logger.Warnw("Named collection instances not supported, using the default buffer of the driver",
				"infoLog", name, "instanceName", instance)
			instance = ""
		}

		sources = append(sources, &infoLogSource{
			InfoLog:  infoLog,
			logger:   logger,
			name:     name,
			instance: instance,
		})
	}

	return sources
}

// enable enables the collection of the records of the info log.
func (s *infoLogSource) enable() {
	if s.enabled {
		return
	}
	if err := s.Enable(s.instance, 0, 0); err != nil {
		s.logger.Errorw("InfoLog Enable() failed: info log records not available "+
			"(enabling the collection requires elevated privileges)",
			zap.Error(err), "infoLog", s.name)
		return
	}
	s.enabled = true
	s.logger.Infow("Info log collection enabled", "infoLog", s.name, "instanceName", s.instance)
}

// disable disables the collection of the records of the info log.
func (s *infoLogSource) disable() {
	if !s.enabled {
		return
	}
	if err := s.Disable(); err != nil {
		s.logger.Warnw("InfoLog Disable() failed", zap.Error(err), "infoLog", s.name)
	}
	s.enabled = false
}

// read returns the records pending in the info log, nil when its collection is disabled.
func (s *infoLogSource) read() []l0intel.InfoLogRecord {
	if !s.enabled {
		return nil
	}

	records, err := s.ReadWithMetadata()
	switch {
	case errors.Is(err, core.RESULT_WARNING_DROPPED_DATA):
		// The driver dropped a record that did not fit into the read buffer.
		// What was read is valid, so only warn about the loss.
		s.logger.Warnw("Info log record dropped", "infoLog", s.name, "records", len(records))
	case errors.Is(err, core.RESULT_ERROR_NOT_AVAILABLE):
		// The driver does not have the collection enabled anymore. Handled like any
		// other read failure: the info log is dropped, and the reads of the driver
		// end once the last one is gone (see driverEventListener.readInfoLogs).
		s.logger.Errorw("InfoLog ReadWithMetadata() failed: collection no longer enabled, records not collected",
			zap.Error(err), "infoLog", s.name)
		s.disable()
	case err != nil:
		s.logger.Errorw("InfoLog ReadWithMetadata() failed: records no longer collected",
			zap.Error(err), "infoLog", s.name)
		// Disable to prevent flooding the logs. The failure reason tends to
		// persist and we get the event as long as pending log records exist.
		// TODO: consider a recovery mechanism by trying re-enable after a back-off
		s.disable()
	}

	return records
}
