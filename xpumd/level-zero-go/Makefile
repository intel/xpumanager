#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT

Q := @
PACKAGES := core sysman sysman/exp sysman/exp/intel

# Hand-written C sources of the Go packages, i.e. excluding the vendored
# headers, the generated cgo helpers and level-zero-stub (which has its own
# formatting targets).
C_SOURCES := $(shell git ls-files '*.c' '*.h' | \
	     grep -v -e '^level-zero/' -e '^include/' -e '^level-zero-stub/' -e '/cgo_helpers\.')

GO_MODULES := $(shell (git ls-files 'go.mod' '*/go.mod' 2> /dev/null || find . -name go.mod -printf '%P\n') | xargs dirname)

COVERAGE_DIR := coverage
# Extra coverage profiles to merge into the 'coverage' report.
COVERAGE_TO_MERGE ?=

# --- Helper targets ---

.PHONY: help
# List all targets, grouped by section, with their documentation
help:
	@awk -f hack/makefile-help.awk Makefile

.PHONY: stats
# Print per-filetype file / line counts of tracked files
stats:
	@echo "Type:       Files:   Lines:"
	@for ext in $$(git ls-files | sed -e 's%.*/%%' -e 's/.*\././' | grep -F . | sort -u); do \
		files=$$(git ls-files "*$$ext" | wc -l); \
		lines=$$(git ls-files -z "*$$ext" | xargs -0 xargs cat | wc -l); \
		printf " %-10s %5d %8d\n" $$ext $$files $$lines; \
	done

# --- Build targets ---

.PHONY: generate
# Generate Go bindings from the Level-Zero headers, and the stub driver
generate: clean-generated generate-stub install-doxygen
	go mod download
	$(Q)for pkg in $(PACKAGES); do \
	    hack/generate.sh $$pkg || exit 1; \
	done
	go -C hack generate

.PHONY: install-doxygen
# Install Doxygen (needed for code generation)
install-doxygen:
	hack/install-doxygen.sh

.PHONY: generate-stub
# Generate the level-zero-stub driver sources
generate-stub:
	$(MAKE) -C level-zero-stub generate

.PHONY: build-stub
# Build the level-zero-stub driver
build-stub:
	$(MAKE) -C level-zero-stub all

.PHONY: generate-dockerized
# Run code generation in Docker, for reproducible output
generate-dockerized: clean-generated
	$(Q)docker build --target artifacts \
	    --output type=local,dest=. \
	    --build-arg PACKAGES="$(PACKAGES)" \
	    -f hack/Dockerfile.generate .

.PHONY: clean-generated
# Remove generated source files
clean-generated:
	for pkg in $(PACKAGES); do \
	    rm -f $$pkg/cgo_helpers.go $$pkg/cgo_helpers.h $$pkg/cgo_helpers.c; \
	    rm -f $$pkg/const.go $$pkg/doc.go $$pkg/types.go; \
	    rm -f $$pkg/$$(basename $$pkg).go; \
	    rm -f $$pkg/zz_*; \
	done

.PHONY: clean
# Remove generated build artifacts
clean: clean-coverage
	$(MAKE) -C examples clean
	$(MAKE) -C level-zero-stub clean
	for pkg in $(PACKAGES); do rm -f $$pkg/*.o; done

# --- Test targets ---

.PHONY: test
# Run all tests (stub driver + Go)
test: test-stub test-go

.PHONY: test-stub
# Run the level-zero-stub driver's own test suite
test-stub: build-stub
	$(MAKE) -C level-zero-stub test

.PHONY: test-go
# Run Go unit tests against the stub driver
test-go: build-stub
	$(Q)GO_TEST_FLAGS="-tags test" \
	  PKG_CONFIG_PATH=$(CURDIR)/level-zero-stub \
	  hack/go-test-coverage.sh $(COVERAGE_DIR)/modules/level-zero-go $(GO_MODULES)

.PHONY: coverage
# Generate a Go test coverage report
coverage: test-go
	@echo
	$(Q)hack/coverage-report.sh $(COVERAGE_DIR)/modules/level-zero-go $(COVERAGE_TO_MERGE)

.PHONY: clean-coverage
# Remove coverage report files
clean-coverage:
	rm -rf $(COVERAGE_DIR)

# --- Lint targets ---

# Same as in "ci-checks.yml" GH workflow.
# "label=disable" is needed for host volume mounts on hosts using Podman + SELinux.
.PHONY: golint
# Run golangci-lint on all Go code
golint:
	@echo -e "\ngolint: linting Go code...\n"
	docker run --rm -v $$PWD:/app -w /app --security-opt label=disable \
	  -e PKG_CONFIG_PATH=/app/level-zero-stub \
	  golangci/golangci-lint:v2.13.2 golangci-lint run --color=always

.PHONY: check-format
# Check formatting of all sources
check-format: check-go-format check-c-format
	$(MAKE) -C level-zero-stub check-format

.PHONY: check-c-format
# Check clang-format formatting of C sources
check-c-format:
	@echo "Checking clang-format formatting..."
	@clang-format --dry-run --Werror $(C_SOURCES)

.PHONY: check-go-format
# Check gofmt formatting of sources
check-go-format:
	@echo "Checking gofmt formatting..."
	@unformatted=$$(gofmt -l .); \
	if [ -n "$$unformatted" ]; then \
		echo "ERROR: The following files are not gofmt formatted:"; \
		echo "$$unformatted"; \
		echo ""; \
		echo "Run 'gofmt -w' on them to fix."; \
		exit 1; \
	fi; \
	echo "✓ All Go files are properly formatted"

.PHONY: shellcheck
# Lint shell scripts (requires: <apt|dnf> install shellcheck)
shellcheck:
	@echo -e "\nshellcheck: linting shell scripts...\n"
	git ls-files -z '*.sh' | xargs -0 shellcheck

.PHONY: yamllint
# Lint YAML files (requires: <apt|dnf> install yamllint)
yamllint:
	@echo -e "\nyamllint: linting YAML files...\n"
	git ls-files -z '*.yaml' '*.yml' | xargs -0 yamllint -d relaxed --no-warnings
