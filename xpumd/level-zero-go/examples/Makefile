#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT

Q := @

IMAGE_REPO_BASE ?= registry.local/level-zero-go
IMAGE_TAG ?= latest
EXAMPLE_APPS := list-sysman list-sysman-exp events-sysman
BIN_DIR ?= bin

# PIE + full RELRO, drop all symbols
GO_FLAGS := -buildmode=pie --ldflags="-bindnow -s -w"

# Use a dedicated GOCACHE for stub builds.
# Go build cache does not distinguish between PKG_CONFIG_PATH values (set from env) so this
# would otherwise "pollute" the cache for the real driver build.
BUILD_STUB_ENV := PKG_CONFIG_PATH=$(CURDIR)/../level-zero-stub GOCACHE=$(CURDIR)/.gocache-stub

# --- Helper targets ---

.PHONY: help
help:
	@echo -e "\nTargets:\n$$(grep '^[^#:. ]*:' Makefile | sed -e 's/^/- /' -e 's/:$$//')\n"

# --- Build targets ---

BUILD_TARGETS := $(addprefix build-,$(EXAMPLE_APPS))

.PHONY: build $(BUILD_TARGETS)
build: $(BUILD_TARGETS)

.PHONY: build-list-sysman
build-list-sysman:
	$(Q)go build $(GO_FLAGS) -o $(BIN_DIR)/list-sysman ./list-sysman/.

.PHONY: build-list-sysman-exp
build-list-sysman-exp:
	$(Q)go build $(GO_FLAGS) -tags exp -o $(BIN_DIR)/list-sysman-exp ./list-sysman/.

.PHONY: build-events-sysman
build-events-sysman:
	$(Q)go build $(GO_FLAGS) -o $(BIN_DIR)/events-sysman ./events-sysman/.

.PHONY: build-stub
build-stub:
	$(Q)$(BUILD_STUB_ENV) go build $(GO_FLAGS) -o bin/list-sysman-stub ./list-sysman/.
	$(Q)$(BUILD_STUB_ENV) go build $(GO_FLAGS) -o bin/events-sysman-stub ./events-sysman/.
	@echo "Give stub driver config with SYSMAN_STUB_CONFIG, for example:"
	@echo "  SYSMAN_STUB_CONFIG=../level-zero-stub/example-config.yaml ./bin/list-sysman-stub"
	@echo ""

.PHONY: images
# "label=disable" is needed for Dockerfile "RUN --mount" on hosts using Podman + SELinux
images:
	$(Q)for app in $(EXAMPLE_APPS); do \
	    docker build -t $(IMAGE_REPO_BASE)/$$app:$(IMAGE_TAG) \
	                 --build-arg APP=$$app \
	                 --security-opt label=disable \
	                 --progress=plain \
	                 -f Dockerfile .. || exit 1; \
	done

.PHONY: clean
clean:
	rm -f bin/*
	rm -rf .gocache-stub
