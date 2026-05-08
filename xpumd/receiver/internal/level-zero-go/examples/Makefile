#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT

Q := @

IMAGE_REPO_BASE ?= registry.local/level-zero-go
IMAGE_TAG ?= latest
EXAMPLE_APPS := list-sysman events-sysman

# --- Helper targets ---

.PHONY: help
help:
	@echo -e "\nTargets:\n$$(grep '^[^#:. ]*:' Makefile | sed -e 's/^/- /' -e 's/:$$//')\n"

# --- Build targets ---

.PHONY: build
build:
	$(Q)go build -o bin/list-sysman ./list-sysman/.
	$(Q)go build -o bin/events-sysman ./events-sysman/.

.PHONY: images
# "label=disable" is needed for Dockerfile "RUN --mount" on hosts using Podman + SELinux
images:
	$(Q)for app in $(EXAMPLE_APPS); do \
	    docker build -t $(IMAGE_REPO_BASE)/$$app:$(IMAGE_TAG) \
	                 --build-arg APP=$$app \
	                 --security-opt label=disable \
	                 --progress=plain \
	                 -f Dockerfile .. ; \
	done
