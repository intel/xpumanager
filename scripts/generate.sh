#!/bin/bash -e

# We assume this script is run from the root of the repository
ROOT_DIR=$(pwd)

for module in "$@"; do
    echo "Generating code for module '$module'..."
    go -C "$ROOT_DIR/$module" generate ./...
done

go -C charts/xpumd tool -modfile "$ROOT_DIR/tools/go.mod" helm-docs
go -C charts/xpumd tool -modfile "$ROOT_DIR/tools/go.mod" helm-values-schema-json
