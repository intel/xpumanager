//go:build !exp

// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package main

import (
	"time"

	"github.com/intel/level-zero-go/sysman"
)

// eventListener listens for the events of one driver.
type eventListener struct {
	driver *sysman.Driver
}

// newEventListener creates a listener for the events of the driver.
func newEventListener(_ int, driver *sysman.Driver) *eventListener {
	return &eventListener{driver: driver}
}

// listen waits for the device scoped events of the given devices, returning them
// together with a flag telling whether any of the devices had events.
func (l *eventListener) listen(timeout time.Duration,
	devices []*sysman.Device) (bool, []sysman.EventTypeFlags, error) {
	numEvents, events, err := l.driver.EventListenEx(timeout, devices)
	return numEvents != 0, events, err
}
