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

# --- Helper targets ---

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

.PHONY: generate
generate: clean-generated generate-stub install-doxygen
	go mod download
	$(Q)for pkg in $(PACKAGES); do \
	    hack/generate.sh $$pkg || exit 1; \
	done
	go -C hack generate

.PHONY: install-doxygen
install-doxygen:
	hack/install-doxygen.sh

.PHONY: generate-stub
generate-stub:
	$(MAKE) -C level-zero-stub generate

.PHONY: generate-dockerized
generate-dockerized: clean-generated
	$(Q)docker build --target artifacts \
	    --output type=local,dest=. \
	    --build-arg PACKAGES="$(PACKAGES)" \
	    -f hack/Dockerfile.generate .

.PHONY: clean-generated
clean-generated:
	for pkg in $(PACKAGES); do \
	    rm -f $$pkg/cgo_helpers.go $$pkg/cgo_helpers.h $$pkg/cgo_helpers.c; \
	    rm -f $$pkg/const.go $$pkg/doc.go $$pkg/types.go; \
	    rm -f $$pkg/$$(basename $$pkg).go; \
	    rm -f $$pkg/zz_*; \
	done

.PHONY: clean
clean: clean-coverage
	$(MAKE) -C examples clean
	$(MAKE) -C level-zero-stub clean
	for pkg in $(PACKAGES); do rm -f $$pkg/*.o; done

# --- Test targets ---

.PHONY: test
test: test-stub test-go

.PHONY: test-stub
test-stub:
	$(MAKE) -C level-zero-stub test

.PHONY: test-go
test-go:
	@error=""; \
	for module in $(GO_MODULES); do \
		echo "Running tests in module '$$module'..."; \
		outdir=$$(realpath -m --relative-to=. $(COVERAGE_DIR)/$$module); \
		mkdir -p $$outdir; \
		# Use counter=1 to avoid caching test results, needed because of the stub driver as \
		# the Go test cache misses (at least) the following things: \
		# - PKG_CONFIG_PATH trick (a change in the stub driver is not tracked) \
		# - the testdata/ files are read by the stub driver (not the Go code) \
		PKG_CONFIG_PATH=$(CURDIR)/level-zero-stub go -C $$module test -tags test \
		  -count=1 -coverpkg=./... \
		  -coverprofile=$(CURDIR)/$$outdir/coverage.out ./... || error=1; \
		if summary=$$(go -C $$module tool cover -func=$(CURDIR)/$$outdir/coverage.out); then \
			echo "$$summary" | tail -1; \
		else \
			error=1; \
		fi; \
	done; \
	if [ -n "$$error" ]; then \
		exit 1; \
	fi

.PHONY: coverage
coverage: test-go
	@echo
	@for module in $(GO_MODULES); do \
		outdir=$$(realpath -m --relative-to=. $(COVERAGE_DIR)/$$module); \
		go -C $$module tool cover -html=$(CURDIR)/$$outdir/coverage.out \
		  -o $(CURDIR)/$$outdir/coverage.html || exit 1; \
		hack/coverage-report.sh $$module $$outdir/coverage.out || exit 1; \
	done

.PHONY: clean-coverage
clean-coverage:
	rm -rf $(COVERAGE_DIR)

# --- Lint targets ---

.PHONY: golint
# same as in "ci-checks.yml" GH workflow
# "label=disable" is needed for host volume mounts on hosts using Podman + SELinux
golint:
	@echo -e "\ngolint: linting Go code...\n"
	docker run --rm -v $$PWD:/app -w /app --security-opt label=disable \
	  -e PKG_CONFIG_PATH=/app/level-zero-stub \
	  golangci/golangci-lint:v2.6.2 golangci-lint run --color=always

.PHONY: check-format
check-format: check-go-format check-c-format
	$(MAKE) -C level-zero-stub check-format

.PHONY: check-c-format
check-c-format:
	@echo "Checking clang-format formatting..."
	@clang-format --dry-run --Werror $(C_SOURCES)

.PHONY: check-go-format
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
# <apt|dnf> install shellcheck
shellcheck:
	@echo -e "\nshellcheck: linting shell scripts...\n"
	git ls-files -z '*.sh' | xargs -0 shellcheck

.PHONY: yamllint
# <apt|dnf> install yamllint
yamllint:
	@echo -e "\nyamllint: linting YAML files...\n"
	git ls-files -z '*.yaml' '*.yml' | xargs -0 yamllint -d relaxed --no-warnings
