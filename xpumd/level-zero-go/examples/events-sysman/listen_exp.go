//go:build exp

// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package main

import (
	"fmt"
	"log"
	"time"

	"github.com/intel/level-zero-go/sysman"
	"github.com/intel/level-zero-go/sysman/exp/intel"
)

// allDriverEventFlags registers interest in every driver scoped event type.
const allDriverEventFlags = sysman.EventTypeFlags(intel.CPER_DATA_AVAILABLE)

const dumpIndent = "    "

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
	// CPER_DATA_AVAILABLE event to be reported at all: the event is the
	// availability of the records.
	l.enableInfoLogs()

	return l
}

// enableInfoLogs enables the collection of the records of all info logs of the
// driver into the global trace buffer, with the buffer configuration defaults of
// the driver. Enabling requires elevated privileges, a failure is only logged.
func (l *eventListener) enableInfoLogs() {
	infoLogs, err := l.driver.EnumInfoLogs()
	if err != nil {
		log.Printf("Error: driver %d: EnumInfoLogs: %v", l.idx, err)
		return
	}

	for i, infoLog := range infoLogs {
		if err := infoLog.Enable("", 0, 0); err != nil {
			log.Printf("Error: driver %d info log %d: Enable: %v", l.idx, i, err)
			continue
		}
		l.infoLogs = append(l.infoLogs, infoLog)
	}
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

// readInfoLogs reads the pending info log records of the driver, reporting their
// size. The records are always read, and thus consumed: the
// CPER_DATA_AVAILABLE event signals the availability of the records, i.e. it
// keeps being reported until they have been read.
func (l *eventListener) readInfoLogs() {
	for i, infoLog := range l.infoLogs {
		// NOTE: the data read is valid (and consumed) even if the driver had to drop records
		data, err := infoLog.ReadAll()
		if err != nil {
			log.Printf("Error: driver %d info log %d: ReadAll: %v", l.idx, i, err)
		}
		if len(data) == 0 {
			continue
		}

		fmt.Printf("%sinfo log %d: %d bytes read\n", dumpIndent, i, len(data))
	}
}
