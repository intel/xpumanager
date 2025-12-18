#!/bin/bash -ex

# We assume this script is run from within the main module
ROOT_DIR=$(dirname "$(go env GOMOD)")

cd charts/xpu-exporter
go tool -modfile "$ROOT_DIR/tools/go.mod" helm-docs
go tool -modfile "$ROOT_DIR/tools/go.mod" helm-values-schema-json
