# Copyright (C) 2025-2026 Intel Corporation
# SPDX-License-Identifier: MIT

from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout
from conan.tools.files import get, copy
import os


class LevelZeroConan(ConanFile):
    name = "level-zero"
    version = "1.27.0"
    
    # Package metadata
    license = "MIT"
    description = "Intel(R) Graphics Compute Runtime for oneAPI Level Zero"
    url = "https://github.com/oneapi-src/level-zero"
    
    # Settings and options
    settings = "os", "compiler", "build_type", "arch"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "build_tests": [True, False],
        "build_samples": [True, False],
    }
    default_options = {
        "shared": True,
        "fPIC": True,
        "build_tests": False,
        "build_samples": False,
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
            url=f"https://github.com/oneapi-src/level-zero/archive/refs/tags/v{self.version}.tar.gz",
            destination=self.source_folder,
            strip_root=True)
    
    def generate(self):
        tc = CMakeToolchain(self)
        
        # Configure CMake options
        tc.variables["LZ_BUILD_TESTS"] = self.options.build_tests
        tc.variables["LZ_BUILD_SAMPLES"] = self.options.build_samples
        tc.variables["LZ_BUILD_TOOLS"] = False
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
        copy(self, "LICENSE", 
             src=self.source_folder, 
             dst=os.path.join(self.package_folder, "licenses"))
    
    def package_info(self):
        # Main library
        self.cpp_info.libs = ["ze_loader"]
        
        # Include directories
        self.cpp_info.includedirs = ["include/level_zero"]
        
        # System dependencies
        if self.settings.os == "Linux":
            self.cpp_info.system_libs = ["dl", "pthread"]
        elif self.settings.os == "Windows":
            self.cpp_info.system_libs = ["kernel32", "user32"]
        
        # Set component for better dependency management
        self.cpp_info.set_property("cmake_file_name", "level-zero")
        self.cpp_info.set_property("cmake_target_name", "level-zero::level-zero")
