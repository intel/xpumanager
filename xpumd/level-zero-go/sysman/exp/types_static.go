// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT
//

package exp

import (
	"github.com/intel/level-zero-go/sysman"
)

// Ras provides access to the experimental (Exp) RAS Sysman API functionality.
// The stable RAS API is available through the embedded sysman.Ras.
type Ras struct {
	*sysman.Ras
	// handle is the Level Zero handle of the embedded sysman.Ras, re-typed for
	// this package (cgo types are package-specific).
	handle rasHandle
}

// NewRas returns a Ras wrapping a sysman.Ras instance, giving
// access to the experimental (Exp) RAS Sysman API functionality.
// Returns nil if sysRas is nil.
func NewRas(sysRas *sysman.Ras) *Ras {
	if sysRas == nil {
		return nil
	}
	return &Ras{Ras: sysRas, handle: rasHandle(sysRas.Handle())}
}
