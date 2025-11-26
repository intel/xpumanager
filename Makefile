#
# Copyright (C) 2019-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT

Q := @
PACKAGES := core levelzero sysman

all: build

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
