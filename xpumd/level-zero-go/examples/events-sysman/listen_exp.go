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

// infoLogTimeout searches everything that the collection instance holds.
const infoLogTimeout = -1 * time.Millisecond

var (
	infoLogDump     bool
	infoLogInstance = defaultInfoLogInstance
)

func init() {
	flag.BoolVar(&infoLogDump, "infolog-dump", false,
		"dump the info log records with their per-record metadata, they are read in any case")
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
	// infoLogs is indexed as enumerated, nil for the info logs without an instance
	infoLogs []*intel.InfoLogInstance
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

	// The collection of the info log records has to be started for the
	// CPER_DATA_AVAILABLE event to be reported at all, not just for reading the
	// records: the event is the availability of the records.
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

	l.infoLogs = make([]*intel.InfoLogInstance, len(infoLogs))
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

func (l *eventListener) infoLogInstanceFor(idx int, infoLog *intel.InfoLog) string {
	props, err := infoLog.GetProperties()
	if err != nil {
		log.Printf("Error: driver %d info log %d: GetProperties: %v", l.idx, idx, err)
		return infoLogInstance
	}
	if !props.IsNamedInstancedCollectionSupported.IsTrue() {
		log.Printf("Driver %d info log %d: no support for named instanced collection, collecting into the default buffer",
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

// readInfoLogs reads pending info log records from the collection instances of
// the driver, dumping them when asked for with -infolog-dump. The records are
// always read, and thus consumed: the CPER_DATA_AVAILABLE event signals the
// availability of the records, i.e. it keeps being reported until they have
// been read.
func (l *eventListener) readInfoLogs() {
	for i, instance := range l.infoLogs {
		if instance == nil {
			continue
		}
		// NOTE: the records that were read are valid (and consumed) even if a read
		// failed half-way, i.e. they are dumped despite an error
		records, status, err := instance.ReadWithMetadata(infoLogTimeout)
		if err != nil {
			log.Printf("Error: driver %d info log %d: ReadWithMetadata: %v", l.idx, i, err)
		}
		if status.DroppedRecordCount != 0 {
			incomplete := ""
			if !status.IsDroppedRecordCountValid.IsTrue() {
				incomplete = " (at least, the count is incomplete)"
			}
			log.Printf("Warning: driver %d info log %d: the driver dropped %d records%s",
				l.idx, i, status.DroppedRecordCount, incomplete)
		}
		if len(records) == 0 {
			continue
		}
		dumpInfoLogRecords(i, records)
	}
}

func dumpInfoLogRecords(idx int, records []intel.InfoLogRecord) {
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
func dumpInfoLogRecord(idx int, record intel.InfoLogRecord) {
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
