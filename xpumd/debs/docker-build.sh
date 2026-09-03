#!/bin/sh
#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT

# exit with error code after printing error message arg
error_exit ()
{
	echo
	echo "Usage: ${0##*/} <image registry> <driver DEB packages dir> [prefix]"
	echo
	echo "Image tag will be <prefix>-<L0 GPU driver package version>."
	echo "If prefix argument is missing, fallbacks are (in this order):"
	echo "Current commit Git tag, branch name, commit ID."
	echo
	echo "ERROR: $1!"
	exit 1
}

if [ $# -lt 2 ]; then
	error_exit "not enough arguments"
fi

SCRIPT_DIR=${0%/*}
cd "$SCRIPT_DIR" || error_exit "failed to 'cd' into script's dir"

REGISTRY=$1
shift

DEB_DIR=${1%/}
if [ -z "$DEB_DIR" ] || [ ! -d "$DEB_DIR" ] || [ "${DEB_DIR##*/}" != "$DEB_DIR" ]; then
	error_exit "driver packages dir '$DEB_DIR' missing, or not script dir subdir"
fi
shift

# L0 backend DEB package version
# shellcheck disable=SC2012
L0_VERSION=$(ls "$DEB_DIR"/libze-intel-gpu*.deb | sed -e 's|.*/||' -e 's/^.*_\([0-9.]\+\)-.*$/\1/')
if [ "$(echo "$L0_VERSION" | wc -w)" -gt 1 ]; then
	error_exit "unable to determine L0 backend version, multiple matches for '$DEB_DIR/libze-intel-gpu*.deb'"
fi
if [ -z "$L0_VERSION" ]; then
	error_exit "unable to determine L0 backend version from '$DEB_DIR/libze-intel-gpu*.deb'"
fi

# XPUMD root dir needed for image build
cd ..

PREFIX=$1
if [ -z "$PREFIX" ]; then
	# Git repo tag, branch name, or commit ID available?
	PREFIX=$(git describe --tags --exact-match 2>/dev/null)
	if [ -z "$PREFIX" ]; then
		COMMIT_TYPE="branch name"
		PREFIX=$(git branch --show-current)
		if [ -z "$PREFIX" ]; then
			COMMIT_TYPE="commit ID"
			PREFIX=$(git rev-parse --short=12 HEAD)
			if [ -z "$PREFIX" ]; then
				error_exit "failed to get either Git branch or commit ID => specify tag prefix explicitly"
			fi
		fi
		echo "Not on tagged commit => using git $COMMIT_TYPE as image tag prefix!"
	fi
	PREFIX=$(echo "$PREFIX" | tr / -)
fi

image="$REGISTRY/xpumd:$PREFIX-$L0_VERSION"
echo "Building XPUMD image:"
echo "$image"

echo
set -x
# Real Docker requires explicit passing of proxy variables, Podman wrapper does not.
# shellcheck disable=SC2154
docker build -t "$image" \
  --build-arg BACKEND="local" \
  --build-arg FILES_DIR="debs/$DEB_DIR" \
  --build-arg https_proxy="$https_proxy" \
  --build-arg http_proxy="$http_proxy" \
  .
