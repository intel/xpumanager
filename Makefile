#
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: Apache-2.0

OUT_DIR ?= bin

# --- Helper targets ---

.PHONY: help
help:
	@echo -e "\nTargets:\n$$(grep '^[#^:. ]*:' Makefile | sed -e 's/^/- /' -e 's/:$$//')\n"

.PHONY: stats
stats:
	@echo "Type:       Files:   Lines:"
	@for ext in $$(git ls-files | sed -e 's%.*/%%' -e 's/.*\././' | grep -F . | sort -u); do \
		files=$$(git ls-files "*$$ext" | wc -l); \
		lines=$$(git ls-files -z "*$$ext" | xargs -0 xargs cat | wc -l); \
		printf " %-10s %5d %8d\n" $$ext $$files $$lines; \
	done

# --- Build targets ---

.PHONY: build
build:
	go build -o $(OUT_DIR)/xpu-exporter ./cmd/xpu-exporter

.PHONY: generate
generate:
	scripts/generate.sh

# --- Test / lint targets

.PHONY: test
test:
	go test ./...

.PHONY: golint
golint:
	go tool -modfile tools/go.mod golangci-lint run

.PHONY: shellcheck
# <apt|dnf> install shellcheck
shellcheck:
	@echo -e "\nshellcheck: linting shell scripts...\n"
	git ls-files -z '*.sh' | xargs -0 shellcheck
