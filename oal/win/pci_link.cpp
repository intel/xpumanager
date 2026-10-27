/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Windows exposes none of these without a kernel driver, so each reads as unknown.
 */

#include "pci_link.h"
#include "os.h"

PcieLinkMax getRootPortMaxLink(UNUSED std::string_view bdf) { return {}; }

std::optional<PcieAerCounts> getPcieAerCounts(UNUSED std::string_view bdf) { return std::nullopt; }

std::optional<PcieAtomicOps> getPcieAtomicOps(UNUSED std::string_view bdf) { return std::nullopt; }
