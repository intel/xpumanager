project('xpum', 'cpp',
  version : '1.0.0',
  license : 'MIT',
  default_options : [
    'cpp_std=c++20',
    'buildtype=debugoptimized',
    'warning_level=3',
    'b_sanitize=undefined',
    'b_coverage=true'
    ]
)

# Build level-zero
cmake = import('cmake')

# Project information
project_description = 'Intel(R) XPU Manager and XPU System Management Interface'

# Get build configuration
buildtype = get_option('buildtype')
is_debug = buildtype == 'debug'

# Compiler setup
cpp = meson.get_compiler('cpp')

# Common compiler arguments
compile_args = []

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
  compile_args = [
    '/DUNICODE',
    '/D_UNICODE',
    '/MD',
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
  compile_args = [
    '-pedantic',
  ]
endif

# Compiler warnings
if is_windows 
  compile_args += [    
    '/W4',
    # W4 is pretty exhaustive, could add more as needed but Linux is more familiar
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
    compile_args += [
      '-Wimplicit-fallthrough',
      '-Wno-unused-parameter',
      '-Wno-unused-variable',
      '-Wno-unused-function',
      '-Wno-missing-field-initializers',
      '-Wno-missing-braces',
      '-Wno-unknown-pragmas',
      '-Wno-deprecated-declarations',
      '-Wfloat-equal',
      '-Wshadow',
      '-Wcast-align',
      '-Wcast-qual',
      '-Wconversion',
    ]
  endif
endif

# Warnings as errors (default = true)
if not get_option('dev')
  if is_windows
    compile_args += ['/WX']
  elif is_linux
    compile_args += ['-Werror']
  endif
endif

# Security/hardening flags
if is_windows
  compile_args += [
    '/guard:cf', 
    '/sdl'        
  ]
elif is_linux
  compile_args += [
    '-U_FORTIFY_SOURCE',
    '-D_FORTIFY_SOURCE=3',
    '-fstack-protector-strong',
    '-fstack-clash-protection',
    '-Wformat-security',
    '-D_GLIBCXX_ASSERTIONS',
    '-Wtrampolines',
  ]
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

# System dependencies
thread_dep = dependency('threads')

# level-zero
if get_option('use_system_levelzero')
  message('Searching for Level Zero dependency using CMake find_package or pkg-config...')
  levelzero_dep = dependency('level-zero', required : true)
else # Standard case using subprojects. This is preferred
  # Try to download and build real Level Zero. See subprojects/level-zero.wrap
  message('Downloading and building Level Zero from source...')
  cmake_opts = cmake.subproject_options()
  cmake_opts.set_install(true)
  levelzero_proj = cmake.subproject('level-zero', options: cmake_opts)
  levelzero_dep = levelzero_proj.get_variable('ze_loader_dep')
endif

if get_option('use_system_igsc')
  message('Searching for IGSC dependency using CMake find_package or pkg-config...')
  igsc_dep = dependency('igsc', required : true)
else
  # IGSC requires metee prior to compilation, if we need to install IGSC via Meson then
  # we first need to ensure metee is available. We will always install and build it manually
  # and not check our system to be consistent with the handling of IGSC
  metee_dep_static = dependency('metee', required : false)
  if not metee_dep_static.found()
    metee_proj = subproject('metee')
    metee_dep_static = metee_proj.get_variable('metee_dep_static')
  endif

  # Try to download and build real IGSC
  message('Downloading and building IGSC from source...')
  
  # Build IGSC using our custom Meson build (with Windows support)
  igsc_proj = subproject('igsc')
  igsc_dep = igsc_proj.get_variable('igsc_dep')
endif

# Additional Linux-specific dependencies
if is_linux
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


# Build subdirectories
subdir('oal')
subdir('hal')
subdir('ial')

# Summary
summary_info = {
  'Build type' : buildtype,
  'Platform' : host_system,
  'C++ standard' : get_option('cpp_std'),
  'Level Zero' : levelzero_dep.found() ? 'found' : 'not found',
  'IGSC' : igsc_dep.found() ? 'found' : 'not found',
}

if is_linux
  summary_info += {'PCIAccess' : pciaccess_dep.found() ? 'found' : 'not found'}
endif
