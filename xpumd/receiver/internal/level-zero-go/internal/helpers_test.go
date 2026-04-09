// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT
//

package internal

import (
	"testing"

	"github.com/stretchr/testify/assert"
)

func TestFlagsToBits(t *testing.T) {
	t.Run("zero returns empty slice", func(t *testing.T) {
		result := FlagsToBits(uint32(0))
		assert.Empty(t, result)
	})

	t.Run("single bit uint32", func(t *testing.T) {
		result := FlagsToBits(uint32(0b0100))
		assert.Equal(t, []uint32{0b0100}, result)
	})

	t.Run("multiple bits uint32", func(t *testing.T) {
		// bits 0, 2, 4 set → values 1, 4, 16
		result := FlagsToBits(uint32(0b10101))
		assert.Equal(t, []uint32{1, 4, 16}, result)
	})

	t.Run("all bits in a byte uint32", func(t *testing.T) {
		result := FlagsToBits(uint32(0xFF))
		assert.Equal(t, []uint32{1, 2, 4, 8, 16, 32, 64, 128}, result)
	})

	t.Run("bits returned LSB first", func(t *testing.T) {
		result := FlagsToBits(uint32(0b110))
		assert.Equal(t, []uint32{2, 4}, result)
	})

	t.Run("single bit uint64", func(t *testing.T) {
		result := FlagsToBits(uint64(1) << 32)
		assert.Equal(t, []uint64{1 << 32}, result)
	})

	t.Run("multiple bits uint64", func(t *testing.T) {
		result := FlagsToBits(uint64(0b1001))
		assert.Equal(t, []uint64{1, 8}, result)
	})
}
