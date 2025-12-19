#
# Copyright (C) 2019-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT

Q := @
PACKAGES := core sysman

# --- Helper targets ---

.PHONY: help
help:
	@echo -e "\nTargets:\n$$(grep '^[-_a-z]*:' Makefile | sed -e 's/^/- /' -e 's/:$$//')\n"

.PHONY: stats
stats:
	@echo "Type:  Files:   Lines:"
	@for ext in $$(git ls-files | sed -e 's%.*/%%' -e 's/.*\././' | grep -F . | sort -u); do \
		files=$$(git ls-files "*$$ext" | wc -l); \
		lines=$$(git ls-files -z "*$$ext" | xargs -0 xargs cat | wc -l); \
		printf " %-5s %5d %8d\n" $$ext $$files $$lines; \
	done

# --- Build targets ---

.PHONY: generate
generate: clean
	go mod download
	$(Q)for pkg in $(PACKAGES); do \
	    hack/generate.sh $$pkg || exit 1; \
	done

.PHONY: generate-dockerized
generate-dockerized: clean
	$(Q)docker build --target artifacts \
	    --output type=local,dest=. \
	    --build-arg PACKAGES="$(PACKAGES)" \
	    -f hack/Dockerfile.generate .

.PHONY: clean
clean:
	for pkg in $(PACKAGES); do \
	    rm -f $$pkg/cgo_helpers.go $$pkg/cgo_helpers.h $$pkg/cgo_helpers.c; \
	    rm -f $$pkg/const.go $$pkg/doc.go $$pkg/types.go; \
	    rm -f $$pkg/$$pkg.go; \
	    rm -f $$pkg/zz_*; \
	done

# --- Lint targets ---

.PHONY: golint
# same as in "ci-checks.yml" GH workflow
# (label=disable is needed on hosts using SELinux)
golint:
	@echo -e "\ngolint: linting Go code...\n"
	docker run --rm -v $$PWD:/app -w /app --security-opt label=disable \
	  -e PKG_CONFIG_PATH=/app/hack \
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
