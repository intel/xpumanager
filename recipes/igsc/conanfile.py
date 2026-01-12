# Copyright (C) 2025-2026 Intel Corporation
# SPDX-License-Identifier: MIT

from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout
from conan.tools.files import get, copy
import os


class IgscConan(ConanFile):
    name = "igsc"
    version = "0.9.6"
    
    # Package metadata
    license = "Apache-2.0"
    description = "Intel(R) Graphics System Controller Firmware Update Library"
    url = "https://github.com/intel/igsc"
    
    # Settings and options
    settings = "os", "compiler", "build_type", "arch"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "build_cli": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "build_cli": False,  # Disable CLI to avoid linking issues
    }
    
    def requirements(self):
        # IGSC depends on METEE
        self.requires("metee/6.0.0")
    
    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")
        if self.settings.os == "Windows":
            self.options.rm_safe("fPIC")
    
    def layout(self):
        cmake_layout(self)
    
    def source(self):
        # Download from GitHub
        get(self, 
            url=f"https://github.com/intel/igsc/archive/refs/tags/V{self.version}.tar.gz",
            destination=self.source_folder,
            strip_root=True)
    
    def generate(self):
        tc = CMakeToolchain(self)
        
        # Configure options - use the correct CMake option names from IGSC
        tc.variables["ENABLE_CLI"] = False  # Disable CLI to avoid linking issues
        tc.variables["BUILD_SHARED_LIBS"] = self.options.shared
        tc.variables["CMAKE_POSITION_INDEPENDENT_CODE"] = self.options.get_safe("fPIC", True)
        tc.variables["ENABLE_TESTS"] = False  # Also disable tests
        tc.variables["ENABLE_DOCS"] = False   # Disable docs
        
        tc.generate()
    
    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
    
    def package(self):
        cmake = CMake(self)
        cmake.install()
        
        # Copy license
        copy(self, "COPYING", 
             src=self.source_folder, 
             dst=os.path.join(self.package_folder, "licenses"))
    
    def package_info(self):
        # Main library
        self.cpp_info.libs = ["igsc"]
        
        # Include directories
        self.cpp_info.includedirs = ["include"]
        
        # System dependencies
        if self.settings.os == "Linux":
            self.cpp_info.system_libs = ["pthread", "udev"]
        elif self.settings.os == "Windows":
            self.cpp_info.system_libs = ["kernel32", "user32", "advapi32", "setupapi", "cfgmgr32"]
