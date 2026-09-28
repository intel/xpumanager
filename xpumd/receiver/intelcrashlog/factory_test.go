//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package intelcrashlog

import (
	"context"
	"testing"

	"github.com/stretchr/testify/assert"
	"go.opentelemetry.io/collector/consumer/consumertest"
	"go.opentelemetry.io/collector/receiver/receivertest"
)

func TestCreateLogsReceiverInvalidConfigType(t *testing.T) {
	_, err := createLogsReceiver(context.Background(), receivertest.NewNopSettings(typ),
		&struct{}{}, consumertest.NewNop())

	assert.ErrorContains(t, err, "invalid config type")
}
