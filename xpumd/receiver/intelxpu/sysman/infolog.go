//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package sysman

import (
	"fmt"
	"time"

	"go.opentelemetry.io/collector/pdata/plog"
	"go.uber.org/zap"

	l0sysman "github.com/intel/level-zero-go/sysman"
	l0intel "github.com/intel/level-zero-go/sysman/exp/intel"
)

// The timeout bounds the search of the records (it's not a block/wait
// timeout). Set to negative to let it search all records.
const infoLogReadTimeout = -1 * time.Millisecond

// cperDataAvailableEvent is the driver scoped event that tells the info logs of
// the driver have records to read.
const cperDataAvailableEvent = l0sysman.EventTypeFlags(l0intel.CPER_DATA_AVAILABLE)

// infoLogSource represents one InfoLog of a Sysman driver plus a collection
// instance that its records are read from.
type infoLogSource struct {
	*l0intel.InfoLog
	logger *zap.SugaredLogger
	// name identifies the info log in the logged messages.
	name string
	// instanceName is the name of the collection instance to use.
	instanceName string
	// collection instance, nil when the collection hasn't been started or has been stopped.
	instance *l0intel.InfoLogInstance
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
			logger.Errorw("InfoLog GetProperties() failed: info log skipped", zap.Error(err), "infoLog", name)
			continue
		}
		// Only device-scoped CPER records are supported, skip anything else.
		// Logging just in case, we should never see anything else for now.
		if props.InfoLogFormat != l0intel.INFO_LOG_FORMAT_CPER ||
			props.InfoLogType != l0intel.INFO_LOG_TYPE_EXP_DEVICE {
			logger.Warnw("Unsupported info log format or type: info log skipped", "properties", props,
				"infoLog", name)
			continue
		}
		logger.Infow("Sysman info log found", "infoLog", name, "properties", props)

		// Without support for a named collection instance only the default buffer can be used
		instanceName := cfg.InstanceName
		if !props.IsNamedInstancedCollectionSupported.IsTrue() {
			logger.Warnw("Named collection instances not supported, using the default buffer of the driver",
				"infoLog", name, "instanceName", instanceName)
			instanceName = ""
		}

		sources = append(sources, &infoLogSource{
			InfoLog:      infoLog,
			logger:       logger,
			name:         name,
			instanceName: instanceName,
		})
	}
	return sources
}

// startCollection starts the collections of the records of the info log.
func (s *infoLogSource) startCollection() {
	if s.collecting() {
		return
	}
	// A zero buffer size leaves the size of the collection buffer as it is
	instance, err := s.CreateInstance(s.instanceName, 0)
	if err != nil {
		s.logger.Errorw("InfoLog CreateInstance() failed: info log records not available "+
			"(collecting them requires elevated privileges)",
			zap.Error(err), "infoLog", s.name)
		return
	}
	s.instance = instance
	s.logger.Infow("Info log collection started", "infoLog", s.name, "instanceName", s.instanceName,
		"bufferSizeKb", instance.BufferSizeInKb)
}

// collecting tells whether the records of the info log are collected.
func (s *infoLogSource) collecting() bool {
	return s.instance != nil
}

// stopCollection stops the collection of the records of the info log.
func (s *infoLogSource) stopCollection() {
	if !s.collecting() {
		return
	}
	if err := s.instance.Delete(); err != nil {
		s.logger.Warnw("InfoLogInstance Delete() failed", zap.Error(err), "infoLog", s.name)
	}
	s.instance = nil
}

// read returns the records pending in the info log collection instance, nil when the collection is not running.
func (s *infoLogSource) read() []l0intel.InfoLogRecord {
	if !s.collecting() {
		return nil
	}

	records, status, err := s.instance.ReadWithMetadata(infoLogReadTimeout)
	switch {
	case err != nil:
		s.logger.Errorw("InfoLogInstance ReadWithMetadata() failed: records no longer collected",
			zap.Error(err), "infoLog", s.name)
		// Stop the collection to prevent flooding the logs. The failure reason tends
		// to persist and we get the event as long as pending log records exist.
		// TODO: consider a recovery mechanism by re-creating the instance after a back-off
		s.stopCollection()
	// The dropped record count is only meaningful when the read succeeded
	case status.IsDroppedRecordCountValid.IsTrue() && status.DroppedRecordCount != 0:
		// The collection buffer of the driver overflowed and it lost records. What
		// was read is valid, so only warn about the loss.
		s.logger.Warnw("Info log records dropped by the driver: collection buffer overflowed",
			"infoLog", s.name, "droppedRecords", status.DroppedRecordCount, "records", len(records))
	}

	return records
}

// infoLogSeverity maps the type of an info log record to an OTel log severity level.
func infoLogSeverity(recordType l0intel.InfoLogRecordTypeExp) plog.SeverityNumber {
	switch recordType {
	case l0intel.INFO_LOG_RECORD_TYPE_EXP_INFORMATIONAL:
		return plog.SeverityNumberInfo
	case l0intel.INFO_LOG_RECORD_TYPE_EXP_ERROR_CORRECTED:
		return plog.SeverityNumberWarn
	case l0intel.INFO_LOG_RECORD_TYPE_EXP_ERROR_FATAL:
		return plog.SeverityNumberFatal
	// INFO_LOG_RECORD_TYPE_EXP_ERROR_RECOVERABLE and INFO_LOG_RECORD_TYPE_EXP_UNKNOWN
	default:
		return plog.SeverityNumberError
	}
}
