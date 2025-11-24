#
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: Apache-2.0

OUT_DIR ?= bin

all: build

build:
	go build -o $(OUT_DIR)/xpu-exporter ./cmd/xpu-exporter

lint:
	go tool golangci-lint run

generate:
	scripts/generate.sh
