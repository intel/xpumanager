#!/bin/bash -ex

# We assume this script is run from the root of the repository
ROOT_DIR=$(pwd)

go -C receiver generate

go -C charts/xpumd tool -modfile "$ROOT_DIR/tools/go.mod" helm-docs
go -C charts/xpumd tool -modfile "$ROOT_DIR/tools/go.mod" helm-values-schema-json
