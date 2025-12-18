#
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: Apache-2.0

OUT_DIR ?= bin

.PHONY: all
all: build

.PHONY: build
build:
	go build -o $(OUT_DIR)/xpu-exporter ./cmd/xpu-exporter

.PHONY: test
test:
	go test ./...

.PHONY: lint
lint:
	go tool -modfile tools/go.mod golangci-lint run

.PHONY: generate
generate:
	scripts/generate.sh
