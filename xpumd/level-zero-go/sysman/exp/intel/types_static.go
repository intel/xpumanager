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
// of an InfoLog and of an InfoLogInstance must be called from a single thread (or
// process). The listening for the CPER_DATA_AVAILABLE event must be serialized
// with the reading of the records on that same thread, too: the two operate on
// the same underlying trace buffer reader of the driver.
type InfoLog struct {
	handle infoLogHandle
}

// InfoLogInstance provides access to one collection instance of an info log.
// Instances are created with InfoLog.CreateInstance.
type InfoLogInstance struct {
	handle infoLogInstanceHandle

	// BufferSizeInKb is the total size of the collection buffer that the driver
	// applied for this instance, in kilobytes. Zero if the driver could not
	// determine it.
	BufferSizeInKb uint32
}

// InfoLogRecord is one info log record together with its metadata, as returned
// by InfoLogInstance.ReadWithMetadata and InfoLogInstance.PeekWithMetadata.
type InfoLogRecord struct {
	// Metadata describes the record, e.g. the device that it originates from
	Metadata InfoLogMetadataExp `yaml:"Metadata"`
	// Data is the record itself, in the format that the InfoLogFormat property of
	// the info log reports
	Data []byte `yaml:"Data"`
}
