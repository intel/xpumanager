//go:build exp

// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT
//

package main

import (
	"encoding/hex"
	"errors"
	"flag"
	"fmt"

	"github.com/intel/level-zero-go/examples/common"
	"github.com/intel/level-zero-go/sysman"
	"github.com/intel/level-zero-go/sysman/exp"
	"github.com/intel/level-zero-go/sysman/exp/intel"
)

const defaultInfoLogInstance = "list-sysman"

var (
	infoLogFormat   = common.InfoLogFormatNone
	infoLogInstance = defaultInfoLogInstance
)

func init() {
	flag.Var(&infoLogFormat, "infolog-format", fmt.Sprintf(
		"dump the info log records with their per-record metadata (%s) or as one opaque blob (%s), "+
			"or leave them unread (%s)",
		common.InfoLogFormatMetadata, common.InfoLogFormatRaw, common.InfoLogFormatNone))
	// NOTE: flag.Func instead of flag.StringVar to reject an empty name
	flag.Func("infolog-instance", fmt.Sprintf(
		"`name` of the collection instance for the info log records (default %q)", defaultInfoLogInstance),
		func(name string) error {
			if name == "" {
				return errors.New("must not be empty")
			}
			infoLogInstance = name
			return nil
		})
}

func (d *DeviceInfo) collectRasInfoExp(ras *sysman.Ras, info *RasInfo) {
	if states, err := exp.NewRas(ras).GetStateExp(); err != nil {
		d.recordError("RasErrorSet.GetStateExp", err)
	} else {
		info.StateExp = states
	}
}

// collectInfoLogsExp lists the info logs of the driver.
// NOTE: The log records themselves are only read when asked for with
// -infolog-format, as reading consumes them.
func (d *DriverInfo) collectInfoLogsExp(driver *sysman.Driver) {
	infoLogs, err := intel.NewDriver(driver).EnumInfoLogs()
	if err != nil {
		d.recordError("InfoLogs", err)
		return
	}

	result := make([]InfoLogInfo, len(infoLogs))
	for i, infoLog := range infoLogs {
		if props, err := infoLog.GetProperties(); err != nil {
			d.recordError("InfoLog.GetProperties", err)
		} else {
			result[i].Properties = &props
		}
		if infoLogFormat != common.InfoLogFormatNone {
			result[i].Records = d.readInfoLogExp(infoLog, result[i].Properties)
		}
	}

	d.InfoLogs = result
}

// readInfoLogExp reads all the pending records of one info log, consuming them.
// The default collection buffer configuration of the driver is used.
func (d *DriverInfo) readInfoLogExp(infoLog *intel.InfoLog, props *intel.InfoLogPropertiesExp) *InfoLogRecords {
	instance := infoLogInstance
	if props != nil && props.IsInstancedCollectionSupported == 0 {
		instance = ""
	}
	if err := infoLog.Enable(instance, 0, 0); err != nil {
		d.recordError("InfoLog.Enable", err)
		return nil
	}
	defer func() {
		if err := infoLog.Disable(); err != nil {
			d.recordError("InfoLog.Disable", err)
		}
	}()

	// NOTE: the data read is valid (and consumed) even if the driver had to
	// drop records, i.e. it is reported despite an error
	if infoLogFormat == common.InfoLogFormatRaw {
		data, err := infoLog.ReadAll()
		if err != nil {
			d.recordError("InfoLog.ReadAll", err)
		}
		if len(data) == 0 {
			return nil
		}
		return &InfoLogRecords{Dump: hex.Dump(data)}
	}

	records, err := infoLog.ReadWithMetadata()
	if err != nil {
		d.recordError("InfoLog.ReadWithMetadata", err)
	}
	if len(records) == 0 {
		return nil
	}

	result := &InfoLogRecords{Records: make([]InfoLogRecord, len(records))}
	for i, record := range records {
		result.Records[i] = InfoLogRecord{Metadata: record.Metadata, Dump: hex.Dump(record.Data)}
	}
	return result
}
