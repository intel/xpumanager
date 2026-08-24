# Copyright (C) 2025-2026 Intel Corporation
# SPDX-License-Identifier: MIT

from conan import ConanFile
from conan.tools.files import copy
from conan.tools.meson import MesonToolchain
from conan.tools.gnu import PkgConfigDeps
import os


class XpumConan(ConanFile):
    name = "xpum"
    version = "1.0.0"

    # Package metadata
    license = "MIT"
    description = "Intel(R) XPU Manager and XPU System Management Interface"
    url = "https://github.com/intel/xpum"

    # Settings and options
    settings = "os", "compiler", "build_type", "arch"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "with_igsc": [True, False],
        "with_tests": [True, False],
        "use_system_levelzero": [True, False],
        "use_system_igsc": [True, False],
        "use_system_hwloc": [True, False],
        "use_system_curl": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "with_igsc": True,
        "with_tests": False,
        "use_system_levelzero": False,
        "use_system_igsc": False,
        "use_system_hwloc": False,
        "use_system_curl": False,
    }

    def requirements(self):
        if not self.options.use_system_levelzero:
            self.requires("level-zero/1.33.0")
        if not self.options.use_system_igsc:
            self.requires("igsc/1.2.0")
        self.requires("nlohmann_json/3.10.2")
        if not self.options.use_system_hwloc:
            self.requires("hwloc/2.9.3")
        if not self.options.use_system_curl:
            self.requires("libcurl/8.5.0")
        self.requires("cli11/2.6.2")
        # {fmt} is only needed on compilers without a conforming std::format.
        # GCC 13+ defines __cpp_lib_format; earlier versions (e.g. GCC 12 / Debian 12) do not.
        if not (self.settings.compiler == "gcc" and int(str(self.settings.compiler.version)) >= 13):  # type: ignore
            self.requires("fmt/10.2.1")

    def build_requirements(self):
        self.tool_requires("meson/1.3.2")

        # Development tools
        if self.options.with_tests:
            self.tool_requires("cmake/3.25.3")  # For some test dependencies
            self.test_requires("boost-ext-ut/2.1.0")  # boost-ext/ut testing framework
            self.test_requires("doctest/2.4.11")  # logger unit tests

    def configure(self):
        # Configure fPIC based on shared option
        if self.options.shared:
            self.options.rm_safe("fPIC")

        # Platform-specific configuration
        if self.settings.os == "Windows":
            # Windows doesn't need fPIC
            self.options.rm_safe("fPIC")

    def generate(self):
        pc = PkgConfigDeps(self)
        pc.generate()
        # Generate Meson toolchain
        tc = MesonToolchain(self)

        if self.settings.os == "Windows":
            # Pass dependency build info to Meson
            tc.project_options["conan_deps_build_type"] = str(self.settings.build_type)
            tc.project_options["conan_deps_runtime"] = str(
                self.settings.compiler.runtime_type
            )

            # Always use from_buildtype for the application
            tc.b_vscrt = "from_buildtype"

            # Handle CRT conflicts based on dependency runtime
            if self.settings.compiler.runtime_type == "Debug":
                # Dependencies use debug runtime, exclude static debug CRT
                tc.cpp_link_args = ["/NODEFAULTLIB:LIBCMTD"]
            else:
                # Dependencies use release runtime, exclude static release CRT
                tc.cpp_link_args = ["/NODEFAULTLIB:LIBCMT"]
        # Add custom meson options based on conan options
        if not self.options.with_igsc:
            tc.project_options["use_system_igsc"] = False
        if self.options.use_system_levelzero:
            tc.project_options["use_system_levelzero"] = True
        if self.options.use_system_igsc:
            tc.project_options["use_system_igsc"] = True

        tc.generate()

        # Collect license files from all host dependencies into THIRD_PARTY_LICENSES/.
        for dep in self.dependencies.host.values():
            src = os.path.join(dep.package_folder, "licenses")
            dst = os.path.join(self.recipe_folder, "THIRD_PARTY_LICENSES", dep.ref.name)
            copy(self, "*", src, dst)
