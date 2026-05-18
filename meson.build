# Copyright (C) 2025-2026 Intel Corporation
# SPDX-License-Identifier: MIT

# Compilers' C++20 support is experimental / partial:
# - https://en.cppreference.com/w/cpp/compiler_support/20.html
# - https://gcc.gnu.org/projects/cxx-status.html
# - https://clang.llvm.org/cxx_status.html
# Buildtype vs. warnings:
# - Buildtype (optimization level) can affect GCC generated warnings
#   as its higher optimization levels do more extensive analysis:
#   - https://mesonbuild.com/Builtin-options.html#details-for-buildtype
#   - https://mesonbuild.com/Builtin-options.html#details-for-warning_level
project('xpum', 'c', 'cpp',
  license : 'MIT',
  default_options : [
    'cpp_std=c++20',
    'buildtype=debugoptimized',
    'warning_level=3',
    'werror=true',
    'b_lto=true',
    ]
)

# File system module
fs = import('fs')

# Read version from file
xpum_version = fs.read('VERSION').strip()

# Project information
project_description = 'Intel(R) XPU Manager and XPU System Management Interface'

# Get build configuration
buildtype = get_option('buildtype')
is_debug = buildtype == 'debug'

# Compiler setup
cpp = meson.get_compiler('cpp')

# Common compiler arguments
compile_args = []
# Common link arguments
link_args = []

# Parse version
ver_arr = xpum_version.split('.')
if ver_arr.length() != 3
  error('Invalid VERSION format: "' + xpum_version + '". Expected format: MAJOR.MINOR.PATCH')
endif
major = ver_arr[0]
minor = ver_arr[1]
patch = ver_arr[2]

# Add version definitions
compile_args += [
  '-DXPUM_VERSION_MAJOR=' + major,
  '-DXPUM_VERSION_MINOR=' + minor,
  '-DXPUM_VERSION_PATCH=' + patch,
]

# Add _DEBUG flag
if is_debug
  compile_args += ['-D_DEBUG']
endif


# Platform detection
host_system = host_machine.system()
is_windows = host_system == 'windows'
is_linux = host_system == 'linux'

if is_windows
  message('Detected Windows platform')
elif is_linux
  message('Detected Linux platform')
else
  error('Unsupported platform: ' + host_system)
endif

# Standard compile flags
if is_windows
  compile_args += [
    '/DUNICODE',
    '/D_UNICODE',
    '/GS',
    '/permissive-',
    '/Zc:__cplusplus',
    '/Zc:preprocessor',
    '/Zc:rvalueCast',
    '-DWIN32',
    '-D_WINDOWS',
  ]

  if get_option('shared_library')
    compile_args += ['-DLIBXPUM_EXPORTS']
  endif
elif is_linux
  compile_args += [
    '-pedantic',
  ]
endif

# Handle CRT conflicts based on dependency vs application build type
if is_windows
  conan_deps_build_type = get_option('conan_deps_build_type')
  conan_deps_runtime = get_option('conan_deps_runtime')

  message('Conan dependencies built with: ' + conan_deps_build_type)
  message('Conan dependencies runtime: ' + conan_deps_runtime)
  deps_are_release = conan_deps_build_type == 'Release'
  app_is_debug = is_debug

  # Handle mixed runtime scenarios
  if deps_are_release and app_is_debug
    message('Mixed build: Release deps + Debug app')
    # Dependencies built with /MD, app with /MDd
    # Exclude the dynamic release CRT that deps pull in
    link_args += ['/NODEFAULTLIB:MSVCRT']
  elif not deps_are_release and not app_is_debug
    message('Mixed build: Debug deps + Release app')
    # Dependencies built with /MDd, app with /MD
    # Exclude the dynamic debug CRT that deps pull in
    link_args += ['/NODEFAULTLIB:MSVCRTD']
  else
    message('Consistent build: same debug level')
  endif
endif

# Compiler warnings
if is_windows
  compile_args += [
    # W4 is pretty exhaustive, could add more as needed. Automatically set by the `warning_level` option
    ]
elif is_linux
  # Note: the 'warning_level' option is set to 3 by default, which is equivalent to '-Wall -Wextra -pedantic'
  if cpp.get_id() == 'clang'
    compile_args += [
      '-Weverything',
      '-Wno-c++98-compat',
      '-Wno-c++98-compat-pedantic',
      '-Wno-c++98-c++11-c++14-compat-pedantic',
      '-Wno-c++20-compat',
      '-Wno-gnu-zero-variadic-macro-arguments',
      '-Wno-undefined-func-template',
      '-Wno-ctad-maybe-unsupported',
      '-Wno-global-constructors',
      '-Wno-exit-time-destructors',
      '-Wno-extra-semi',
      '-Wno-extra-semi-stmt',
      '-Wno-unused-local-typedef',
      '-Wno-undef',
      '-Wno-unknown-warning-option'
    ]
  elif cpp.get_id() == 'gcc'
    # use "release" buildtype (-O3 = better analysis) to see all possible warnings
    compile_args += [
      '-Wimplicit-fallthrough',
      '-Wno-deprecated-declarations',
      '-Wfloat-equal',
      '-Wshadow',
      '-Wcast-align',
      '-Wcast-qual',
      '-Wconversion',
    ]
  endif
endif

# Security/hardening flags
if is_windows
  compile_args += [
    '/guard:cf',
    '/sdl',
    '/analyze',
    '/Gy',
  ]
  link_args += [
    '/CETCOMPAT',
    '/DYNAMICBASE',
    '/NXCOMPAT',
  ]
elif is_linux
  compile_args += [
    '-U_FORTIFY_SOURCE',
    '-D_FORTIFY_SOURCE=3',
    '-fstack-protector-strong',
    '-fstack-clash-protection',
    '-Wformat-security',
    '-D_GLIBCXX_ASSERTIONS',
    '-fcf-protection=full',
    '-fPIE',
  ]
  link_args += [
    '-Wl,-z,noexecstack',
    '-Wl,-z,relro',
    '-Wl,-z,now',
  ]

  # Disable LTO when sanitizers are enabled (they cause significant false positives)
  sanitizers = get_option('b_sanitize')
  if sanitizers != 'none'
    message('Sanitizers enabled (' + sanitizers + '), disabling LTO to avoid conflicts')
    add_project_arguments('-fno-lto', language: ['c', 'cpp'])
    add_project_link_arguments('-fno-lto', language: ['c', 'cpp'])
  endif
endif

# Add platform-specific arguments
if is_windows
  compile_args += ['-DWIN32', '-D_WINDOWS']
  if get_option('shared_library')
    compile_args += ['-DLIBXPUM_EXPORTS']
  endif
elif is_linux
  compile_args += ['-fPIC']
endif

# Target for running static checkers:
# - https://mesonbuild.com/Run-targets.html
# - https://github.com/mesonbuild/meson/issues/2383
run_target('lint', command: 'scripts/lint.sh')

# Target for API documentation
run_target('docs', command: 'scripts/doxygen.sh')

# System dependencies
thread_dep = dependency('threads')

# Dependencies managed by Conan
levelzero_dep = dependency('level-zero', required : true)
igsc_dep = dependency('igsc', required : true)
nlohmann_json_dep = dependency('nlohmann_json', required : true)
cli11_dep = dependency('CLI11', required : true, include_type : 'system')

# Additional Linux-specific dependencies
if is_linux
  hwloc_dep = dependency('hwloc', required : true)
  pciaccess_dep = dependency('pciaccess', required : true)
endif

# Windows-specific libraries
windows_deps = []
if is_windows
  windows_deps = [
    cpp.find_library('kernel32'),
    cpp.find_library('user32'),
    cpp.find_library('psapi'),
    cpp.find_library('setupapi'),
  ]
endif

# Global include directories
global_inc = include_directories('.')

# Specify compile flags before any xpum targets are compiled
add_project_arguments(compile_args, language : 'cpp')
# Specify link flags
add_project_link_arguments(link_args, language: 'cpp')

# Install-path constant: used for resource/ files
if is_linux
  xpum_data_dir = get_option('prefix') / get_option('datadir') / 'xpum'
  add_project_arguments('-DXPUM_DATA_DIR="' + xpum_data_dir + '"', language : 'cpp')
endif

# Build subdirectories
subdir('utility')
subdir('oal')
subdir('hal')
subdir('ial')

# Copy resources to builddir for development builds
if is_linux
  run_command('cp', '-r',
    meson.project_source_root() / 'resources',
    meson.project_build_root(),
    check: false)
elif is_windows
  run_command('xcopy', 
    meson.project_source_root() / 'resources',
    meson.project_build_root() / 'resources',
    '/E', '/I', '/Y', '/Q',
    check: false)
endif

# Copy resources to build dir / ial / cli for development builds
if is_linux
  run_command('cp', '-r',
    meson.project_source_root() / 'resources',
    meson.project_build_root() / 'ial' / 'cli'/ 'resources',
    check: false)
elif is_windows
  run_command('xcopy', 
    meson.project_source_root() / 'resources',
    meson.project_build_root() / 'ial' / 'cli' / 'resources',
    '/E', '/I', '/Y', '/Q',
    check: false)
endif

# Copy test scripts to build dir / ial / cli 
if is_linux
  run_command('cp', '-r',
    meson.project_source_root() / 'scripts' / 'test_LinuxXpum.sh',
    meson.project_build_root() / 'ial' / 'cli' ,
    check: false)
elif is_windows
  run_command('xcopy', 
    meson.project_source_root() / 'scripts' / 'test_WinXpum.ps1',
    meson.project_build_root() / 'ial' / 'cli' ,
    '/E', '/I', '/Y', '/Q',
    check: false)
endif

# Install resources under the FHS data directory.
# Results in: <prefix>/share/xpum/resources/
install_subdir('resources',
  install_dir: get_option('datadir') / 'xpum',
  strip_directory: false)


# Summary
summary_info = {
  'Build type' : buildtype,
  'Platform' : host_system,
  'C++ standard' : get_option('cpp_std'),
  'Level Zero' : levelzero_dep.found() ? 'found' : 'not found',
  'IGSC' : igsc_dep.found() ? 'found' : 'not found',
  'nlohmann_json' : nlohmann_json_dep.found() ? 'found' : 'not found',
}

if is_linux
  summary_info += {'PCIAccess' : pciaccess_dep.found() ? 'found' : 'not found'}
  summary_info += {'hwloc' : hwloc_dep.found() ? 'found' : 'not found'}
endif
