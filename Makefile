#
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: Apache-2.0

# --- Helper targets ---

# Skip the main module at the repo root as it's currently empty and makes some checks to fail.
GO_MODULES := $(shell git ls-files '*/go.mod' | xargs dirname)

.PHONY: help
help:
	@echo -e "\nTargets:\n$$(grep '^[^#:. ]*:' Makefile | sed -e 's/^/- /' -e 's/:$$//')\n"

.PHONY: stats
stats:
	@echo "Type:       Files:   Lines:"
	@for ext in $$(git ls-files | sed -e 's%.*/%%' -e 's/.*\././' | grep -F . | sort -u); do \
		files=$$(git ls-files "*$$ext" | wc -l); \
		lines=$$(git ls-files -z "*$$ext" | xargs -0 xargs cat | wc -l); \
		printf " %-10s %5d %8d\n" $$ext $$files $$lines; \
	done

# --- Build targets ---

OTEL_VERSION ?= 0.145.0

.PHONY: build
build:
	sed 's/@OTEL_VERSION@/$(OTEL_VERSION)/g' builder-config.yaml.in > builder-config.yaml
	go tool -modfile tools/go.mod builder --config=builder-config.yaml
	go -C dist build github.com/intel/xpumanager/exporter/xpuinfo-cli

.PHONY: generate
generate: generate-proto generate-mods

.PHONY: generate-mods
generate-mods:
	scripts/generate.sh $(GO_MODULES)

.PHONY: generate-proto
generate-proto: install-protoc
	PATH=tools/bin/ protoc --go_out=. --go_opt=paths=source_relative \
	                       --go-grpc_out=. --go-grpc_opt=paths=source_relative \
	                       exporter/api/deviceinfo/v1alpha1/deviceinfo.proto

.PHONY: install-protoc
install-protoc:
	scripts/install-protoc.sh

# --- Test / lint targets

.PHONY: test
test:
	@error=""; for module in $(GO_MODULES); do \
		echo "Running tests in module '$$module'..."; \
		go -C $$module test ./... || error=1; \
	done; \
	if [ -n "$$error" ]; then \
		exit 1; \
	fi; \
	echo "✓ All tests passed"

.PHONY: golint
golint:
	@error=""; for module in $(GO_MODULES); do \
		echo "Running golangci-lint in module '$$module'..."; \
		go -C $$module tool -modfile $(PWD)/tools/go.mod golangci-lint run || error=1; \
	done; \
	if [ -n "$$error" ]; then \
		exit 1; \
	fi; \
	echo "✓ No linter issues found"

.PHONY: shellcheck
# <apt|dnf> install shellcheck
shellcheck:
	@echo -e "\nshellcheck: linting shell scripts...\n"
	git ls-files -z '*.sh' | xargs -0 shellcheck

.PHONY: check-modfiles
check-modfiles:
	@echo "Verifying go.mod files..."
	@error=""; for module in $(GO_MODULES); do \
		echo "Checking module '$$module'"; \
		if ! (cd $$module && go mod tidy -diff 2> /dev/null); then \
			echo "ERROR: go.mod file in '$$module' is out of sync. Run 'go mod tidy' in that directory to fix it."; \
			error=1; \
		fi; \
	done; \
	if [ -n "$$error" ]; then \
		exit 1; \
	fi; \
	echo "✓ All go.mod files are in sync"
