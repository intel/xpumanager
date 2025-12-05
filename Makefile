#
# Copyright (C) 2019-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT

Q := @
PACKAGES := core levelzero sysman

help:
	@echo -e "\nTargets:\n$$(grep '^[-_a-z]*:' Makefile | sed -e 's/^/- /' -e 's/:$$//')\n"

generate: clean
	go mod download
	$(Q)for pkg in $(PACKAGES); do \
	    hack/generate.sh $$pkg || exit 1; \
	done

generate-dockerized: clean
	$(Q)docker build --target artifacts \
	    --output type=local,dest=. \
	    --build-arg PACKAGES="$(PACKAGES)" \
	    -f hack/Dockerfile.generate .

clean:
	for pkg in $(PACKAGES); do \
	    rm -f $$pkg/cgo_helpers.go $$pkg/cgo_helpers.h $$pkg/cgo_helpers.c; \
	    rm -f $$pkg/const.go $$pkg/doc.go $$pkg/types.go; \
	    rm -f $$pkg/$$pkg.go; \
	    rm -f $$pkg/zz_*; \
	done

# same as in "ci-checks.yml" GH workflow
# (label=disable is needed on hosts using SELinux)
golint:
	@echo -e "\ngolint: linting Go code...\n"
	docker run --rm -v $$PWD:/app -w /app --security-opt label=disable \
	  golangci/golangci-lint:v2.6.2 golangci-lint run

# <apt|dnf> install shellcheck
shellcheck:
	@echo -e "\nshellcheck: linting shell scripts...\n"
	git ls-files -z '*.sh' | xargs -0 shellcheck

# <apt|dnf> install yamllint
yamllint:
	@echo -e "\nyamllint: linting YAML files...\n"
	git ls-files -z '*.yaml' '*.yml' | xargs -0 yamllint -d relaxed --no-warnings

stats:
	@echo "Type:  Files:   Lines:"
	@for ext in $$(git ls-files | sed -e 's%.*/%%' -e 's/.*\././' | grep -F . | sort -u); do \
		files=$$(git ls-files "*$$ext" | wc -l); \
		lines=$$(git ls-files -z "*$$ext" | xargs -0 xargs cat | wc -l); \
		printf " %-5s %5d %8d\n" $$ext $$files $$lines; \
	done

.PHONY: help generate generate-dockerized clean \
	golint shellcheck yamllint stats
