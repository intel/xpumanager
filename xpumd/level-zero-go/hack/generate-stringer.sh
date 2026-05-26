#!/bin/bash
#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT


project_root_dir=$(dirname "$(go env GOMOD)")
types=$(awk '/^type [A-Z]/{print $2}' const.go)

camel_to_snake() {
    echo "$1" | sed 's/\([A-Z]\)/_\1/g;s/^_//;s/.*/\U&/'
}

for type in $types; do
    echo "Generating string() method for type: $type"

    prefix=$(camel_to_snake "$type")_
    go tool -modfile="$project_root_dir/hack/go.mod" stringer -type="$type" -trimprefix="$prefix" -output="zz_${type}_string.go" const.go
done
