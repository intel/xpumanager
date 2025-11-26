#
# Copyright (C) 2019-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT

Q := @

all: build

generate: clean
	$(Q)hack/generate.sh levelzero

generate-dockerized: clean
	$(Q)docker build --target artifacts \
	    --output type=local,dest=. \
	    -f hack/Dockerfile.generate .

clean:
	rm -f levelzero/cgo_helpers.go levelzero/cgo_helpers.h levelzero/cgo_helpers.c
	rm -f levelzero/const.go levelzero/doc.go levelzero/types.go
	rm -f levelzero/levelzero.go
	rm -f levelzero/zz_*
