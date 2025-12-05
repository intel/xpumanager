#!/bin/bash -ex
set -o pipefail

ROOT_DIR=$(pwd)
PACKAGE="$1"

gotool() {
    go tool -modfile "$ROOT_DIR/hack/go.mod" "$@"
}

if [ -z "$PACKAGE" ] || [ ! -d "$PACKAGE" ]; then
    echo "ERROR: package <dir> argument missing or not a dir ('$PACKAGE')!" 1>&2
    exit 1
fi

gotool c-for-go -nostamp $PACKAGE.yml

sed -i "$PACKAGE/$PACKAGE.go" \
    -e s'!*C.struct__\(ze_[a-z_]*_handle_t\)!*C.\1!' \
    -e s'!*C.struct__\(zes_[a-z_]*_handle_t\)!*C.\1!'

if [ "$PACKAGE" != "levelzero" ]; then
    cp levelzero/{ze,zes}_api.h "$PACKAGE"
fi

cd "$PACKAGE"
go tool cgo -godefs ./types.go > types.go.tmp
go -C "$ROOT_DIR/hack" run ./types-mangle \
    -in-place \
    -config "$PWD/types-mangle.yaml" \
    "$PWD/types.go.tmp" "$PWD/const.go"
gotool goimports types.go.tmp > types.go
rm -f types.go.tmp
cd -

go generate "./$PACKAGE/..."

rm -rf "$PACKAGE"/{cgo_helpers.go,_obj/}
