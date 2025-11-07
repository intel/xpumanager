#
# Copyright (C) 2019-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT

OUTDIR ?= .
Q := @

all: build

generate: clean
	$(Q)go tool c-for-go -nostamp -out $(OUTDIR) levelzero.yml
	$(Q)sed -i $(OUTDIR)/levelzero/levelzero.go \
	        -e s'!*C.struct__\(ze_[a-z_]*_handle_t\)!*C.\1!' \
	        -e s'!*C.struct__\(zes_[a-z_]*_handle_t\)!*C.\1!'
	$(Q)sed -i $(OUTDIR)/levelzero/cgo_helpers.go \
		    -e s'!ZesMemState) Free()!ZesMemState) FreeX()!'

generate-dockerized: clean
	$(Q)docker build --target artifacts \
	    --output type=local,dest=. \
	    -f hack/Dockerfile.generate .

clean:
	rm -f levelzero/cgo_helpers.go levelzero/cgo_helpers.h levelzero/cgo_helpers.c
	rm -f levelzero/const.go levelzero/doc.go levelzero/types.go
	rm -f levelzero/levelzero.go
