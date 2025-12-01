#!/bin/bash -e

set -o pipefail

PB_VERSION=${PB_VERSION-33.4}
PB_REL="https://github.com/protocolbuffers/protobuf/releases"

# We operate under the tools go module
cd tools
BIN_DIR=$(realpath bin)

if [ -x "$BIN_DIR/protoc" ]; then
    INSTALLED_PROTOC_VERSION=$("$BIN_DIR/protoc" --version | awk '{print $2}')
fi

if [ "$INSTALLED_PROTOC_VERSION" == "$PB_VERSION" ]; then
    echo "found protoc version $INSTALLED_PROTOC_VERSION installed in $BIN_DIR"
else
    curl -LO "$PB_REL/download/v$PB_VERSION/protoc-$PB_VERSION-linux-x86_64.zip"

    unzip -o "protoc-$PB_VERSION-linux-x86_64.zip"

    rm -f "protoc-$PB_VERSION-linux-x86_64.zip"
    rm -f readme.txt
fi

export GOBIN="$BIN_DIR"
go install google.golang.org/grpc/cmd/protoc-gen-go-grpc
go install google.golang.org/protobuf/cmd/protoc-gen-go
