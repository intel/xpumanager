# Copyright (C) 2025-2026 Intel Corporation
# SPDX-License-Identifier: MIT

from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout
from conan.tools.files import get, copy, save
import os


class MeteeConan(ConanFile):
    name = "metee"
    version = "6.0.0"
    
    # Package metadata
    license = "Apache-2.0"
    description = "Intel(R) ME Test Environment Library"
    url = "https://github.com/intel/metee"
    
    # Settings and options
    settings = "os", "compiler", "build_type", "arch"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
    }
    
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
            url=f"https://github.com/intel/metee/archive/refs/tags/{self.version}.tar.gz",
            destination=self.source_folder,
            strip_root=True)
    
    def generate(self):
        tc = CMakeToolchain(self)
        
        # Configure for static/shared library
        if self.options.shared:
            tc.variables["BUILD_SHARED_LIBS"] = True
        else:
            tc.variables["BUILD_SHARED_LIBS"] = False
            
        tc.variables["CMAKE_POSITION_INDEPENDENT_CODE"] = self.options.get_safe("fPIC", True)
        
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
        # Main library name - metee always builds as 'metee' regardless of shared/static
        self.cpp_info.libs = ["metee"]
        
        # Include directories
        self.cpp_info.includedirs = ["include"]
        
        # System dependencies
        if self.settings.os == "Linux":
            self.cpp_info.system_libs = ["pthread"]
        elif self.settings.os == "Windows":
            self.cpp_info.system_libs = ["kernel32", "user32", "advapi32", "cfgmgr32"]
