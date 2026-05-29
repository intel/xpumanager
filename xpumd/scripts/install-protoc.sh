#!/bin/bash -e
#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT


set -o pipefail

PB_REL="https://github.com/protocolbuffers/protobuf/releases"

PB_VERSION="${PB_VERSION-33.4}"
ZIP_FILE="protoc-$PB_VERSION-linux-x86_64.zip"
CHECKSUM="${CHECKSUM-c0040ea9aef08fdeb2c74ca609b18d5fdbfc44ea0042fcfbfb38860d35f7dd66}"

# We operate under the tools go module
SCRIPT_DIR="$(dirname "$(realpath "$0")")"
cd "$SCRIPT_DIR/../tools"
BIN_DIR=$(realpath bin)

if [ -x "$BIN_DIR/protoc" ]; then
    INSTALLED_PROTOC_VERSION=$("$BIN_DIR/protoc" --version | awk '{print $2}')
fi

if [ "$INSTALLED_PROTOC_VERSION" == "$PB_VERSION" ]; then
    echo "found protoc version $INSTALLED_PROTOC_VERSION installed in $BIN_DIR"
else
    curl -LO "$PB_REL/download/v$PB_VERSION/$ZIP_FILE"
    if ! echo "$CHECKSUM $ZIP_FILE" | sha256sum --check -; then
	echo "--- Checksum ERROR, expected: ---"
	echo "$CHECKSUM  $ZIP_FILE"
	echo "--- Got: ---"
	sha256sum "$ZIP_FILE"
	exit 1
    fi

    unzip -o "$ZIP_FILE"
    rm -f "$ZIP_FILE"
    rm -f readme.txt
fi

export GOBIN="$BIN_DIR"
go install google.golang.org/grpc/cmd/protoc-gen-go-grpc
go install google.golang.org/protobuf/cmd/protoc-gen-go
