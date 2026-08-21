// Copyright (C) 2026-2026 Intel Corporation
//
// SPDX-License-Identifier: MIT
//

package common

import "fmt"

// InfoLogFormat is the format that the info log records are dumped in, i.e. the
// value of the -infolog-format option. It implements flag.Value so that the
// unknown formats are rejected by flag.Parse.
type InfoLogFormat string

// The info log formats that InfoLogFormat accepts.
const (
	InfoLogFormatNone     InfoLogFormat = "none"
	InfoLogFormatMetadata InfoLogFormat = "metadata"
	InfoLogFormatRaw      InfoLogFormat = "raw"
)

// String implements flag.Value.
func (f InfoLogFormat) String() string {
	return string(f)
}

// Set implements flag.Value, accepting the known formats only.
func (f *InfoLogFormat) Set(value string) error {
	switch format := InfoLogFormat(value); format {
	case InfoLogFormatNone, InfoLogFormatMetadata, InfoLogFormatRaw:
		*f = format
		return nil
	}
	return fmt.Errorf("unknown info log format %q (use %s, %s or %s)",
		value, InfoLogFormatNone, InfoLogFormatMetadata, InfoLogFormatRaw)
}
