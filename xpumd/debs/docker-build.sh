#!/bin/sh
#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#
# Helper script for testing releases with different driver versions

# exit with error code after printing error message arg
error_exit ()
{
	echo
	echo "Usage: ${0##*/} <image registry> <driver DEB packages dir> [alias tag]"
	echo
	echo "Image tag will be <commit>-<L0 GPU driver package version>,"
	echo "Where <commit> will be one of these (in this order):"
	echo "Current commit Git tag, branch name, commit ID."
	echo
	echo "If [alias tag] is specified (e.g. 'latest-v2.3.0'),"
	echo "that will be added as an alias for the resulting image."
	echo
	echo "ERROR: $1!"
	exit 1
}

# --- check arguments ---

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

ALIAS=$1

# --- build image ---

# XPUMD root dir needed for image build
cd ..

# Git repo tag, branch name, or commit ID available?
COMMIT=$(git describe --tags --exact-match 2>/dev/null)
if [ -z "$COMMIT" ]; then
	COMMIT_TYPE="branch name"
	COMMIT=$(git branch --show-current)
	if [ -z "$COMMIT" ]; then
		COMMIT_TYPE="commit ID"
		COMMIT=$(git rev-parse --short=12 HEAD)
		if [ -z "$COMMIT" ]; then
			error_exit "failed to get Git tag, branch name, or commit ID => run from a valid Git checkout"
		fi
	fi
	echo "Not on tagged commit => using git $COMMIT_TYPE as image tag prefix!"
fi

TAG="$(echo "$COMMIT" | tr / -)-$L0_VERSION"
IMAGE="$REGISTRY/xpumd:$TAG"

if [ -z "$XPUMD_VERSION" ]; then
	XPUMD_VERSION=$(git describe --tags --dirty --always | tr / -)
fi

echo "Building XPUMD ($XPUMD_VERSION) image:"
echo "$IMAGE"

echo
set -ex
# Real Docker requires explicit passing of proxy variables, Podman wrapper does not.
# shellcheck disable=SC2154
docker build -t "$IMAGE" \
  --build-arg "XPUMD_VERSION=$XPUMD_VERSION" \
  --build-arg BACKEND="local" \
  --build-arg FILES_DIR="debs/$DEB_DIR" \
  --build-arg https_proxy="$https_proxy" \
  --build-arg http_proxy="$http_proxy" \
  .

if [ -n "$ALIAS" ]; then
	docker tag "$IMAGE" "$REGISTRY/xpumd:$ALIAS"
fi
