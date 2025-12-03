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

type zeDriverHandle C.ze_driver_handle_t

type zeDeviceHandle C.ze_device_handle_t

type zesSchedHandle C.zes_sched_handle_t

type zesPerfHandle C.zes_perf_handle_t

type zesPwrHandle C.zes_pwr_handle_t

type zesFreqHandle C.zes_freq_handle_t

type zesEngineHandle C.zes_engine_handle_t

type zesStandbyHandle C.zes_standby_handle_t

type zesFirmwareHandle C.zes_firmware_handle_t

type zesMemHandle C.zes_mem_handle_t

type zesFabricPortHandle C.zes_fabric_port_handle_t

type zesTempHandle C.zes_temp_handle_t

type zesPsuHandle C.zes_psu_handle_t

type zesFanHandle C.zes_fan_handle_t

type zesLedHandle C.zes_led_handle_t

type zesRasHandle C.zes_ras_handle_t

type zesDiagHandle C.zes_diag_handle_t

type zesOverclockHandle C.zes_overclock_handle_t

type zesVfHandle C.zes_vf_handle_t

/* Wrappers for handles */
type Driver struct {
	handle zeDriverHandle
}

func (w *Driver) setHandle(h zeDriverHandle) {
	w.handle = h
}

type Device struct {
	handle zeDeviceHandle
}

func (w *Device) setHandle(h zeDeviceHandle) {
	w.handle = h
}

func (w *Device) getHandle() zeDeviceHandle {
	return w.handle
}

type Overclock struct {
	handle zesOverclockHandle
}

func (w *Overclock) setHandle(h zesOverclockHandle) {
	w.handle = h
}

type Diagnostics struct {
	handle zesDiagHandle
}

func (w *Diagnostics) setHandle(h zesDiagHandle) {
	w.handle = h
}

type Engine struct {
	handle zesEngineHandle
}

func (w *Engine) setHandle(h zesEngineHandle) {
	w.handle = h
}

type FabricPort struct {
	handle zesFabricPortHandle
}

func (w *FabricPort) setHandle(h zesFabricPortHandle) {
	w.handle = h
}

func (w *FabricPort) getHandle() zesFabricPortHandle {
	return w.handle
}

type Fan struct {
	handle zesFanHandle
}

func (w *Fan) setHandle(h zesFanHandle) {
	w.handle = h
}

type Firmware struct {
	handle zesFirmwareHandle
}

func (w *Firmware) setHandle(h zesFirmwareHandle) {
	w.handle = h
}

type Freq struct {
	handle zesFreqHandle
}

func (w *Freq) setHandle(h zesFreqHandle) {
	w.handle = h
}

type Led struct {
	handle zesLedHandle
}

func (w *Led) setHandle(h zesLedHandle) {
	w.handle = h
}

type Mem struct {
	handle zesMemHandle
}

func (w *Mem) setHandle(h zesMemHandle) {
	w.handle = h
}

type Perf struct {
	handle zesPerfHandle
}

func (w *Perf) setHandle(h zesPerfHandle) {
	w.handle = h
}

type Pwr struct {
	handle zesPwrHandle
}

func (w *Pwr) setHandle(h zesPwrHandle) {
	w.handle = h
}

type Psu struct {
	handle zesPsuHandle
}

func (w *Psu) setHandle(h zesPsuHandle) {
	w.handle = h
}

type Ras struct {
	handle zesRasHandle
}

func (w *Ras) setHandle(h zesRasHandle) {
	w.handle = h
}

type Sched struct {
	handle zesSchedHandle
}

func (w *Sched) setHandle(h zesSchedHandle) {
	w.handle = h
}

type Standby struct {
	handle zesStandbyHandle
}

func (w *Standby) setHandle(h zesStandbyHandle) {
	w.handle = h
}

type Temp struct {
	handle zesTempHandle
}

func (w *Temp) setHandle(h zesTempHandle) {
	w.handle = h
}

type Vf struct {
	handle zesVfHandle
}

func (w *Vf) setHandle(h zesVfHandle) {
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
