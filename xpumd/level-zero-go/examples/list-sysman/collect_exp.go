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
	"time"

	"github.com/intel/level-zero-go/sysman"
	"github.com/intel/level-zero-go/sysman/exp"
	"github.com/intel/level-zero-go/sysman/exp/intel"
)

const defaultInfoLogInstance = "list-sysman"

// infoLogTimeout searches everything that the collection instance holds.
const infoLogTimeout = -1 * time.Millisecond

var (
	infoLogDump     bool
	infoLogInstance = defaultInfoLogInstance
)

func init() {
	flag.BoolVar(&infoLogDump, "infolog-dump", false,
		"dump the info log records with their per-record metadata, they are left unread by default")
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

func (d *DeviceInfo) collectVFInfoExp(device *sysman.Device) {
	vfs, err := exp.NewDevice(device).EnumEnabledVFExp()
	if err != nil {
		d.recordError("VirtualFunctions", err)
		return
	}

	result := make([]VFInfo, len(vfs))
	for i, vf := range vfs {
		if caps, err := vf.GetCapabilitiesExp(); err != nil {
			d.recordError("VirtualFunction.GetCapabilitiesExp2", err)
		} else {
			result[i].Capabilities = &caps
		}
		if memUtil, err := vf.GetMemoryUtilizationExp(); err != nil {
			d.recordError("VirtualFunction.GetMemoryUtilizationExp2", err)
		} else {
			result[i].MemoryUtilization = memUtil
		}
		if engineUtil, err := vf.GetEngineUtilizationExp(); err != nil {
			d.recordError("VirtualFunction.GetEngineUtilizationExp2", err)
		} else {
			result[i].EngineUtilization = engineUtil
		}
	}

	d.VirtualFunctions = result
}

// collectInfoLogsExp lists the info logs of the driver.
// NOTE: The log records themselves are only collected when asked for with
// -infolog-dump, as collecting them may consume them.
func (d *DriverInfo) collectInfoLogsExp(driver *sysman.Driver) {
	infoLogs, err := intel.NewDriver(driver).EnumInfoLogs()
	if err != nil {
		d.recordError("IntelInfoLogs", err)
		return
	}

	result := make([]InfoLogInfo, len(infoLogs))
	for i, infoLog := range infoLogs {
		if props, err := infoLog.GetProperties(); err != nil {
			d.recordError("IntelInfoLog.GetProperties", err)
		} else {
			result[i].Properties = &props
		}
		if infoLogDump {
			d.readInfoLogExp(infoLog, &result[i])
		}
	}

	d.IntelInfoLogs = result
}

// readInfoLogExp reads all the pending records of one info log.
//
// NOTE: the records are peeked at, i.e. left in place for the other consumers of
// the log, whenever the log supports peeking. Otherwise, the records are read and thus consumed.
func (d *DriverInfo) readInfoLogExp(infoLog *intel.InfoLog, info *InfoLogInfo) {
	props := info.Properties
	name := infoLogInstance
	if props != nil && !props.IsNamedInstancedCollectionSupported.IsTrue() {
		name = ""
	}
	instance, err := infoLog.CreateInstance(name, 0)
	if err != nil {
		d.recordError("IntelInfoLog.CreateInstance", err)
		return
	}
	defer func() {
		if err := instance.Delete(); err != nil {
			d.recordError("IntelInfoLogInstance.Delete", err)
		}
	}()

	collect, context := instance.PeekWithMetadata, "IntelInfoLogInstance.PeekWithMetadata"
	if props == nil || !props.IsPeekSupported.IsTrue() {
		collect, context = instance.ReadWithMetadata, "IntelInfoLogInstance.ReadWithMetadata"
	}

	// NOTE: the records that were collected are valid (and consumed, unless
	// peeked at) even if the collection failed midway. They are reported
	// despite an error.
	records, status, err := collect(infoLogTimeout)
	d.recordError(context, err)

	info.BufferSizeInKb = instance.BufferSizeInKb
	info.Collection = &InfoLogCollection{Status: status}
	if len(records) == 0 {
		return
	}

	info.Collection.Records = make([]InfoLogRecord, len(records))
	for i, record := range records {
		info.Collection.Records[i] = InfoLogRecord{InfoLogMetadataExp: record.Metadata, Dump: hex.Dump(record.Data)}
	}
}
