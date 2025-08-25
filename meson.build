project('xpum', 'cpp',
  version : '1.0.0',
  license : 'MIT',
  default_options : [
    'cpp_std=c++20',
    'buildtype=debugoptimized',
    'warning_level=3'
    ]
)

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

# Dependencies managed by Conan
levelzero_dep = dependency('level-zero', required : true)
igsc_dep = dependency('igsc', required : true)
nlohmann_json_dep = dependency('nlohmann_json', required : true) 

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
# Specify link flags
add_project_link_arguments(link_args, language: 'cpp')

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
  'nlohmann_json' : nlohmann_json_dep.found() ? 'found' : 'not found',
}

if is_linux
  summary_info += {'PCIAccess' : pciaccess_dep.found() ? 'found' : 'not found'}
endif
