#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT

# --- Helper targets ---

# Skip the main module at the repo root as it's currently empty and makes some checks to fail.
GO_MODULES := $(shell (git ls-files '*/go.mod' 2> /dev/null || find -mindepth 2 -name go.mod -printf '%P\n') | xargs dirname)
LEVEL_ZERO_GO_MODULES := $(foreach m,$(GO_MODULES),$(if $(findstring level-zero-go,$(m)),$(m)))
# Filter out level-zero-go and test/
XPUMD_GO_MODULES := $(filter-out $(LEVEL_ZERO_GO_MODULES) $(filter test/%,$(GO_MODULES)),$(GO_MODULES))

WHAT ?=

.PHONY: help
# List all targets, grouped by section, with their documentation
help:
	@awk -f level-zero-go/hack/makefile-help.awk Makefile

.PHONY: stats
# Print per-filetype file / line counts of tracked files for XPUMD + L0 Go binding
stats:
	@echo "Type:           Files:   Lines:"
	@for ext in $$(git ls-files | sed -e 's%.*/%%' -e 's/.*\././' | grep -F . | sort -u); do \
		files=$$(git ls-files "*$$ext" | wc -l); \
		lines=$$(git ls-files -z "*$$ext" | xargs -0 xargs cat | wc -l); \
		printf " %-14s %5d %8d\n" $$ext $$files $$lines; \
	done

# --- Build targets ---

OTEL_VERSION ?= 0.161.0
IMAGE_REPOSITORY ?= registry.local/xpumd
IMAGE_TAG ?= latest
IMAGE_PULL_POLICY ?= IfNotPresent
# Where the image gets the Level-Zero GPU backend from: "deb" or "src" (see Dockerfile)
BACKEND ?= deb

# PIE + full RELRO, drop all symbols
GO_FLAGS := -buildmode=pie -bindnow -s -w

# Use a dedicated GOCACHE for stub builds.
# Go build cache does not distinguish between PKG_CONFIG_PATH values (set from env) so this
# would otherwise "pollute" the cache for the real driver build.
BUILD_STUB_ENV := PKG_CONFIG_PATH=$(CURDIR)/level-zero-go/level-zero-stub GOCACHE=$(CURDIR)/.gocache-stub

# Packages to instrument for coverage.
XPUMD_PKG := $(shell go list -m)
LEVEL_ZERO_GO_PKG := $(shell go -C level-zero-go list -m)
GO_COVER_FLAGS := -cover -coverpkg=$(XPUMD_PKG)/...,$(LEVEL_ZERO_GO_PKG)/...

.PHONY: builder-config.yaml
builder-config.yaml:
	sed 's/@OTEL_VERSION@/$(OTEL_VERSION)/g' builder-config.yaml.in > builder-config.yaml

.PHONY: build
# Build xpumd + xpuinfo-cli
build: builder-config.yaml
	go tool -modfile tools/go.mod builder --ldflags "$(GO_FLAGS)" --config=builder-config.yaml
	go -C dist build --ldflags "$(GO_FLAGS)" github.com/intel/xpumanager/xpumd/exporter/intelxpuinfo/xpuinfo-cli

.PHONY: level-zero-stub
# Build the Level-Zero stub driver
level-zero-stub:
	$(MAKE) -C level-zero-go build-stub

.PHONY: build-stub
# Build xpumd + xpuinfo-cli against the level-zero-stub driver
build-stub: level-zero-stub builder-config.yaml
	sed -i 's#output_path: ./dist#output_path: ./dist-stub#' builder-config.yaml
	$(BUILD_STUB_ENV) go tool -modfile tools/go.mod builder --ldflags "$(GO_FLAGS)" --config=builder-config.yaml
	$(BUILD_STUB_ENV) go -C dist-stub build --ldflags "$(GO_FLAGS)" github.com/intel/xpumanager/xpumd/exporter/intelxpuinfo/xpuinfo-cli
	@echo "Give stub driver config with SYSMAN_STUB_CONFIG, for example:"
	@echo "  SYSMAN_STUB_CONFIG=level-zero-go/level-zero-stub/example-config.yaml ./dist-stub/xpumd" --config config-example.yaml
	@echo ""

.PHONY: build-cover
# Build coverage-instrumented xpumd + xpuinfo-cli
build-cover: builder-config.yaml
	go tool -modfile tools/go.mod builder --skip-compilation --config=builder-config.yaml
	go -C dist build $(GO_COVER_FLAGS) --ldflags "$(GO_FLAGS)" -o xpumd .
	go -C dist build $(GO_COVER_FLAGS) --ldflags "$(GO_FLAGS)" github.com/intel/xpumanager/xpumd/exporter/intelxpuinfo/xpuinfo-cli

.PHONY: race
# Build race detector versions (incompatible with PIE mode)
# Log errors to file with: GORACE=log_path=xpumd-races
race: builder-config.yaml
	GOFLAGS=-race go tool -modfile tools/go.mod builder --config=builder-config.yaml
	go -C dist build -race github.com/intel/xpumanager/xpumd/exporter/intelxpuinfo/xpuinfo-cli

.PHONY: docker-build
# Build the xpumd container image
docker-build:
	docker build --build-arg BACKEND=$(BACKEND) -t $(IMAGE_REPOSITORY):$(IMAGE_TAG) .

.PHONY: docker-build-cover
# Build image with coverage-instrumented binaries
docker-build-cover:
	docker build --build-arg BACKEND=$(BACKEND) --build-arg BUILD_TARGET=build-cover \
	  -t $(IMAGE_REPOSITORY):$(IMAGE_TAG) .

.PHONY: generate
# Run all code generation
generate: generate-bindings generate-mods generate-proto generate-dockerfile-args

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
	                       exporter/intelxpuinfo/api/deviceinfo/v1alpha1/deviceinfo.proto

.PHONY: generate-dockerfile-args
# Sync the compute-runtime backend build args from the Level-Zero Go example Dockerfile
generate-dockerfile-args:
	scripts/sync-dockerfile-args.sh

.PHONY: install-protoc
# Install protoc (and needed plugins) under tools/
install-protoc:
	scripts/install-protoc.sh

.PHONY: clean
# Remove generated build artifacts
clean: clean-coverage
	$(MAKE) -C level-zero-go clean
	rm -f builder-config.yaml \
	  tools/include/google/protobuf/compiler/plugin.proto \
	  tools/include/google/protobuf/*.proto \
	  tools/bin/* \
	  dist/*
	rm -rf dist-stub .gocache-stub

# --- Test targets

COVERAGE_DIR := coverage

.PHONY: test
# Run all unit tests
test: test-daemon test-config test-bindings

.PHONY: test-daemon
# Run xpumd unit tests
# Select tests with WHAT=<regexp>
test-daemon: level-zero-stub
	@$(BUILD_STUB_ENV) GO_TEST_FLAGS="$(if $(WHAT),-run $(WHAT))" \
	  level-zero-go/hack/go-test-coverage.sh $(COVERAGE_DIR)/modules/xpumd $(XPUMD_GO_MODULES)
	@echo "✓ All tests passed"

.PHONY: coverage
# Generate a Go test coverage report of xpumd unit tests
coverage: test-daemon
	@echo
	@level-zero-go/hack/coverage-report.sh $(COVERAGE_DIR)/modules/xpumd

.PHONY: clean-coverage
# Remove coverage report files
clean-coverage:
	rm -rf $(COVERAGE_DIR)

.PHONY: test-bindings
# Run Level-Zero Go bindings unit tests
test-bindings:
	@echo "Running Level-Zero Go bindings tests..."
	make -C level-zero-go test

.PHONY: test-config
# Validate example config (and invalid test configs) with the built xpumd binary
test-config: build-stub
	scripts/run-config-tests.sh

.PHONY: test-integration-kind
# Run integration tests in an ephemeral kind cluster
# Select tests with WHAT=<regexp>
test-integration-kind: docker-build
	RUN_REGEX='$(WHAT)' scripts/run-integration-tests.sh \
		--image-repository=$(IMAGE_REPOSITORY) \
		--image-tag=$(IMAGE_TAG) \
		--kind-load-image

.PHONY: test-integration-existing-cluster
# Run integration tests in the currently configured cluster
# Select tests with WHAT=<regexp>
test-integration-existing-cluster:
	RUN_REGEX='$(WHAT)' scripts/run-integration-tests.sh \
		--image-repository=$(IMAGE_REPOSITORY) \
		--image-tag=$(IMAGE_TAG) \
		--use-existing-cluster \
		--image-pull-policy=$(IMAGE_PULL_POLICY)

.PHONY: coverage-integration-test-kind
# Measure integration test coverage in an ephemeral Kind cluster.
coverage-integration-test-kind: docker-build-cover
	TIMEOUT=20m scripts/run-integration-tests-coverage.sh \
		$(CURDIR)/$(COVERAGE_DIR)/covdata $(COVERAGE_DIR)/integration-tests.cov \
		--image-repository=$(IMAGE_REPOSITORY) \
		--image-tag=$(IMAGE_TAG) \
		--kind-load-image

.PHONY: coverage-integration-test-existing-cluster
# Measure integration test coverage in the currently configured cluster.
# Build the image with 'make docker-build-cover' first, and make sure it is reachable by the cluster.
# NOTE: not safe to run concurrently in the same cluster (coverage collection uses fixed hostPath mount and namespace).
coverage-integration-test-existing-cluster:
	TIMEOUT=20m scripts/run-integration-tests-coverage.sh \
		$(CURDIR)/$(COVERAGE_DIR)/covdata $(COVERAGE_DIR)/integration-tests.cov \
		--image-repository=$(IMAGE_REPOSITORY) \
		--image-tag=$(IMAGE_TAG) \
		--use-existing-cluster \
		--image-pull-policy=$(IMAGE_PULL_POLICY)

# --- Lint targets

.PHONY: lint
# Run all linters
lint: codespell golint shellcheck

.PHONY: codespell
# Run codespell on all sources (requires: <apt|dnf> install codespell)
codespell:
	@echo -e "Spellchecking other than generated & imported code..."
	codespell --config .codespellrc .

.PHONY: golint
# Run golangci-lint on all modules
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
# Lint shell scripts (requires: <apt|dnf> install shellcheck)
shellcheck:
	@echo -e "\nshellcheck: linting shell scripts...\n"
	git ls-files -z '*.sh' | xargs -0 shellcheck

.PHONY: check-format
# Check formatting of all sources
check-format: check-go-format
	$(MAKE) -C level-zero-go check-format

.PHONY: check-go-format
# Check gofmt formatting of sources
check-go-format:
	@echo "Checking gofmt formatting..."
	@unformatted=$$(gofmt -l . | grep -v '^dist/'); \
	if [ -n "$$unformatted" ]; then \
		echo "ERROR: The following files are not gofmt formatted:"; \
		echo "$$unformatted"; \
		echo ""; \
		echo "Run 'gofmt -w' on them to fix."; \
		exit 1; \
	fi; \
	echo "✓ All Go files are properly formatted"

.PHONY: check-modfiles
# Check that all go.mod files are tidy
check-modfiles:
	@echo "Verifying go.mod files..."
	@error=""; for module in $(GO_MODULES); do \
		echo "Checking module '$$module'"; \
		if ! (cd $$module && go mod tidy -diff 2> /dev/null); then \
			echo "ERROR: go.mod file in '$$module' is out of sync. Run 'make go-tidy' to fix it."; \
			error=1; \
		fi; \
	done; \
	if [ -n "$$error" ]; then \
		exit 1; \
	fi; \
	echo "✓ All go.mod files are in sync"

.PHONY: go-tidy
# Run 'go mod tidy' on all modules
go-tidy:
	for module in $(GO_MODULES); do \
		echo "Running go mod tidy in '$$module'"; \
		go -C $$module mod tidy || exit 1; \
	done; \

.PHONY: go-fix
# Run 'go fix' on all modules
go-fix:
	@for module in $(GO_MODULES); do \
		echo "Running go fix in '$$module'"; \
		go -C $$module fix ./... || exit 1; \
	done
