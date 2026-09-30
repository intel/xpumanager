//go:build !exp

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

	"github.com/intel/level-zero-go/core"
	"github.com/intel/level-zero-go/sysman"
)

// allDriverEventFlags registers interest in every driver scoped event type.
const allDriverEventFlags = sysman.EventTypeFlags(sysman.EVENT_TYPE_FLAG_INFO_LOG_CPER_DATA_AVAILABLE_EXT)

const (
	maxDumpSize            = 512
	dumpIndent             = "    "
	defaultInfoLogInstance = "events-sysman"
)

// infoLogTimeout searches everything that the collection instance holds.
const infoLogTimeout = -1 * time.Millisecond

var (
	infoLogDump     bool
	infoLogInstance string
)

func init() {
	flag.BoolVar(&infoLogDump, "infolog-dump", false,
		"dump the info log records with their per-record metadata, they are read in any case")
	flag.StringVar(&infoLogInstance, "infolog-instance", defaultInfoLogInstance,
		"`name` of the collection instance for the info log records, empty for the global instance")
}

// eventListener listens for the events of one driver.
type eventListener struct {
	idx          int
	driver       *sysman.Driver
	driverEvents bool
	// infoLogs is indexed as enumerated, nil for the info logs without an instance
	infoLogs []*sysman.InfoLogInstance
}

// newEventListener registers the driver scoped events of the driver. A failure
// is only logged (the device scoped events are listened for in any case).
func newEventListener(idx int, driver *sysman.Driver) *eventListener {
	l := &eventListener{idx: idx, driver: driver}

	if err := driver.EventRegisterExt(allDriverEventFlags); err != nil {
		log.Printf("Error: driver %d: driver scoped EventRegisterExt: %v", idx, err)
		return l
	}
	l.driverEvents = true
	log.Printf("Registered for driver scoped events on driver %d: %s", idx, allDriverEventFlags)

	// The collection of the info log records has to be started for the
	// INFO_LOG_CPER_DATA_AVAILABLE_EXT event to be reported at all, not just for
	// reading the records: the event is the availability of the records.
	l.createInfoLogInstances()

	return l
}

// createInfoLogInstances starts the collection of the records of all info logs of
// the driver. A failure is only logged.
func (l *eventListener) createInfoLogInstances() {
	infoLogs, err := l.driver.EnumInfoLogs()
	if err != nil {
		log.Printf("Error: driver %d: EnumInfoLogs: %v", l.idx, err)
		return
	}

	l.infoLogs = make([]*sysman.InfoLogInstance, len(infoLogs))
	for i, infoLog := range infoLogs {
		// The same instance name across different InfoLogs is expected to be fine
		instance, err := infoLog.CreateInstance(l.infoLogInstanceFor(i, infoLog), 0)
		if err != nil {
			log.Printf("Error: driver %d info log %d: CreateInstance: %v", l.idx, i, err)
			continue
		}
		log.Printf("Collecting the records of driver %d info log %d, buffer size %d kB",
			l.idx, i, instance.BufferSizeInKb)
		l.infoLogs[i] = instance
	}
}

func (l *eventListener) infoLogInstanceFor(idx int, infoLog *sysman.InfoLog) string {
	if infoLogInstance == "" {
		return ""
	}
	props, err := infoLog.GetProperties()
	if err != nil {
		log.Printf("Error: driver %d info log %d: GetProperties: %v", l.idx, idx, err)
		return infoLogInstance
	}
	if !props.IsNamedInstanceSupported.IsTrue() {
		log.Printf("Driver %d info log %d: no support for named instances, collecting into the default buffer",
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
		// device scoped ones only.
		numEvents, events, err := l.driver.EventListenEx(timeout, devices)
		return numEvents != 0, events, err
	}

	numEvents, events, driverEvents, err := l.driver.EventListenExt(timeout, devices)
	if err != nil {
		return false, nil, err
	}

	if driverEvents != 0 {
		fmt.Printf("[driver %d] %s\n", l.idx, driverEvents)
	}
	if driverEvents&sysman.EventTypeFlags(sysman.EVENT_TYPE_FLAG_INFO_LOG_CPER_DATA_AVAILABLE_EXT) != 0 {
		l.readInfoLogs()
	}

	return numEvents != 0, events, nil
}

// readInfoLogs reads pending info log records from the collection instances of
// the driver, dumping them when asked for with -infolog-dump. The records are
// always read, and thus consumed: the INFO_LOG_CPER_DATA_AVAILABLE_EXT event
// signals the availability of the records, i.e. it keeps being reported until
// they have been read.
func (l *eventListener) readInfoLogs() {
	for i, instance := range l.infoLogs {
		if instance == nil {
			continue
		}
		// NOTE: the records are valid (and consumed) even if the driver had to drop
		// some, i.e. they are dumped despite the warning
		records, status, err := instance.ReadWithMetadata(infoLogTimeout)
		switch {
		case errors.Is(err, core.RESULT_WARNING_DROPPED_DATA):
			log.Printf("Warning: driver %d info log %d: the driver dropped %d records", l.idx, i, status.DroppedRecordCount)
		case err != nil:
			log.Printf("Error: driver %d info log %d: ReadWithMetadata: %v", l.idx, i, err)
		}
		if len(records) == 0 {
			continue
		}
		dumpInfoLogRecords(i, records)
	}
}

func dumpInfoLogRecords(idx int, records []sysman.InfoLogRecord) {
	size := 0
	for _, record := range records {
		size += len(record.Data)
	}

	if !infoLogDump {
		fmt.Printf("%sinfo log %d: %d records, %d bytes (dump them with -infolog-dump)\n",
			dumpIndent, idx, len(records), size)
		return
	}

	fmt.Printf("%sinfo log %d: %d records, %d bytes\n", dumpIndent, idx, len(records), size)
	for i, record := range records {
		dumpInfoLogRecord(i, record)
	}
}

// dumpInfoLogRecord prints the metadata of one info log record together with a
// hex dump of the record itself, truncated to maxDumpSize bytes.
func dumpInfoLogRecord(idx int, record sysman.InfoLogRecord) {
	meta := record.Metadata
	pciBDF := fmt.Sprintf("%04x:%02x:%02x.%x",
		meta.Address.Domain, meta.Address.Bus, meta.Address.Device, meta.Address.Function)

	shown := min(len(record.Data), maxDumpSize)
	truncated := ""
	if shown < len(record.Data) {
		truncated = fmt.Sprintf(", first %d shown", shown)
	}
	fmt.Printf("%s%srecord %d: %s, %d bytes%s from [%s] %s at %d\n",
		dumpIndent, dumpIndent, idx, meta.RecordType, len(record.Data), truncated, pciBDF, meta.Uuid.Id, meta.Timestamp)
	for line := range strings.Lines(hex.Dump(record.Data[:shown])) {
		fmt.Print(dumpIndent, dumpIndent, dumpIndent, line)
	}
}

// close deletes the collection instances of the info logs. A failure is only logged.
func (l *eventListener) close() {

	for i, instance := range l.infoLogs {
		if instance == nil {
			continue
		}
		if err := instance.Delete(); err != nil {
			log.Printf("Error: driver %d info log %d: Delete: %v", l.idx, i, err)
		}
	}
	l.infoLogs = nil
}
