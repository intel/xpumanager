// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT
//

package intel

import (
	"github.com/intel/level-zero-go/core"
	"github.com/intel/level-zero-go/sysman"
)

// Driver provides access to the Intel experimental (Exp) Sysman API
// functionality. The stable driver API is available through the embedded sysman.Driver.
type Driver struct {
	*sysman.Driver
	// handle is the Level Zero handle of the embedded sysman.Driver
	handle driverHandle

	// State of the extensions, tracked separately as the driver may implement only a subset
	infoLog     extensionState
	driverEvent extensionState
}

// extensionState caches the result of resolving the functions of one extension.
type extensionState struct {
	resolved bool
	ret      core.Result
}

// NewDriver returns a Driver wrapping a sysman.Driver instance, giving access
// to the Intel experimental (Exp) Sysman API functionality.
func NewDriver(sysDriver *sysman.Driver) *Driver {
	if sysDriver == nil {
		return nil
	}
	return &Driver{Driver: sysDriver, handle: driverHandle(sysDriver.Handle())}
}

// InfoLog provides access to the Intel experimental (Exp) info log Sysman API
// functionality. Instances are obtained from Driver.EnumInfoLogs.
//
// NOTE: the info log API is not thread-safe. Driver.EnumInfoLogs and the methods
// of an InfoLog must be called from a single thread (or process), and the
// listening for the CPER_DATA_AVAILABLE event must be serialized with them on
// that same thread: they all operate on the same underlying data source of the
// driver.
type InfoLog struct {
	handle infoLogHandle
}

// InfoLogRecord is one info log record together with its metadata, as returned
// by InfoLog.ReadWithMetadata.
type InfoLogRecord struct {
	// Metadata describes the record, e.g. the device that it originates from
	Metadata InfoLogMetadataExp
	// Data is the record itself, in the format that the InfoLogFormat property of
	// the info log reports
	Data []byte
}
