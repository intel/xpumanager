// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT
//

package exp

import (
	"testing"

	"github.com/intel/level-zero-go/core"
	th "github.com/intel/level-zero-go/internal/testhelper"
	"github.com/intel/level-zero-go/sysman"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

// Driver config paths (relative to the sysman/exp/ package directory, i.e.
// go test CWD).
const (
	driverConfigDefault       = th.ConfigDefault
	driverConfigNoExtension   = "testdata/no_extension.yaml"
	driverConfigComponentErrs = "testdata/error_component.yaml"
)

// getRas returns an exp.Ras wrapping drivers[drvIdx].devices[devIdx]'s RAS
// error set at the given index, from the currently-loaded stub state.
func getRas(t *testing.T, drvIdx, devIdx, idx int) *Ras {
	t.Helper()
	driver := th.GetIndexed(t, "driver", drvIdx, sysman.DriverGet)
	device := th.GetIndexed(t, "device", devIdx, driver.DeviceGet)
	return NewRas(th.GetIndexed(t, "component", idx, device.EnumRasErrorSets))
}

// rasGetter returns the RAS error set under test, as addressed by the test case
// config.
func rasGetter(cfg *th.Config) func(*testing.T) *Ras {
	return func(t *testing.T) *Ras {
		t.Helper()
		return getRas(t, cfg.DrvIdx, cfg.DevIdx, cfg.CompIdx)
	}
}

func testRasGetterError[R any](t *testing.T, method func(*Ras) (R, error), opts ...th.Opt) {
	t.Helper()
	cfg := th.NewConfig("Error", core.RESULT_ERROR_INSUFFICIENT_PERMISSIONS, opts...)
	th.Getter(t, cfg, rasGetter(cfg), method, nil)
}

func testRasGetterSuccess[R any](t *testing.T, method func(*Ras) (R, error), check func(*testing.T, R), opts ...th.Opt) {
	t.Helper()
	cfg := th.NewConfig("Success", nil, opts...)
	th.Getter(t, cfg, rasGetter(cfg), method, check)
}

func testRasActionError(t *testing.T, action func(*Ras) error, opts ...th.Opt) {
	t.Helper()
	cfg := th.NewConfig("Error", core.RESULT_ERROR_INSUFFICIENT_PERMISSIONS, opts...)
	th.Action(t, cfg, rasGetter(cfg), action)
}

func TestNewRas(t *testing.T) {
	assert.Nil(t, NewRas(nil))
}

func TestRasGetStateExp(t *testing.T) {
	testRasGetterError(t, (*Ras).GetStateExp, th.WithConfig(driverConfigComponentErrs))
	testRasGetterSuccess(t, (*Ras).GetStateExp,
		th.CheckValue([]RasStateExp{{
			Category:     RAS_ERROR_CATEGORY_EXP_MEMORY_ERRORS,
			ErrorCounter: 1,
		}, {
			Category:     RAS_ERROR_CATEGORY_EXP_L3FABRIC_ERRORS,
			ErrorCounter: 2,
		}}),
		th.WithName("SuccessCorrectable"),
	)
	testRasGetterSuccess(t, (*Ras).GetStateExp,
		th.CheckValue([]RasStateExp{{
			Category:     RAS_ERROR_CATEGORY_EXP_RESET,
			ErrorCounter: 3,
		}, {
			Category:     RAS_ERROR_CATEGORY_EXP_PROGRAMMING_ERRORS,
			ErrorCounter: 4,
		}}),
		th.WithName("SuccessUncorrectable"), th.WithCompIdx(1),
	)
	// no experimental API, the legacy fallback fails
	testRasGetterError(t, (*Ras).GetStateExp,
		th.WithName("ErrorNoExtension"), th.WithConfig(driverConfigNoExtension), th.WithCompIdx(1),
	)
	// no experimental API, legacy state converted to the Exp categories
	expected := make([]RasStateExp, sysman.MAX_RAS_ERROR_CATEGORY_COUNT)
	for cat := range expected {
		expected[cat] = RasStateExp{
			Category:     RasErrorCategoryExp(cat),
			ErrorCounter: uint64(cat) + 1,
		}
	}
	testRasGetterSuccess(t, (*Ras).GetStateExp, th.CheckValue(expected),
		th.WithName("SuccessNoExtension"), th.WithConfig(driverConfigNoExtension),
	)
}

func TestRasClearStateExp(t *testing.T) {
	clearFirst := func(z *Ras) error { return z.ClearStateExp(0) }
	testRasActionError(t, clearFirst, th.WithConfig(driverConfigComponentErrs))
	// no legacy fallback for clearing a single error category
	testRasActionError(t, clearFirst,
		th.WithName("ErrorNoExtension"),
		th.WithConfig(driverConfigNoExtension),
		th.WithError(core.RESULT_ERROR_UNSUPPORTED_FEATURE),
	)
	t.Run("Success", func(t *testing.T) {
		th.LoadConfig(t, driverConfigDefault)
		// uncorrectable
		states, err := getRas(t, 0, 0, 1).GetStateExp()
		require.NoError(t, err)
		assert.EqualExportedValues(t, []RasStateExp{{
			Category:     RAS_ERROR_CATEGORY_EXP_RESET,
			ErrorCounter: 3,
		}, {
			Category:     RAS_ERROR_CATEGORY_EXP_PROGRAMMING_ERRORS,
			ErrorCounter: 4,
		}}, states)
		// single counter reset
		err = getRas(t, 0, 0, 1).ClearStateExp(RAS_ERROR_CATEGORY_EXP_RESET)
		require.NoError(t, err)
		// just that zeroed?
		states, err = getRas(t, 0, 0, 1).GetStateExp()
		require.NoError(t, err)
		assert.EqualExportedValues(t, []RasStateExp{{
			Category:     RAS_ERROR_CATEGORY_EXP_RESET,
			ErrorCounter: 0,
		}, {
			Category:     RAS_ERROR_CATEGORY_EXP_PROGRAMMING_ERRORS,
			ErrorCounter: 4,
		}}, states)
	})
}
