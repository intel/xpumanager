#!/bin/bash
#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#
# For a release tag vX.Y.Z, creates and pushes "<module-dir>/v0.X.Y-Z" tags for
# the xpumd Go submodules. Existing tags are skipped. Run from the repository
# root.

set -e -u -o pipefail

DRY_RUN=
if [ "${1:-}" = "--dry-run" ]; then
    DRY_RUN=1
    shift
fi

if [ $# -ne 2 ]; then
    echo "Usage: ${0##*/} [--dry-run] <release-tag> <go-module-prefix>" >&2
    exit 1
fi

RELEASE_TAG=$1
MODULE_PREFIX=$2

if [ ! -d xpumd ]; then
    echo "error: must be run from the repository root" >&2
    exit 1
fi

if [[ ! "$RELEASE_TAG" =~ ^v([0-9]+)\.([0-9]+)\.([0-9]+)$ ]]; then
    echo "error: '$RELEASE_TAG' is not a vX.Y.Z release tag" >&2
    exit 1
fi
version="v0.${BASH_REMATCH[1]}.${BASH_REMATCH[2]}-${BASH_REMATCH[3]}"
commit=$(git rev-parse "refs/tags/$RELEASE_TAG^{commit}")

new_tags=
new_refs=
for gomod in $(git ls-tree -r --name-only "$commit" -- xpumd | grep -E '(^|/)go\.mod$'); do
    dir=$(dirname "$gomod")
    case "$dir" in
        # Not meant for external consumption
        xpumd|xpumd/test*|xpumd/tools*) continue ;;
        # Only tag the API module for now
        # NOTE: many of the modules have replace directives to local paths so they wouldn't be usable outside
        xpumd/exporter/intelxpuinfo/api) ;;
        *) echo "Skipping $dir (not enabled)"; continue ;;
    esac
    # Go only resolves the tag if the module path matches its repo subdir
    # (this will skip the level-zero-go modules, too)
    module=$(git show "$commit:$gomod" | awk '$1 == "module" { print $2; exit }')
    if [ "$module" != "${MODULE_PREFIX}${dir}" ]; then
        echo "Skipping $dir (module path $module)"
        continue
    fi

    tag="$dir/$version"
    if git rev-parse -q --verify "refs/tags/$tag" >/dev/null; then
        echo "Tag $tag already exists, skipping"
        continue
    fi
    if [ -z "$DRY_RUN" ]; then
        git tag -a "$tag" -m "$module $version (from $RELEASE_TAG)" "$commit"
    fi
    new_tags="$new_tags $tag"
    new_refs="$new_refs refs/tags/$tag"
done

if [ -z "$new_tags" ]; then
    echo "No new tags to push"
    exit 0
fi
if [ -n "$DRY_RUN" ]; then
    echo "Would push $new_tags"
    exit 0
fi

echo "Pushing $new_tags"
# shellcheck disable=SC2086  # (info): Double quote to prevent globbing and word splitting.
git push --atomic origin $new_refs
