//go:build exp

// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package main

import (
	"encoding/hex"
	"errors"
	"flag"
	"fmt"
	"log"
	"strings"
	"time"

	"github.com/intel/level-zero-go/examples/common"
	"github.com/intel/level-zero-go/sysman"
	"github.com/intel/level-zero-go/sysman/exp/intel"
)

// allDriverEventFlags registers interest in every driver scoped event type.
const allDriverEventFlags = sysman.EventTypeFlags(intel.CPER_DATA_AVAILABLE)

const (
	maxDumpSize            = 512
	dumpIndent             = "    "
	defaultInfoLogInstance = "events-sysman"
)

var (
	infoLogFormat   = common.InfoLogFormatNone
	infoLogInstance = defaultInfoLogInstance
)

func init() {
	flag.Var(&infoLogFormat, "infolog-format", fmt.Sprintf(
		"dump the info log records with their per-record metadata (%s) or as one opaque blob (%s), "+
			"or not at all (%s); the records are read in any case",
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

// eventListener listens for the events of one driver.
type eventListener struct {
	idx          int
	driver       *intel.Driver
	driverEvents bool
	infoLogs     []*intel.InfoLog
}

// newEventListener registers the driver scoped events of the driver. A failure
// is only logged (the device scoped events are listened for in any case).
func newEventListener(idx int, driver *sysman.Driver) *eventListener {
	l := &eventListener{idx: idx, driver: intel.NewDriver(driver)}

	if err := l.driver.EventRegister(allDriverEventFlags); err != nil {
		log.Printf("Error: driver %d: driver scoped EventRegister: %v", idx, err)
		return l
	}
	l.driverEvents = true
	log.Printf("Registered for driver scoped events on driver %d: CPER_DATA_AVAILABLE", idx)

	// The collection of the info log records has to be enabled for the
	// CPER_DATA_AVAILABLE event to be reported at all, not just for reading the
	// records: the event is the availability of the records.
	l.enableInfoLogs()

	return l
}

// enableInfoLogs enables the collection of the records of all info logs of the driver.
// The default collection buffer configuration of the driver is used. A failure is only logged.
func (l *eventListener) enableInfoLogs() {
	infoLogs, err := l.driver.EnumInfoLogs()
	if err != nil {
		log.Printf("Error: driver %d: EnumInfoLogs: %v", l.idx, err)
		return
	}

	for i, infoLog := range infoLogs {
		if err := infoLog.Enable(l.infoLogInstanceFor(i, infoLog), 0, 0); err != nil {
			log.Printf("Error: driver %d info log %d: Enable: %v", l.idx, i, err)
			continue
		}
		l.infoLogs = append(l.infoLogs, infoLog)
	}
}

func (l *eventListener) infoLogInstanceFor(idx int, infoLog *intel.InfoLog) string {
	props, err := infoLog.GetProperties()
	if err != nil {
		log.Printf("Error: driver %d info log %d: GetProperties: %v", l.idx, idx, err)
		return infoLogInstance
	}
	if props.IsInstancedCollectionSupported == 0 {
		log.Printf("Driver %d info log %d: no support for instanced collection, enabling without an instance name",
			l.idx, idx)
		return ""
	}

	return infoLogInstance
}

// listen waits for the device scoped events of the given devices and for the
// driver scoped events of the driver. The driver scoped events are reported
// here, the device scoped ones are returned to the caller together with a flag
// telling whether any of the devices had events.
func (l *eventListener) listen(timeout time.Duration,
	devices []*sysman.Device) (bool, []sysman.EventTypeFlags, error) {
	if !l.driverEvents {
		// The driver does not implement the driver scoped events, listen for the
		// device scoped ones with the stable API only.
		numEvents, events, err := l.driver.EventListenEx(timeout, devices)
		return numEvents != 0, events, err
	}

	hasDeviceEvents, events, driverEvents, err := l.driver.EventListenExp(timeout, devices)
	if err != nil {
		return false, nil, err
	}

	// NOTE: the stringer of the stable event flags does not know the driver
	// scoped event types, they are reported by name here.
	if driverEvents&intel.CPER_DATA_AVAILABLE != 0 {
		fmt.Printf("[driver %d] CPER_DATA_AVAILABLE\n", l.idx)
		l.readInfoLogs()
	}

	return hasDeviceEvents, events, nil
}

// readInfoLogs reads the pending info log records of the driver, dumping them in
// the format selected with -infolog-format. The records are always read, and thus
// consumed: the CPER_DATA_AVAILABLE event signals the availability of the records,
// i.e. it keeps being reported until they have been read.
func (l *eventListener) readInfoLogs() {
	for i, infoLog := range l.infoLogs {
		if infoLogFormat == common.InfoLogFormatMetadata {
			l.readInfoLogWithMetadata(i, infoLog)
			continue
		}
		// The records that are not dumped are read as one opaque blob, too, as
		// their metadata is not needed for reporting the size only
		l.readInfoLogRaw(i, infoLog)
	}
}

// readInfoLogWithMetadata reads the pending records of one info log together with
// their per-record metadata.
func (l *eventListener) readInfoLogWithMetadata(idx int, infoLog *intel.InfoLog) {
	// NOTE: the data read is valid (and consumed) even if the driver had to drop records, i.e. it is dumped despite an error
	records, err := infoLog.ReadWithMetadata()
	if err != nil {
		log.Printf("Error: driver %d info log %d: ReadWithMetadata: %v", l.idx, idx, err)
	}
	if len(records) == 0 {
		return
	}

	size := 0
	for _, record := range records {
		size += len(record.Data)
	}

	fmt.Printf("%sinfo log %d: %d records, %d bytes\n", dumpIndent, idx, len(records), size)
	for i, record := range records {
		dumpInfoLogRecord(i, record)
	}
}

// readInfoLogRaw reads the pending records of one info log as one opaque blob,
// i.e. without the metadata that locates and describes each record in it. Only
// their size is reported when -infolog-format asks for no dump at all.
func (l *eventListener) readInfoLogRaw(idx int, infoLog *intel.InfoLog) {
	// NOTE: the data read is valid (and consumed) even if the driver had to drop records, i.e. it is dumped despite an error
	data, err := infoLog.ReadAll()
	if err != nil {
		log.Printf("Error: driver %d info log %d: ReadAll: %v", l.idx, idx, err)
	}
	if len(data) == 0 {
		return
	}

	if infoLogFormat == common.InfoLogFormatNone {
		fmt.Printf("%sinfo log %d: %d bytes read (dump them with -infolog-format %s|%s)\n",
			dumpIndent, idx, len(data), common.InfoLogFormatMetadata, common.InfoLogFormatRaw)
		return
	}

	shown := min(len(data), maxDumpSize)
	truncated := ""
	if shown < len(data) {
		truncated = fmt.Sprintf(", first %d shown", shown)
	}
	fmt.Printf("%sinfo log %d: %d bytes%s\n", dumpIndent, idx, len(data), truncated)
	for line := range strings.Lines(hex.Dump(data[:shown])) {
		fmt.Print(dumpIndent, dumpIndent, line)
	}
}

// dumpInfoLogRecord prints the metadata of one info log record together with a
// hex dump of the record itself, truncated to maxDumpSize bytes.
func dumpInfoLogRecord(idx int, record intel.InfoLogRecord) {
	meta := record.Metadata
	pciBDF := fmt.Sprintf("%04x:%02x:%02x.%x",
		meta.Address.Domain, meta.Address.Bus, meta.Address.Device, meta.Address.Function)

	shown := min(len(record.Data), maxDumpSize)
	truncated := ""
	if shown < len(record.Data) {
		truncated = fmt.Sprintf(", first %d shown", shown)
	}
	fmt.Printf("%s%srecord %d: %d bytes%s from [%s] %s at %d\n",
		dumpIndent, dumpIndent, idx, len(record.Data), truncated, pciBDF, meta.Uuid.Id, meta.Timestamp)
	for line := range strings.Lines(hex.Dump(record.Data[:shown])) {
		fmt.Print(dumpIndent, dumpIndent, dumpIndent, line)
	}
}
