#!/bin/bash

types=$(grep type const.go | grep Ze | awk '{print $2;}')

camel_to_snake() {
    echo "$1" | sed 's/\([A-Z]\)/_\1/g;s/^_//;s/.*/\U&/'
}

for type in $types; do
    echo "Generating string() method for type: $type"

    prefix=$(camel_to_snake "$type")_
    go tool stringer -type=$type -trimprefix=$prefix -output=zz_${type}_string.go const.go
done
