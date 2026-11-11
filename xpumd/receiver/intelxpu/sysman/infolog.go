//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package sysman

import (
	"errors"
	"fmt"
	"time"

	"go.opentelemetry.io/collector/pdata/plog"
	"go.uber.org/zap"

	"github.com/intel/level-zero-go/core"
	l0sysman "github.com/intel/level-zero-go/sysman"
	l0intel "github.com/intel/level-zero-go/sysman/exp/intel"
)

// The timeout bounds the search of the records (it's not a block/wait
// timeout). Set to negative to let it search all records.
const infoLogReadTimeout = -1 * time.Millisecond

// infoLogAPI is the Sysman API that the info logs of a driver are collected with.
// TODO: drop the Intel experimental API and the related wrappers here once the
// released driver backend implements the stable API.
type infoLogAPI int

const (
	infoLogAPINone infoLogAPI = iota
	infoLogAPIStable
	infoLogAPIIntelExp
)

func (a infoLogAPI) String() string {
	switch a {
	case infoLogAPIStable:
		return "stable"
	case infoLogAPIIntelExp:
		return "intel-experimental"
	default:
		return "none"
	}
}

// infoLogRecord wraps one info log record, independent of the API it was read with.
// TODO: drop with the Intel experimental API.
type infoLogRecord struct {
	address    l0sysman.PciAddress
	fruID      string
	timestamp  uint64 // nanoseconds
	recordType l0sysman.InfoLogRecordTypeExt
	data       []byte
}

// infoLogInstance is one collection instance of an info log.
// TODO: drop with the Intel experimental API.
type infoLogInstance interface {
	// read returns the pending records and the number of records dropped since
	// the previous read (-1 if unknown), the latter also on error.
	read() (records []infoLogRecord, dropped int64, err error)
	delete() error
}

// infoLogSource represents one InfoLog of a Sysman driver plus a collection
// instance that its records are read from.
type infoLogSource struct {
	logger *zap.SugaredLogger
	// name identifies the info log in the logged messages.
	name string
	// instanceName is the name of the collection instance to use.
	instanceName string
	// create starts a collection, returning also its effective buffer size (zero if not known).
	create func(name string) (infoLogInstance, uint32, error)
	// collection instance, nil when the collection hasn't been started or has been stopped.
	instance infoLogInstance
}

// enumInfoLogs detects the supported info log API and enumerates the info logs of a driver.
// TODO: drop with the Intel experimental API (enumInfoLogsStable() becomes the enum func).
func enumInfoLogs(drv *driver, driverIndex int, cfg InfoLogsConfig, logger *zap.SugaredLogger) (
	infoLogAPI, []*infoLogSource) {
	api := infoLogAPIStable
	sources, err := enumInfoLogsStable(drv.Driver.Driver, driverIndex, cfg, logger)
	// Fall back to exp only if the stable API is not supported
	if errors.Is(err, core.RESULT_ERROR_UNSUPPORTED_FEATURE) {
		logger.Infow("Stable info log API not supported: trying the Intel experimental API", "driverIndex", driverIndex)
		api = infoLogAPIIntelExp
		sources, err = enumInfoLogsIntelExp(drv.Driver, driverIndex, cfg, logger)
	}

	switch {
	case errors.Is(err, core.RESULT_ERROR_UNSUPPORTED_FEATURE):
		logger.Infow("No supported info log API: info logs not available", "driverIndex", driverIndex)
	case err != nil:
		logger.Errorw("Driver EnumInfoLogs() failed: info logs not available", zap.Error(err),
			"infoLogAPI", api, "driverIndex", driverIndex)
	case len(sources) == 0:
		logger.Infow("No supported info logs found: info logs not available", "infoLogAPI", api, "driverIndex", driverIndex)
	default:
		logger.Infow("Using the info log API", "infoLogAPI", api, "infoLogs", len(sources), "driverIndex", driverIndex)
		return api, sources
	}
	return infoLogAPINone, nil
}

// newInfoLogSource is a common constructor for the info log source.
// TODO: could be inlined into enumInfoLogs[Stable()] when the Intel experimental API is dropped.
func newInfoLogSource(name string, cfg InfoLogsConfig, logger *zap.SugaredLogger, isCPER, isDevice,
	namedInstance bool, props any) *infoLogSource {
	// Only device-scoped CPER records are supported, skip anything else.
	// Logging just in case, we should never see anything else for now.
	if !isCPER || !isDevice {
		logger.Warnw("Unsupported info log format or type: info log skipped", "properties", props, "infoLog", name)
		return nil
	}
	logger.Infow("Sysman info log found", "infoLog", name, "properties", props)

	// Without support for a named collection instance only the default buffer can be used
	instanceName := cfg.InstanceName
	if !namedInstance {
		logger.Warnw("Named collection instances not supported, using the default buffer of the driver",
			"infoLog", name, "instanceName", instanceName)
		instanceName = ""
	}
	return &infoLogSource{logger: logger, name: name, instanceName: instanceName}
}

func enumInfoLogsStable(drv *l0sysman.Driver, driverIndex int, cfg InfoLogsConfig,
	logger *zap.SugaredLogger) ([]*infoLogSource, error) {
	infoLogs, err := drv.EnumInfoLogs()
	if err != nil {
		return nil, err
	}

	var sources []*infoLogSource
	for i, infoLog := range infoLogs {
		name := fmt.Sprintf("infolog-%d-%d", driverIndex+1, i+1)

		props, err := infoLog.GetProperties()
		if err != nil {
			logger.Errorw("InfoLog GetProperties() failed: info log skipped", zap.Error(err), "infoLog", name)
			continue
		}
		src := newInfoLogSource(name, cfg, logger,
			props.InfoLogFormat == l0sysman.INFO_LOG_FORMAT_EXT_CPER,
			props.InfoLogType == l0sysman.INFO_LOG_TYPE_EXT_DEVICE,
			props.IsNamedInstanceSupported.IsTrue(), props)
		if src == nil {
			continue
		}
		src.create = func(instanceName string) (infoLogInstance, uint32, error) {
			// A zero buffer size leaves the size of the collection buffer to the driver
			instance, err := infoLog.CreateInstance(instanceName, 0)
			if err != nil {
				return nil, 0, err
			}
			return stableInfoLogInstance{instance}, instance.BufferSizeInKb, nil
		}
		sources = append(sources, src)
	}
	return sources, nil
}

func enumInfoLogsIntelExp(drv *l0intel.Driver, driverIndex int, cfg InfoLogsConfig,
	logger *zap.SugaredLogger) ([]*infoLogSource, error) {
	infoLogs, err := drv.EnumInfoLogs()
	if err != nil {
		return nil, err
	}

	var sources []*infoLogSource
	for i, infoLog := range infoLogs {
		name := fmt.Sprintf("infolog-%d-%d", driverIndex+1, i+1)

		props, err := infoLog.GetProperties()
		if err != nil {
			logger.Errorw("InfoLog GetProperties() failed: info log skipped", zap.Error(err), "infoLog", name)
			continue
		}
		src := newInfoLogSource(name, cfg, logger,
			props.InfoLogFormat == l0intel.INFO_LOG_FORMAT_CPER,
			props.InfoLogType == l0intel.INFO_LOG_TYPE_EXP_DEVICE,
			props.IsNamedInstancedCollectionSupported.IsTrue(), props)
		if src == nil {
			continue
		}
		src.create = func(instanceName string) (infoLogInstance, uint32, error) {
			// A zero buffer size leaves the size of the collection buffer to the driver
			instance, err := infoLog.CreateInstance(instanceName, 0)
			if err != nil {
				return nil, 0, err
			}
			return intelExpInfoLogInstance{instance}, instance.BufferSizeInKb, nil
		}
		sources = append(sources, src)
	}
	return sources, nil
}

// stableInfoLogInstance is a collection instance of the stable API.
// TODO: drop with the Intel experimental API.
type stableInfoLogInstance struct {
	*l0sysman.InfoLogInstance
}

func (i stableInfoLogInstance) read() ([]infoLogRecord, int64, error) {
	records, status, err := i.ReadWithMetadata(infoLogReadTimeout)
	// The records read are valid despite the warning, the drops are in the status
	if errors.Is(err, core.RESULT_WARNING_DROPPED_DATA) {
		err = nil
	}
	if err != nil {
		return nil, status.DroppedRecordCount, err
	}

	result := make([]infoLogRecord, len(records))
	for j, rec := range records {
		result[j] = infoLogRecord{
			address:    rec.Metadata.Address,
			fruID:      rec.Metadata.Uuid.Id.String(),
			timestamp:  rec.Metadata.Timestamp,
			recordType: rec.Metadata.RecordType,
			data:       rec.Data,
		}
	}
	return result, status.DroppedRecordCount, nil
}

func (i stableInfoLogInstance) delete() error {
	return i.Delete()
}

// intelExpInfoLogInstance is a collection instance of the Intel experimental API.
// TODO: drop with the Intel experimental API.
type intelExpInfoLogInstance struct {
	*l0intel.InfoLogInstance
}

func (i intelExpInfoLogInstance) read() ([]infoLogRecord, int64, error) {
	records, status, err := i.ReadWithMetadata(infoLogReadTimeout)
	// An incomplete count (!IsDroppedRecordCountValid) is still counted, a later
	// read reports what it misses.
	dropped := int64(status.DroppedRecordCount)
	if err != nil {
		return nil, dropped, err
	}

	result := make([]infoLogRecord, len(records))
	for j, rec := range records {
		result[j] = infoLogRecord{
			address:   rec.Metadata.Address,
			fruID:     rec.Metadata.Uuid.Id.String(),
			timestamp: rec.Metadata.Timestamp,
			// The record type values are the same in both APIs
			recordType: l0sysman.InfoLogRecordTypeExt(rec.Metadata.RecordType),
			data:       rec.Data,
		}
	}
	return result, dropped, nil
}

func (i intelExpInfoLogInstance) delete() error {
	return i.Delete()
}

// startCollection starts the collections of the records of the info log.
func (s *infoLogSource) startCollection() error {
	if s.collecting() {
		return nil
	}
	instance, bufferSizeInKb, err := s.create(s.instanceName)
	if err != nil {
		return err
	}

	s.instance = instance
	s.logger.Infow("Info log collection started", "infoLog", s.name, "instanceName", s.instanceName,
		"bufferSizeKb", bufferSizeInKb)
	return nil
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
	if err := s.instance.delete(); err != nil {
		s.logger.Warnw("InfoLogInstance Delete() failed", zap.Error(err), "infoLog", s.name)
	}
	s.instance = nil
}

// read returns the records pending in the info log collection instance, nil
// when the collection is not running, plus the number of records that the
// driver reported as dropped since the previous read. An unknown drop count is
// reported as one (i.e. the lower bound).
func (s *infoLogSource) read() ([]infoLogRecord, int64) {
	if !s.collecting() {
		return nil, 0
	}

	records, dropped, err := s.instance.read()
	droppedLog := any(dropped)
	if dropped < 0 {
		droppedLog = "unknown"
		dropped = 1
	}

	if err != nil {
		s.logger.Errorw("InfoLogInstance ReadWithMetadata() failed: records no longer collected",
			zap.Error(err), "infoLog", s.name)
		// Stop the collection to prevent flooding the logs. The failure reason tends
		// to persist and we get the event as long as pending log records exist.
		// TODO: consider a recovery mechanism by re-creating the instance after a back-off
		s.stopCollection()
		// The driver reports the dropped records also when the read fails
		return nil, dropped
	}
	if dropped > 0 {
		// The collection buffer of the driver overflowed and it lost records. What
		// was read is valid, so only warn about the loss.
		s.logger.Warnw("Info log records dropped by the driver: collection buffer overflowed",
			"infoLog", s.name, "droppedRecords", droppedLog, "records", len(records))
	}

	return records, dropped
}

// infoLogSeverity maps the type of an info log record to an OTel log severity level.
func infoLogSeverity(recordType l0sysman.InfoLogRecordTypeExt) plog.SeverityNumber {
	switch recordType {
	case l0sysman.INFO_LOG_RECORD_TYPE_EXT_INFORMATIONAL:
		return plog.SeverityNumberInfo
	case l0sysman.INFO_LOG_RECORD_TYPE_EXT_ERROR_CORRECTED:
		return plog.SeverityNumberWarn
	case l0sysman.INFO_LOG_RECORD_TYPE_EXT_ERROR_FATAL:
		return plog.SeverityNumberFatal
	// INFO_LOG_RECORD_TYPE_EXT_ERROR_RECOVERABLE and INFO_LOG_RECORD_TYPE_EXT_UNKNOWN
	default:
		return plog.SeverityNumberError
	}
}

// cperDataAvailableEvent returns the driver scoped event that tells the info
// logs of the driver have records to read.
// TODO: drop with the Intel experimental API.
func (d *driver) cperDataAvailableEvent() l0sysman.EventTypeFlags {
	switch d.infoLogAPI {
	case infoLogAPIStable:
		return l0sysman.EventTypeFlags(l0sysman.EVENT_TYPE_FLAG_INFO_LOG_CPER_DATA_AVAILABLE_EXT)
	case infoLogAPIIntelExp:
		return l0sysman.EventTypeFlags(l0intel.CPER_DATA_AVAILABLE)
	default:
		return 0
	}
}

// registerCPERDataAvailableEvent registers the cperDataAvailableEvent of the driver.
// TODO: drop with the Intel experimental API.
func (d *driver) registerCPERDataAvailableEvent() error {
	switch d.infoLogAPI {
	case infoLogAPIStable:
		return d.EventRegisterExt(d.cperDataAvailableEvent())
	case infoLogAPIIntelExp:
		return d.EventRegister(d.cperDataAvailableEvent())
	default:
		return core.RESULT_ERROR_UNSUPPORTED_FEATURE
	}
}
