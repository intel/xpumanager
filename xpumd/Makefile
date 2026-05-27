#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT

# --- Helper targets ---

# Skip the main module at the repo root as it's currently empty and makes some checks to fail.
GO_MODULES := $(shell (git ls-files '*/go.mod' 2> /dev/null || find -mindepth 2 -name go.mod) | xargs dirname)
LEVEL_ZERO_GO_MODULES := $(foreach m,$(GO_MODULES),$(if $(findstring level-zero-go,$(m)),$(m)))
XPUMD_GO_MODULES := $(filter-out $(LEVEL_ZERO_GO_MODULES),$(GO_MODULES))

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

OTEL_VERSION ?= 0.153.0

.PHONY: build
# Build xpumd + xpuinfo-cli
build:
	sed 's/@OTEL_VERSION@/$(OTEL_VERSION)/g' builder-config.yaml.in > builder-config.yaml
	go tool -modfile tools/go.mod builder --config=builder-config.yaml
	go -C dist build github.com/intel/xpumanager/xpumd/exporter/xpuinfo-cli

.PHONY: generate
generate: generate-bindings generate-mods generate-proto

.PHONY: generate-bindings
# Generate Go bindings for Level-Zero
generate-bindings:
	make -C level-zero-go generate

.PHONY: generate-mods
# Generate indicated Go modules code + Helm docs / validation schema
generate-mods:
	scripts/generate.sh $(XPUMD_GO_MODULES)

.PHONY: generate-proto
# Generate health processor gRPC protocol implementation
generate-proto: install-protoc
	PATH=tools/bin/ protoc --go_out=. --go_opt=paths=source_relative \
	                       --go-grpc_out=. --go-grpc_opt=paths=source_relative \
	                       exporter/api/deviceinfo/v1alpha1/deviceinfo.proto

.PHONY: install-protoc
install-protoc:
	scripts/install-protoc.sh

# --- Test / lint targets

.PHONY: test
test: test-daemon test-bindings

.PHONY: test-daemon
test-daemon:
	@error=""; \
	for module in $(XPUMD_GO_MODULES); do \
		echo "Running tests in module '$$module'..."; \
		go -C $$module test ./... || error=1; \
	done; \
	if [ -n "$$error" ]; then \
		exit 1; \
	fi; \
	echo "✓ All tests passed"

.PHONY: test-bindings
test-bindings:
	@echo "Running Level-Zero Go bindings tests..."
	make -C level-zero-go test

.PHONY: golint
golint:
	@error=""; for module in $(GO_MODULES); do \
		echo "Running golangci-lint in module '$$module'..."; \
		go -C $$module tool -modfile $$(realpath --relative-to="$$module" tools/go.mod) golangci-lint run || error=1; \
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

.PHONY: go-fix
go-fix:
	@for module in $(GO_MODULES); do \
		echo "Running go fix in '$$module'"; \
		go -C $$module fix ./... || exit 1; \
	done
