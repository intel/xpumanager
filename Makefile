#
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: Apache-2.0

all: build

build:
	go build -o bin/xpu-exporter ./cmd/xpu-exporter

clean:
	rm -rf bin

lint:
	go tool golangci-lint run
