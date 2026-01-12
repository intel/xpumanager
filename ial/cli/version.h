/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _VERSION_H
#define _VERSION_H

#ifdef XPUM_VERSION_MAJOR
#define MAJOR XPUM_VERSION_MAJOR
#else
#define MAJOR 0
#endif

#ifdef XPUM_VERSION_MINOR
#define MINOR XPUM_VERSION_MINOR
#else
#define MINOR 0
#endif

#ifdef XPUM_VERSION_PATCH
#define PATCH XPUM_VERSION_PATCH
#else
#define PATCH 0
#endif

#define XPUM_STRINGIFY_IMPL(x) #x
#define XPUM_TOSTRING(x) XPUM_STRINGIFY_IMPL(x)

#define XPUM_VERSION_STRING XPUM_TOSTRING(MAJOR) "." XPUM_TOSTRING(MINOR) "." XPUM_TOSTRING(PATCH)
#define BUILD_NUMBER 20250225

#endif