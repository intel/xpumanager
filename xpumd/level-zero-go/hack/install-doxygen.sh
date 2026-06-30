#!/bin/bash -e
#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT

set -o pipefail

DOXYGEN_REL="https://github.com/doxygen/doxygen/releases/download"

DOXYGEN_VERSION="${DOXYGEN_VERSION:-1.17.0}"
TAR_FILE="doxygen-${DOXYGEN_VERSION}.linux.bin.tar.gz"
CHECKSUM="${CHECKSUM:-75419ef4f446fc1c24ef12514b574e66e898ee6f527c6ae2ad84f91a905823c2}"

SCRIPT_DIR="$(dirname "$(realpath "$0")")"
cd "$SCRIPT_DIR"
mkdir -p bin
BIN_DIR=$(realpath bin)

if [ -x "$BIN_DIR/doxygen" ]; then
    INSTALLED_DOXYGEN_VERSION=$("$BIN_DIR/doxygen" --version 2> /dev/null | awk '{print $1}')
fi

if [ "$INSTALLED_DOXYGEN_VERSION" == "$DOXYGEN_VERSION" ]; then
    echo "found doxygen version $INSTALLED_DOXYGEN_VERSION installed in $BIN_DIR"
else
    echo "downloading doxygen version $DOXYGEN_VERSION to $BIN_DIR"
    TAG="Release_$(echo "$DOXYGEN_VERSION" | tr '.' '_')"
    curl -fSLO "$DOXYGEN_REL/$TAG/$TAR_FILE"
    if ! echo "$CHECKSUM  $TAR_FILE" | sha256sum --strict --check -; then
        echo "--- Checksum ERROR, expected: ---"
        echo "$CHECKSUM  $TAR_FILE"
        echo "--- Got: ---"
        sha256sum "$TAR_FILE"
        exit 1
    fi

    tar -xzf "$TAR_FILE" -C "$BIN_DIR" --strip-components=2 "doxygen-${DOXYGEN_VERSION}/bin/doxygen"
    rm -f "$TAR_FILE"
fi
