/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (C) 2020 Intel Corporation
 */
#ifndef __IGSC_GCC_CONFIG_H__
#define __IGSC_GCC_CONFIG_H__

#if defined(__clang__) || defined(__GNUC__)

#ifdef UNIT_TESTING
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#define  mockable_static __attribute__((weak))
#define  mockable __attribute__((weak))
#else
#define  mockable_static static
#define  mockable
#endif

#endif /* defined(__clang__) || defined(__GNUC__) */

#endif /* !__IGSC_GCC_CONFIG_H__ */
