/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#ifdef INFO
#undef INFO
#endif

// progName is an extern defined by the CLI executable; provide a stub for tests.
#include <string>
std::string progName = "test";
#ifdef INFO
#undef INFO
#endif
// cmd_topology.h pulls in cmds.h → zes_api.h (level-zero), so we just need
// the NIC-related public surface: TopoNode, TopoNodeKind, and determineLinkType.
// discoverNics lives in the oal_lin library and can be called directly.
#include "cmd_topology.h"
#include "topology.h"

#include <filesystem>
#include <fstream>
#include <optional>
#include <stdlib.h>
#include <string>
#include <vector>

// ─── Helpers ─────────────────────────────────────────────────────────────────

namespace {

/// Build a minimal GPU TopoNode.
struct GpuArgs
{
	int deviceId;
	int tileId;
	std::optional<int> numaNode{};
	std::optional<PciePath> pciePath{};
};

TopoNode makeGpu(GpuArgs args)
{
	return TopoNode{.label = std::format("GPU {}/{}", args.deviceId, args.tileId),
					.cpuAffinity = {},
					.bdfAddress = {},
					.numaNode = args.numaNode,
					.pciePath = args.pciePath,
					.data = GpuData{.deviceId = args.deviceId, .tileId = args.tileId, .ports = {}}};
}

/// Build a minimal NIC TopoNode.
struct NicArgs
{
	int nicIdx;
	std::optional<int> numaNode{};
	std::optional<PciePath> pciePath{};
};

TopoNode makeNic(NicArgs args)
{
	return TopoNode{.label = std::format("NIC{}", args.nicIdx),
					.cpuAffinity = {},
					.bdfAddress = {},
					.numaNode = args.numaNode,
					.pciePath = args.pciePath,
					.data = NicData{.nicIndex = args.nicIdx}};
}

/// Build a PciePath using canonical sysfs segment strings.
/// rc = root complex (e.g. "pci0000:00"), rp = root port BDF, sw = switch upstream BDF.
/// Omit trailing elements by passing an empty string.
PciePath makePath(std::string rc, std::string rp = "", std::string sw = "")
{
	PciePath p;
	p.push_back(PcieBridgeLink{.bdf = std::move(rc), .isHostBridge = true});
	if (!rp.empty())
		p.push_back(PcieBridgeLink{.bdf = std::move(rp), .isHostBridge = false});
	if (!sw.empty())
		p.push_back(PcieBridgeLink{.bdf = std::move(sw), .isHostBridge = false});
	return p;
}

/// RAII temporary directory that removes itself on destruction.
struct TempDir
{
	std::filesystem::path path;

	TempDir()
	{
		path = std::filesystem::temp_directory_path() / std::filesystem::path{"topology_test_XXXXXX"};
		// Use mkdtemp for a unique name.
		std::string tmpl = path.string();
		const char *const result = mkdtemp(tmpl.data());
		REQUIRE(result != nullptr);
		path = result;
	}

	~TempDir()
	{
		std::error_code err;
		std::filesystem::remove_all(path, err);
	}

	// non-copyable
	TempDir(const TempDir &) = delete;
	TempDir &operator=(const TempDir &) = delete;
};

/// Write text to a file, creating parent directories as needed.
void writeFile(const std::filesystem::path &filePath, const std::string &content)
{
	std::filesystem::create_directories(filePath.parent_path());
	std::ofstream ofs{filePath};
	REQUIRE(ofs);
	ofs << content;
}

/// Create a symlink at linkPath pointing to target (creating parent dirs).
void createSymlink(const std::filesystem::path &target, const std::filesystem::path &linkPath)
{
	std::filesystem::create_directories(linkPath.parent_path());
	std::filesystem::create_symlink(target, linkPath);
}

} // namespace

// ─── determineLinkType tests ──────────────────────────────────────────────────

TEST_SUITE("determineLinkType")
{
	// determineLinkType is a static method; call it as cmdTopology::determineLinkType.

	TEST_CASE("Self: same node identity returns S")
	{
		const auto node = makeGpu({.deviceId = 0, .tileId = 0, .numaNode = 0});
		CHECK(cmdTopology::determineLinkType(node, node) == "S");
	}

	TEST_CASE("MDF: same GPU device, different tiles")
	{
		const auto tile0 = makeGpu({.deviceId = 0, .tileId = 0, .numaNode = 0});
		const auto tile1 = makeGpu({.deviceId = 0, .tileId = 1, .numaNode = 0});
		CHECK(cmdTopology::determineLinkType(tile0, tile1) == "MDF");
		CHECK(cmdTopology::determineLinkType(tile1, tile0) == "MDF");
	}

	TEST_CASE("MDF does not apply to NIC nodes on the same NUMA node")
	{
		// Two NICs happen to share NUMA node 0 — there's no MDF link between NICs.
		const auto nic0 = makeNic({.nicIdx = 0, .numaNode = 0});
		const auto nic1 = makeNic({.nicIdx = 1, .numaNode = 0});
		// Different labels, not GPU/GPU same device → NODE (same NUMA)
		CHECK(cmdTopology::determineLinkType(nic0, nic1) == "NODE");
	}

	TEST_CASE("PIX: same PCIe switch (3 common ancestors)")
	{
		const PciePath shared = makePath("pci0000:00", "0000:00:01.0", "0000:01:00.0");
		const auto gpu0 = makeGpu({.deviceId = 0, .tileId = 0, .pciePath = shared});
		const auto gpu1 = makeGpu({.deviceId = 1, .tileId = 0, .pciePath = shared});
		CHECK(cmdTopology::determineLinkType(gpu0, gpu1) == "PIX");
		CHECK(cmdTopology::determineLinkType(gpu1, gpu0) == "PIX");
	}

	TEST_CASE("PIX beats NODE: same NUMA and same switch still reports PIX")
	{
		const PciePath shared = makePath("pci0000:00", "0000:00:01.0", "0000:01:00.0");
		const auto gpu0 = makeGpu({.deviceId = 0, .tileId = 0, .numaNode = 0, .pciePath = shared});
		const auto gpu1 = makeGpu({.deviceId = 1, .tileId = 0, .numaNode = 0, .pciePath = shared});
		CHECK(cmdTopology::determineLinkType(gpu0, gpu1) == "PIX");
	}

	TEST_CASE("PXB: same root port, different switches (2 common ancestors)")
	{
		const auto gpu0 =
			makeGpu({.deviceId = 0, .tileId = 0, .pciePath = makePath("pci0000:00", "0000:00:01.0", "0000:01:00.0")});
		const auto gpu1 =
			makeGpu({.deviceId = 1, .tileId = 0, .pciePath = makePath("pci0000:00", "0000:00:01.0", "0000:01:10.0")});
		CHECK(cmdTopology::determineLinkType(gpu0, gpu1) == "PXB");
		CHECK(cmdTopology::determineLinkType(gpu1, gpu0) == "PXB");
	}

	TEST_CASE("PHB: same host bridge, different root ports (1 common ancestor)")
	{
		const auto gpu0 = makeGpu({.deviceId = 0, .tileId = 0, .pciePath = makePath("pci0000:00", "0000:00:01.0")});
		const auto gpu1 = makeGpu({.deviceId = 1, .tileId = 0, .pciePath = makePath("pci0000:00", "0000:00:08.0")});
		CHECK(cmdTopology::determineLinkType(gpu0, gpu1) == "PHB");
		CHECK(cmdTopology::determineLinkType(gpu1, gpu0) == "PHB");
	}

	TEST_CASE("PHB: GPU and NIC behind same host bridge")
	{
		const auto gpu = makeGpu({.deviceId = 0, .tileId = 0, .pciePath = makePath("pci0000:00", "0000:00:01.0")});
		const auto nic = makeNic({.nicIdx = 0, .pciePath = makePath("pci0000:00", "0000:00:04.0")});
		CHECK(cmdTopology::determineLinkType(gpu, nic) == "PHB");
	}

	TEST_CASE("NODE: same NUMA, no PCIe path info")
	{
		const auto gpu0 = makeGpu({.deviceId = 0, .tileId = 0, .numaNode = 0});
		const auto gpu1 = makeGpu({.deviceId = 1, .tileId = 0, .numaNode = 0});
		CHECK(cmdTopology::determineLinkType(gpu0, gpu1) == "NODE");
		CHECK(cmdTopology::determineLinkType(gpu1, gpu0) == "NODE");
	}

	TEST_CASE("SYS: no PCIe path, different NUMA nodes")
	{
		const auto gpu0 = makeGpu({.deviceId = 0, .tileId = 0, .numaNode = 0});
		const auto gpu1 = makeGpu({.deviceId = 1, .tileId = 0, .numaNode = 1});
		CHECK(cmdTopology::determineLinkType(gpu0, gpu1) == "SYS");
	}

	TEST_CASE("SYS: no common PCIe bridge (different host bridges)")
	{
		const auto gpu0 = makeGpu({.deviceId = 0, .tileId = 0, .pciePath = makePath("pci0000:00")});
		const auto gpu1 = makeGpu({.deviceId = 1, .tileId = 0, .pciePath = makePath("pci0001:00")});
		CHECK(cmdTopology::determineLinkType(gpu0, gpu1) == "SYS");
	}
}

// ─── discoverNics tests ────────────────────────────────────────────────────────

TEST_SUITE("discoverNics")
{
	TEST_CASE("Returns nullopt when netRoot does not exist")
	{
		const auto result = discoverNics(SysfsPaths{.netRoot = "/nonexistent/path/that/cannot/exist"});
		CHECK_FALSE(result.has_value());
	}

	TEST_CASE("Returns empty vector when netRoot exists but has no interfaces")
	{
		const TempDir tmp;
		const auto netRoot = tmp.path / "net";
		std::filesystem::create_directories(netRoot);

		const auto result = discoverNics(SysfsPaths{.netRoot = netRoot, .pciDevRoot = tmp.path / "pci"});
		REQUIRE(result.has_value());
		CHECK(result->empty());
	}

	TEST_CASE("Skips loopback interface")
	{
		const TempDir tmp;
		const auto netRoot = tmp.path / "net";
		createSymlink("../../../0000:00:00.0", netRoot / "lo" / "device");

		const auto result = discoverNics(SysfsPaths{.netRoot = netRoot, .pciDevRoot = tmp.path / "pci"});
		REQUIRE(result.has_value());
		CHECK(result->empty());
	}

	TEST_CASE("Skips virtual interfaces (no device symlink)")
	{
		const TempDir tmp;
		const auto netRoot = tmp.path / "net";
		std::filesystem::create_directories(netRoot / "virbr0");

		const auto result = discoverNics(SysfsPaths{.netRoot = netRoot, .pciDevRoot = tmp.path / "pci"});
		REQUIRE(result.has_value());
		CHECK(result->empty());
	}

	TEST_CASE("Discovers a single NIC with correct BDF and CPU affinity")
	{
		const TempDir tmp;
		const auto netRoot = tmp.path / "net";
		const auto pciRoot = tmp.path / "pci";
		const std::string bdf = "0000:03:00.0";

		createSymlink(std::filesystem::path{"../../pci"} / bdf, netRoot / "eth0" / "device");
		writeFile(pciRoot / bdf / "local_cpulist", "0-15");

		const auto result = discoverNics(SysfsPaths{.netRoot = netRoot, .pciDevRoot = pciRoot});
		REQUIRE(result.has_value());
		REQUIRE(result->size() == 1U);
		CHECK((*result)[0].name == "eth0");
		CHECK((*result)[0].bdfAddress == bdf);
		CHECK((*result)[0].cpuAffinity == "0-15");
	}

	TEST_CASE("Returns empty cpuAffinity when local_cpulist file is absent")
	{
		const TempDir tmp;
		const auto netRoot = tmp.path / "net";
		const auto pciRoot = tmp.path / "pci";
		const std::string bdf = "0000:04:00.0";

		// Create the PCI device directory (target of the symlink) but no local_cpulist inside it
		std::filesystem::create_directories(pciRoot / bdf);
		createSymlink(std::filesystem::path{"../../pci"} / bdf, netRoot / "eth0" / "device");

		const auto result = discoverNics(SysfsPaths{.netRoot = netRoot, .pciDevRoot = pciRoot});
		REQUIRE(result.has_value());
		REQUIRE(result->size() == 1U);
		CHECK((*result)[0].cpuAffinity.empty());
	}

	TEST_CASE("Discovers multiple NICs sorted by name")
	{
		const TempDir tmp;
		const auto netRoot = tmp.path / "net";
		const auto pciRoot = tmp.path / "pci";

		createSymlink(std::filesystem::path{"../../pci/0000:05:00.0"}, netRoot / "ens3f0" / "device");
		createSymlink(std::filesystem::path{"../../pci/0000:03:00.0"}, netRoot / "eth0" / "device");
		createSymlink(std::filesystem::path{"../../pci/0000:07:00.0"}, netRoot / "ens3f1" / "device");
		writeFile(pciRoot / "0000:03:00.0" / "local_cpulist", "0-15");
		writeFile(pciRoot / "0000:05:00.0" / "local_cpulist", "16-31");
		writeFile(pciRoot / "0000:07:00.0" / "local_cpulist", "16-31");

		const auto result = discoverNics(SysfsPaths{.netRoot = netRoot, .pciDevRoot = pciRoot});
		REQUIRE(result.has_value());
		REQUIRE(result->size() == 3U);

		// Sorted alphabetically: ens3f0, ens3f1, eth0
		CHECK((*result)[0].name == "ens3f0");
		CHECK((*result)[1].name == "ens3f1");
		CHECK((*result)[2].name == "eth0");
	}

	TEST_CASE("Ignores loopback but keeps other interfaces")
	{
		const TempDir tmp;
		const auto netRoot = tmp.path / "net";
		const auto pciRoot = tmp.path / "pci";

		createSymlink(std::filesystem::path{"../../pci/0000:03:00.0"}, netRoot / "eth0" / "device");
		createSymlink(std::filesystem::path{"../../pci/0000:00:00.0"}, netRoot / "lo" / "device");
		writeFile(pciRoot / "0000:03:00.0" / "local_cpulist", "0-31");

		const auto result = discoverNics(SysfsPaths{.netRoot = netRoot, .pciDevRoot = pciRoot});
		REQUIRE(result.has_value());
		REQUIRE(result->size() == 1U);
		CHECK((*result)[0].name == "eth0");
	}
}

// ─── getNumaNodes tests ────────────────────────────────────────────────────────

TEST_SUITE("getNumaNodes")
{
	TEST_CASE("Returns empty map for empty BDF list")
	{
		const auto result = getNumaNodes({});
		CHECK(result.empty());
	}

	TEST_CASE("Resolves NUMA node from sysfs numa_node file")
	{
		const TempDir tmp;
		const auto pciRoot = tmp.path / "pci";
		const std::string bdf = "0000:03:00.0";
		writeFile(pciRoot / bdf / "numa_node", "1");

		const auto result = getNumaNodes({bdf}, SysfsPaths{.pciDevRoot = pciRoot});
		REQUIRE(result.count(bdf));
		CHECK(result.at(bdf) == std::optional{1});
	}

	TEST_CASE("Returns nullopt when numa_node file is absent")
	{
		const TempDir tmp;
		const auto pciRoot = tmp.path / "pci";
		const std::string bdf = "0000:03:00.0";
		std::filesystem::create_directories(pciRoot / bdf);

		const auto result = getNumaNodes({bdf}, SysfsPaths{.pciDevRoot = pciRoot});
		REQUIRE(result.count(bdf));
		CHECK_FALSE(result.at(bdf).has_value());
	}

	TEST_CASE("Returns nullopt when numa_node is -1 (UMA system)")
	{
		const TempDir tmp;
		const auto pciRoot = tmp.path / "pci";
		const std::string bdf = "0000:03:00.0";
		writeFile(pciRoot / bdf / "numa_node", "-1");

		const auto result = getNumaNodes({bdf}, SysfsPaths{.pciDevRoot = pciRoot});
		REQUIRE(result.count(bdf));
		CHECK_FALSE(result.at(bdf).has_value());
	}

	TEST_CASE("Resolves multiple BDFs in one call")
	{
		const TempDir tmp;
		const auto pciRoot = tmp.path / "pci";
		writeFile(pciRoot / "0000:03:00.0" / "numa_node", "0");
		writeFile(pciRoot / "0000:83:00.0" / "numa_node", "1");

		const auto result = getNumaNodes({"0000:03:00.0", "0000:83:00.0"}, SysfsPaths{.pciDevRoot = pciRoot});
		REQUIRE(result.count("0000:03:00.0"));
		REQUIRE(result.count("0000:83:00.0"));
		CHECK(result.at("0000:03:00.0") == std::optional{0});
		CHECK(result.at("0000:83:00.0") == std::optional{1});
	}

	TEST_CASE("Key is absent when BDF device directory does not exist")
	{
		// A BDF that has no directory under pciDevRoot should not appear in the
		// result map at all (distinguishable from a present-but-unknown NUMA node).
		const TempDir tmp;
		const auto pciRoot = tmp.path / "pci";
		const std::string bdf = "0000:03:00.0";
		// Intentionally do NOT create pciRoot/bdf

		const auto result = getNumaNodes({bdf}, SysfsPaths{.pciDevRoot = pciRoot});
		CHECK(result.count(bdf) == 0U);
	}
}

// ─── getPciePaths tests ───────────────────────────────────────────────────────

TEST_SUITE("getPciePaths")
{
	// Helper: create the real device directory and a symlink at pciRoot/bdf → realDir.
	// Returns the path to the real device directory.
	auto makeDevice = [](const std::filesystem::path &pciRoot, const std::filesystem::path &devicesBase,
						 const std::string &bdf,
						 const std::vector<std::string> &pathSegments) -> std::filesystem::path {
		// Build the real canonical path under devicesBase:
		//   devicesBase / pathSegments[0] / ... / bdf
		std::filesystem::path realDir = devicesBase;
		for (const auto &seg : pathSegments) {
			realDir /= seg;
		}
		realDir /= bdf;
		std::filesystem::create_directories(realDir);
		// Symlink pciRoot/bdf → realDir
		std::filesystem::create_directories(pciRoot);
		std::filesystem::create_symlink(realDir, pciRoot / bdf);
		return realDir;
	};

	TEST_CASE("Returns empty map for empty BDF list")
	{
		const auto result = getPciePaths({});
		CHECK(result.empty());
	}

	TEST_CASE("Key absent when BDF symlink does not exist")
	{
		const TempDir tmp;
		const auto pciRoot = tmp.path / "pci";
		std::filesystem::create_directories(pciRoot);

		const auto result = getPciePaths({"0000:03:00.0"}, SysfsPaths{.pciDevRoot = pciRoot});
		CHECK(result.count("0000:03:00.0") == 0U);
	}

	TEST_CASE("Device directly under root complex (no intermediate bridges)")
	{
		// Canonical path: .../devices/pci0000:00/0000:03:00.0
		// Expected PciePath: [ {pci0000:00, host=true} ]
		const TempDir tmp;
		const auto pciRoot = tmp.path / "pci";
		const std::string bdf = "0000:03:00.0";
		makeDevice(pciRoot, tmp.path / "devices", bdf, {"pci0000:00"});

		const auto result = getPciePaths({bdf}, SysfsPaths{.pciDevRoot = pciRoot});
		REQUIRE(result.count(bdf));
		const auto &path = result.at(bdf);
		REQUIRE(path.size() == 1U);
		CHECK(path[0].bdf == "pci0000:00");
		CHECK(path[0].isHostBridge == true);
	}

	TEST_CASE("One intermediate bridge (root port only)")
	{
		// Canonical path: .../devices/pci0000:00/0000:00:1c.0/0000:03:00.0
		// Expected PciePath: [ {pci0000:00, host}, {0000:00:1c.0, bridge} ]
		const TempDir tmp;
		const auto pciRoot = tmp.path / "pci";
		const std::string bdf = "0000:03:00.0";
		makeDevice(pciRoot, tmp.path / "devices", bdf, {"pci0000:00", "0000:00:1c.0"});

		const auto result = getPciePaths({bdf}, SysfsPaths{.pciDevRoot = pciRoot});
		REQUIRE(result.count(bdf));
		const auto &path = result.at(bdf);
		REQUIRE(path.size() == 2U);
		CHECK(path[0].bdf == "pci0000:00");
		CHECK(path[0].isHostBridge == true);
		CHECK(path[1].bdf == "0000:00:1c.0");
		CHECK(path[1].isHostBridge == false);
	}

	TEST_CASE("Two intermediate bridges (root port + PCIe switch)")
	{
		// Canonical path: .../devices/pci0000:00/0000:00:1c.0/0000:01:00.0/0000:03:00.0
		// Expected PciePath: [ {pci0000:00, host}, {0000:00:1c.0, bridge}, {0000:01:00.0, bridge} ]
		const TempDir tmp;
		const auto pciRoot = tmp.path / "pci";
		const std::string bdf = "0000:03:00.0";
		makeDevice(pciRoot, tmp.path / "devices", bdf, {"pci0000:00", "0000:00:1c.0", "0000:01:00.0"});

		const auto result = getPciePaths({bdf}, SysfsPaths{.pciDevRoot = pciRoot});
		REQUIRE(result.count(bdf));
		const auto &path = result.at(bdf);
		REQUIRE(path.size() == 3U);
		CHECK(path[0].bdf == "pci0000:00");
		CHECK(path[0].isHostBridge == true);
		CHECK(path[1].bdf == "0000:00:1c.0");
		CHECK(path[1].isHostBridge == false);
		CHECK(path[2].bdf == "0000:01:00.0");
		CHECK(path[2].isHostBridge == false);
	}

	TEST_CASE("Two devices share a PCIe switch (PIX classification basis)")
	{
		// Both devices share root complex + root port + switch → 3 common ancestors → PIX
		const TempDir tmp;
		const auto pciRoot = tmp.path / "pci";
		const std::string bdf0 = "0000:03:00.0";
		const std::string bdf1 = "0000:03:00.1";
		makeDevice(pciRoot, tmp.path / "devices", bdf0, {"pci0000:00", "0000:00:1c.0", "0000:01:00.0"});
		makeDevice(pciRoot, tmp.path / "devices", bdf1, {"pci0000:00", "0000:00:1c.0", "0000:01:00.0"});

		const auto result = getPciePaths({bdf0, bdf1}, SysfsPaths{.pciDevRoot = pciRoot});
		REQUIRE(result.count(bdf0));
		REQUIRE(result.count(bdf1));
		// Paths should be identical up to (and including) the switch segment
		CHECK(result.at(bdf0) == result.at(bdf1));
		CHECK(result.at(bdf0).size() == 3U);
	}

	TEST_CASE("Two devices behind different root ports (PHB classification basis)")
	{
		// Share root complex only (commonLen == 1) → PHB; root port differs
		const TempDir tmp;
		const auto pciRoot = tmp.path / "pci";
		const std::string bdf0 = "0000:03:00.0";
		const std::string bdf1 = "0000:05:00.0";
		makeDevice(pciRoot, tmp.path / "devices", bdf0, {"pci0000:00", "0000:00:1c.0"});
		makeDevice(pciRoot, tmp.path / "devices", bdf1, {"pci0000:00", "0000:00:1d.0"});

		const auto result = getPciePaths({bdf0, bdf1}, SysfsPaths{.pciDevRoot = pciRoot});
		REQUIRE(result.count(bdf0));
		REQUIRE(result.count(bdf1));
		REQUIRE(result.at(bdf0).size() == 2U);
		REQUIRE(result.at(bdf1).size() == 2U);
		// Root complex matches, root port differs
		CHECK(result.at(bdf0)[0].bdf == "pci0000:00");
		CHECK(result.at(bdf1)[0].bdf == "pci0000:00");
		CHECK(result.at(bdf0)[1].bdf == "0000:00:1c.0");
		CHECK(result.at(bdf1)[1].bdf == "0000:00:1d.0");
	}
}
