/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

// The premerge static-analysis stage runs clang-tidy over the files a pull
// request changes using the compile database of a build configured without
// -Dwith_tests, so this file is analysed with no doctest include directory and
// an unguarded include would be a fatal analyser error. Guarding it makes the
// translation unit empty in that pass; a real test build cannot miss the header
// because hal/core/meson.build requires the doctest dependency for this target.
#if __has_include(<doctest/doctest.h>)

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
// doctest defines INFO(expr) for test context; undef it so debug.h (pulled in
// via memory.h -> sysman.h) can define INFO(fmt, ...) for log-level gating.
#undef INFO

#include "memory.h"

// Tests for memory::sysmanMemoryTypeToString(), which names the memory type
// reported per module by zesMemoryGetProperties(). Every value this Level Zero
// version defines is covered so a header update that renumbers or adds an enum
// value is caught here rather than showing up as a wrong name in `discovery`.

TEST_CASE("sysmanMemoryTypeToString: names every sysman memory type")
{
	CHECK(memory::sysmanMemoryTypeToString(ZES_MEM_TYPE_HBM) == "HBM");
	CHECK(memory::sysmanMemoryTypeToString(ZES_MEM_TYPE_DDR) == "DDR");
	CHECK(memory::sysmanMemoryTypeToString(ZES_MEM_TYPE_DDR3) == "DDR3");
	CHECK(memory::sysmanMemoryTypeToString(ZES_MEM_TYPE_DDR4) == "DDR4");
	CHECK(memory::sysmanMemoryTypeToString(ZES_MEM_TYPE_DDR5) == "DDR5");
	CHECK(memory::sysmanMemoryTypeToString(ZES_MEM_TYPE_LPDDR) == "LPDDR");
	CHECK(memory::sysmanMemoryTypeToString(ZES_MEM_TYPE_LPDDR3) == "LPDDR3");
	CHECK(memory::sysmanMemoryTypeToString(ZES_MEM_TYPE_LPDDR4) == "LPDDR4");
	CHECK(memory::sysmanMemoryTypeToString(ZES_MEM_TYPE_LPDDR5) == "LPDDR5");
	CHECK(memory::sysmanMemoryTypeToString(ZES_MEM_TYPE_SRAM) == "SRAM");
	CHECK(memory::sysmanMemoryTypeToString(ZES_MEM_TYPE_L1) == "L1");
	CHECK(memory::sysmanMemoryTypeToString(ZES_MEM_TYPE_L3) == "L3");
	CHECK(memory::sysmanMemoryTypeToString(ZES_MEM_TYPE_GRF) == "GRF");
	CHECK(memory::sysmanMemoryTypeToString(ZES_MEM_TYPE_SLM) == "SLM");
	CHECK(memory::sysmanMemoryTypeToString(ZES_MEM_TYPE_GDDR4) == "GDDR4");
	CHECK(memory::sysmanMemoryTypeToString(ZES_MEM_TYPE_GDDR5) == "GDDR5");
	CHECK(memory::sysmanMemoryTypeToString(ZES_MEM_TYPE_GDDR5X) == "GDDR5X");
	CHECK(memory::sysmanMemoryTypeToString(ZES_MEM_TYPE_GDDR6) == "GDDR6");
	CHECK(memory::sysmanMemoryTypeToString(ZES_MEM_TYPE_GDDR6X) == "GDDR6X");
	CHECK(memory::sysmanMemoryTypeToString(ZES_MEM_TYPE_GDDR7) == "GDDR7");
}

TEST_CASE("sysmanMemoryTypeToString: the Intel LPDDR5X extension value is named")
{
	// Crescent Island's sysman product helper casts ZES_INTEL_MEM_TYPE_LPDDR5X (500)
	// into zes_mem_type_t, which has no enumerator for it. Failing to name it would
	// discard the only source that distinguishes LPDDR5X from LPDDR5.
	CHECK(memory::sysmanMemoryTypeToString(MEMORY_INTEL_TYPE_LPDDR5X) == "LPDDR5X");
}

TEST_CASE("sysmanMemoryTypeToString: unmapped types report unknown")
{
	// FORCE_UINT32 is what the driver actually reports when the platform's
	// product helper has no memory-type mapping (observed on Arrow Lake), so it
	// must degrade to "unknown". It is also what the type table stores for a name
	// the sysman enum cannot express, so this pins that those rows -- HBM2 is the
	// first of them -- are never returned as a match for "no type".
	CHECK(memory::sysmanMemoryTypeToString(ZES_MEM_TYPE_FORCE_UINT32) == MEMORY_SPEC_UNKNOWN);

	// A value from a newer driver than this header knows about must not be
	// misreported either.
	CHECK(memory::sysmanMemoryTypeToString(static_cast<zes_mem_type_t>(9999)) == MEMORY_SPEC_UNKNOWN);
}

// Tests for memory::coreMemoryTypeToString(), the core-API counterpart used via
// ze_device_memory_ext_properties_t. Its enum covers memory generations sysman
// does not (HBM2E/HBM3/HBM3E/HBM4), which is the whole reason it is consulted,
// so those are checked explicitly.

TEST_CASE("coreMemoryTypeToString: names every core memory extension type")
{
	CHECK(memory::coreMemoryTypeToString(ZE_DEVICE_MEMORY_EXT_TYPE_HBM) == "HBM");
	CHECK(memory::coreMemoryTypeToString(ZE_DEVICE_MEMORY_EXT_TYPE_HBM2) == "HBM2");
	CHECK(memory::coreMemoryTypeToString(ZE_DEVICE_MEMORY_EXT_TYPE_HBM2E) == "HBM2E");
	CHECK(memory::coreMemoryTypeToString(ZE_DEVICE_MEMORY_EXT_TYPE_HBM3) == "HBM3");
	CHECK(memory::coreMemoryTypeToString(ZE_DEVICE_MEMORY_EXT_TYPE_HBM3E) == "HBM3E");
	CHECK(memory::coreMemoryTypeToString(ZE_DEVICE_MEMORY_EXT_TYPE_HBM4) == "HBM4");
	CHECK(memory::coreMemoryTypeToString(ZE_DEVICE_MEMORY_EXT_TYPE_DDR) == "DDR");
	CHECK(memory::coreMemoryTypeToString(ZE_DEVICE_MEMORY_EXT_TYPE_DDR2) == "DDR2");
	CHECK(memory::coreMemoryTypeToString(ZE_DEVICE_MEMORY_EXT_TYPE_DDR3) == "DDR3");
	CHECK(memory::coreMemoryTypeToString(ZE_DEVICE_MEMORY_EXT_TYPE_DDR4) == "DDR4");
	CHECK(memory::coreMemoryTypeToString(ZE_DEVICE_MEMORY_EXT_TYPE_DDR5) == "DDR5");
	CHECK(memory::coreMemoryTypeToString(ZE_DEVICE_MEMORY_EXT_TYPE_LPDDR) == "LPDDR");
	CHECK(memory::coreMemoryTypeToString(ZE_DEVICE_MEMORY_EXT_TYPE_LPDDR3) == "LPDDR3");
	CHECK(memory::coreMemoryTypeToString(ZE_DEVICE_MEMORY_EXT_TYPE_LPDDR4) == "LPDDR4");
	CHECK(memory::coreMemoryTypeToString(ZE_DEVICE_MEMORY_EXT_TYPE_LPDDR5) == "LPDDR5");
	CHECK(memory::coreMemoryTypeToString(ZE_DEVICE_MEMORY_EXT_TYPE_SRAM) == "SRAM");
	CHECK(memory::coreMemoryTypeToString(ZE_DEVICE_MEMORY_EXT_TYPE_L1) == "L1");
	CHECK(memory::coreMemoryTypeToString(ZE_DEVICE_MEMORY_EXT_TYPE_L3) == "L3");
	CHECK(memory::coreMemoryTypeToString(ZE_DEVICE_MEMORY_EXT_TYPE_GRF) == "GRF");
	CHECK(memory::coreMemoryTypeToString(ZE_DEVICE_MEMORY_EXT_TYPE_SLM) == "SLM");
	CHECK(memory::coreMemoryTypeToString(ZE_DEVICE_MEMORY_EXT_TYPE_GDDR4) == "GDDR4");
	CHECK(memory::coreMemoryTypeToString(ZE_DEVICE_MEMORY_EXT_TYPE_GDDR5) == "GDDR5");
	CHECK(memory::coreMemoryTypeToString(ZE_DEVICE_MEMORY_EXT_TYPE_GDDR5X) == "GDDR5X");
	CHECK(memory::coreMemoryTypeToString(ZE_DEVICE_MEMORY_EXT_TYPE_GDDR6) == "GDDR6");
	CHECK(memory::coreMemoryTypeToString(ZE_DEVICE_MEMORY_EXT_TYPE_GDDR6X) == "GDDR6X");
	CHECK(memory::coreMemoryTypeToString(ZE_DEVICE_MEMORY_EXT_TYPE_GDDR7) == "GDDR7");
}

TEST_CASE("coreMemoryTypeToString: unmapped types report unknown")
{
	// Also pins the table invariant on this side: LPDDR5X is stored with no core
	// value, so "no type" must not resolve to it.
	CHECK(memory::coreMemoryTypeToString(ZE_DEVICE_MEMORY_EXT_TYPE_FORCE_UINT32) == MEMORY_SPEC_UNKNOWN);
	CHECK(memory::coreMemoryTypeToString(static_cast<ze_device_memory_ext_type_t>(9999)) == MEMORY_SPEC_UNKNOWN);
}

// Tests for memory::resolveMemoryTypeName(), which arbitrates between the two
// sources. Neither is authoritative on its own: zes_mem_type_t has a single HBM
// value so sysman cannot express an HBM generation, while the core mapping does
// not know LPDDR5X. The more specific name therefore wins when one name refines
// the other, and the per-module sysman value wins otherwise.

TEST_CASE("resolveMemoryTypeName: agreeing sources report that type")
{
	CHECK(memory::resolveMemoryTypeName(ZES_MEM_TYPE_GDDR6, ZE_DEVICE_MEMORY_EXT_TYPE_GDDR6) == "GDDR6");
	CHECK(memory::resolveMemoryTypeName(ZES_MEM_TYPE_LPDDR5, ZE_DEVICE_MEMORY_EXT_TYPE_LPDDR5) == "LPDDR5");
}

TEST_CASE("resolveMemoryTypeName: single source answers are used as-is")
{
	CHECK(memory::resolveMemoryTypeName(ZES_MEM_TYPE_HBM, ZE_DEVICE_MEMORY_EXT_TYPE_FORCE_UINT32) == "HBM");
	// Observed on real Arrow Lake-U hardware: sysman has no mapping and only the
	// core extension names the memory.
	CHECK(memory::resolveMemoryTypeName(ZES_MEM_TYPE_FORCE_UINT32, ZE_DEVICE_MEMORY_EXT_TYPE_LPDDR5) == "LPDDR5");
	CHECK(memory::resolveMemoryTypeName(ZES_MEM_TYPE_FORCE_UINT32, ZE_DEVICE_MEMORY_EXT_TYPE_HBM3E) == "HBM3E");
}

TEST_CASE("resolveMemoryTypeName: the core generation refines the sysman family")
{
	// zes_mem_type_t has no HBM2/HBM3 enumerators at all, so sysman answers "HBM"
	// for every HBM part while the core extension names the generation. Reporting
	// "HBM" there would throw away data the driver already gave us.
	CHECK(memory::resolveMemoryTypeName(ZES_MEM_TYPE_HBM, ZE_DEVICE_MEMORY_EXT_TYPE_HBM3) == "HBM3");
	CHECK(memory::resolveMemoryTypeName(ZES_MEM_TYPE_HBM, ZE_DEVICE_MEMORY_EXT_TYPE_HBM2E) == "HBM2E");
	CHECK(memory::resolveMemoryTypeName(ZES_MEM_TYPE_HBM, ZE_DEVICE_MEMORY_EXT_TYPE_HBM4) == "HBM4");
	CHECK(memory::resolveMemoryTypeName(ZES_MEM_TYPE_DDR, ZE_DEVICE_MEMORY_EXT_TYPE_DDR5) == "DDR5");
	// Refinement is symmetric: whichever source is more specific wins, so the
	// mirror of the LPDDR5X case below resolves to the core name.
	CHECK(memory::resolveMemoryTypeName(ZES_MEM_TYPE_GDDR6, ZE_DEVICE_MEMORY_EXT_TYPE_GDDR6X) == "GDDR6X");
}

TEST_CASE("resolveMemoryTypeName: the sysman type refines the core family")
{
	// The Crescent Island case: sysman reports the Intel LPDDR5X extension value
	// while the core mapping only knows LPDDR5.
	CHECK(memory::resolveMemoryTypeName(MEMORY_INTEL_TYPE_LPDDR5X, ZE_DEVICE_MEMORY_EXT_TYPE_LPDDR5) == "LPDDR5X");
	CHECK(memory::resolveMemoryTypeName(ZES_MEM_TYPE_GDDR6X, ZE_DEVICE_MEMORY_EXT_TYPE_GDDR6) == "GDDR6X");
}

TEST_CASE("resolveMemoryTypeName: unrelated families keep the sysman value")
{
	// Neither name refines the other, so the per-module sysman value is kept
	// rather than silently switching families.
	CHECK(memory::resolveMemoryTypeName(ZES_MEM_TYPE_GDDR6, ZE_DEVICE_MEMORY_EXT_TYPE_HBM3) == "GDDR6");
	CHECK(memory::resolveMemoryTypeName(ZES_MEM_TYPE_LPDDR5, ZE_DEVICE_MEMORY_EXT_TYPE_DDR5) == "LPDDR5");
}

TEST_CASE("resolveMemoryTypeName: unknown when neither source maps")
{
	// Never guess a type: both sources silent means "unknown".
	CHECK(memory::resolveMemoryTypeName(ZES_MEM_TYPE_FORCE_UINT32, ZE_DEVICE_MEMORY_EXT_TYPE_FORCE_UINT32) ==
		  MEMORY_SPEC_UNKNOWN);
	CHECK(memory::resolveMemoryTypeName(static_cast<zes_mem_type_t>(9999),
										static_cast<ze_device_memory_ext_type_t>(9999)) == MEMORY_SPEC_UNKNOWN);
}

#endif // __has_include(<doctest/doctest.h>)
