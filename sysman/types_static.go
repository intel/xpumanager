// Copyright (C) 2019-2025 Intel Corporation
//
// SPDX-License-Identifier: MIT
//

package sysman

/*
#include "ze_api.h"
#include "zes_api.h"
#include <stdlib.h>
#include "cgo_helpers.h"
*/
import "C"
import (
	"fmt"
	"strings"
)

/* Wrappers for handles */
type Driver struct {
	handle driverHandle
}

func (w *Driver) setHandle(h driverHandle) {
	w.handle = h
}

type Device struct {
	handle deviceHandle
}

func (w *Device) setHandle(h deviceHandle) {
	w.handle = h
}

func (w *Device) getHandle() deviceHandle {
	return w.handle
}

type Overclock struct {
	handle overclockHandle
}

func (w *Overclock) setHandle(h overclockHandle) {
	w.handle = h
}

type Diagnostics struct {
	handle diagHandle
}

func (w *Diagnostics) setHandle(h diagHandle) {
	w.handle = h
}

type Engine struct {
	handle engineHandle
}

func (w *Engine) setHandle(h engineHandle) {
	w.handle = h
}

type FabricPort struct {
	handle fabricPortHandle
}

func (w *FabricPort) setHandle(h fabricPortHandle) {
	w.handle = h
}

func (w *FabricPort) getHandle() fabricPortHandle {
	return w.handle
}

type Fan struct {
	handle fanHandle
}

func (w *Fan) setHandle(h fanHandle) {
	w.handle = h
}

type Firmware struct {
	handle firmwareHandle
}

func (w *Firmware) setHandle(h firmwareHandle) {
	w.handle = h
}

type Freq struct {
	handle freqHandle
}

func (w *Freq) setHandle(h freqHandle) {
	w.handle = h
}

type Led struct {
	handle ledHandle
}

func (w *Led) setHandle(h ledHandle) {
	w.handle = h
}

type Mem struct {
	handle memHandle
}

func (w *Mem) setHandle(h memHandle) {
	w.handle = h
}

type Perf struct {
	handle perfHandle
}

func (w *Perf) setHandle(h perfHandle) {
	w.handle = h
}

type Pwr struct {
	handle pwrHandle
}

func (w *Pwr) setHandle(h pwrHandle) {
	w.handle = h
}

type Psu struct {
	handle psuHandle
}

func (w *Psu) setHandle(h psuHandle) {
	w.handle = h
}

type Ras struct {
	handle rasHandle
}

func (w *Ras) setHandle(h rasHandle) {
	w.handle = h
}

type Sched struct {
	handle schedHandle
}

func (w *Sched) setHandle(h schedHandle) {
	w.handle = h
}

type Standby struct {
	handle standbyHandle
}

func (w *Standby) setHandle(h standbyHandle) {
	w.handle = h
}

type Temp struct {
	handle tempHandle
}

func (w *Temp) setHandle(h tempHandle) {
	w.handle = h
}

/* Generics for handle wrappers */
func handlesToWrappers[H any, V any, W interface {
	*V
	setHandle(H)
}](handles []H) []W {
	wrappers := make([]W, len(handles))
	for i, handle := range handles {
		v := new(V)
		w := W(v)
		w.setHandle(handle)
		wrappers[i] = w
	}
	return wrappers
}

func wrappersToHandles[H any, V any, W interface {
	*V
	getHandle() H
}](wrappers []W) []H {
	handles := make([]H, len(wrappers))
	for i, wrapper := range wrappers {
		handles[i] = wrapper.getHandle()
	}
	return handles
}

/* Types for the higher level golang API */

// OverclockState wraps the overclocking state from zesDeviceReadOverclockState.
type OverclockState struct {
	Mode          OverclockMode
	WaiverSetting bool
	State         bool
	PendingAction PendingAction
	PendingReset  bool
}

/* Stringers for flags types */

// String representation of all set bits of InitFlags.
func (f InitFlags) String() string {
	return flagsToString[InitFlag](InitFlag(f))
}

// String representation of all set bits of EngineTypeFlags.
func (f EngineTypeFlags) String() string {
	return flagsToString[EngineTypeFlag](EngineTypeFlag(f))
}

// String representation of all set bits of ResetReasonFlags.
func (f ResetReasonFlags) String() string {
	return flagsToString[ResetReasonFlag](ResetReasonFlag(f))
}

// String representation of all set bits of DevicePropertyFlags.
func (f DevicePropertyFlags) String() string {
	return flagsToString[DevicePropertyFlag](DevicePropertyFlag(f))
}

// String representation of all set bits of PciLinkQualIssueFlags.
func (f PciLinkQualIssueFlags) String() string {
	return flagsToString[PciLinkQualIssueFlag](PciLinkQualIssueFlag(f))
}

// String representation of all set bits of PciLinkStabIssueFlags.
func (f PciLinkStabIssueFlags) String() string {
	return flagsToString[PciLinkStabIssueFlag](PciLinkStabIssueFlag(f))
}

// String representation of all set bits of EventTypeFlags.
func (f EventTypeFlags) String() string {
	return flagsToString[EventTypeFlag](EventTypeFlag(f))
}

// String representation of all set bits of FabricPortQualIssueFlags.
func (f FabricPortQualIssueFlags) String() string {
	return flagsToString[FabricPortQualIssueFlag](FabricPortQualIssueFlag(f))
}

// String representation of all set bits of FabricPortFailureFlags.
func (f FabricPortFailureFlags) String() string {
	return flagsToString[FabricPortFailureFlag](FabricPortFailureFlag(f))
}

// String representation of all set bits of FreqThrottleReasonFlags.
func (f FreqThrottleReasonFlags) String() string {
	return flagsToString[FreqThrottleReasonFlag](FreqThrottleReasonFlag(f))
}

type flagType interface {
	~int32
	fmt.Stringer
}

func flagsToString[T flagType](flags T) string {
	vals := []string{}

	for flags != 0 {
		// Get the lowest set bit
		bit := flags & -flags

		vals = append(vals, T(bit).String())

		// Clear the lowest set bit
		flags &= flags - 1
	}
	return strings.Join(vals, " | ")
}
