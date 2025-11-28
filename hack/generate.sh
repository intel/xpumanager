#!/bin/bash -ex
set -o pipefail

ROOT_DIR=$(pwd)
PACKAGE="$1"

if [ -z "$PACKAGE" ] || [ ! -d "$PACKAGE" ]; then
    echo "ERROR: package <dir> argument missing or not a dir ('$PACKAGE')!" 1>&2
    exit 1
fi

go tool c-for-go -nostamp $PACKAGE.yml

sed -i "$PACKAGE/$PACKAGE.go" \
    -e s'!*C.struct__\(ze_[a-z_]*_handle_t\)!*C.\1!' \
    -e s'!*C.struct__\(zes_[a-z_]*_handle_t\)!*C.\1!'

if [ "$PACKAGE" != "levelzero" ]; then
    cp levelzero/{ze,zes}_api.h "$PACKAGE"
fi

cd "$PACKAGE"
go tool cgo -godefs ./types.go > types.go.tmp
go run "$ROOT_DIR/hack/types-mangle" -file types.go.tmp > types.go
rm -f types.go.tmp
cd -

sed -i "$PACKAGE/types.go" \
    -e s'!Pad_cgo_[0-9]*!_!' \
    -e s'!Stype!stype!' \
    -e s'!PNext!pnext!'

go fmt "$PACKAGE/types.go"

go generate "./$PACKAGE/..."

rm -rf "$PACKAGE"/{cgo_helpers.go,_obj/}
