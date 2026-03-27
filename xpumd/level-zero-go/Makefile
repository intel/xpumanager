#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT

Q := @
PACKAGES := core sysman

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
generate: clean generate-stub
	go mod download
	$(Q)for pkg in $(PACKAGES); do \
	    hack/generate.sh $$pkg || exit 1; \
	done

.PHONY: generate-stub
generate-stub:
	$(MAKE) -C level-zero-stub generate

.PHONY: generate-dockerized
generate-dockerized: clean
	$(Q)docker build --target artifacts \
	    --output type=local,dest=. \
	    --build-arg PACKAGES="$(PACKAGES)" \
	    -f hack/Dockerfile.generate .

.PHONY: clean
clean: clean-coverage
	for pkg in $(PACKAGES); do \
	    rm -f $$pkg/cgo_helpers.go $$pkg/cgo_helpers.h $$pkg/cgo_helpers.c; \
	    rm -f $$pkg/const.go $$pkg/doc.go $$pkg/types.go; \
	    rm -f $$pkg/$$pkg.go; \
	    rm -f $$pkg/zz_*; \
	done

# --- Test targets ---

.PHONY: test
test: test-stub test-go

.PHONY: test-stub
test-stub:
	$(MAKE) -C level-zero-stub test

.PHONY: test-go
test-go:
	PKG_CONFIG_PATH=$(CURDIR)/level-zero-stub go test -tags test -count=1 -coverprofile=coverage.out ./...
	go tool cover -func=coverage.out | tail -1

.PHONY: coverage
coverage: test-go
	go tool cover -html=coverage.out -o coverage.html
	@echo "Coverage report: coverage.html"

.PHONY: clean-coverage
clean-coverage:
	rm -f coverage.out coverage.html

# --- Lint targets ---

.PHONY: golint
# same as in "ci-checks.yml" GH workflow
# "label=disable" is needed for host volume mounts on hosts using Podman + SELinux
golint:
	@echo -e "\ngolint: linting Go code...\n"
	docker run --rm -v $$PWD:/app -w /app --security-opt label=disable \
	  -e PKG_CONFIG_PATH=/app/level-zero-stub \
	  golangci/golangci-lint:v2.6.2 golangci-lint run --color=always

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
