#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT

Q := @

IMAGE_REPO ?= registry.local/level-zero-go/list-sysman
IMAGE_TAG ?= main

# --- Helper targets ---

.PHONY: help
help:
	@echo -e "\nTargets:\n$$(grep '^[^#:. ]*:' Makefile | sed -e 's/^/- /' -e 's/:$$//')\n"

# --- Build targets ---

.PHONY: build
build:
	$(Q)go build -o bin/list-sysman ./list-sysman/.

.PHONY: images
images:
	$(Q)docker build -t $(IMAGE_REPO):$(IMAGE_TAG) \
	             --progress=plain \
	             -f list-sysman/Dockerfile ..
