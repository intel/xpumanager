//
// Copyright (C) 2025 Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0

package sysman

import (
	"fmt"
	"log/slog"
	"strconv"

	"go.opentelemetry.io/otel/attribute"
)

func subDeviceIdString(onSubdevice uint8, subdeviceId uint32) string {
	if onSubdevice != 0 {
		return strconv.Itoa(int(subdeviceId))
	}
	return ""
}

func attrsToSlog(attrs []attribute.KeyValue) slog.Attr {
	a := []any{}
	for _, attr := range attrs {
		a = append(a, slog.String(string(attr.Key), fmt.Sprintf("%v", attr.Value.AsInterface())))
	}
	return slog.Group("attributes", a...)
}
