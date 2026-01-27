#Copyright © 2025 Intel Corporation
#SPDX - License - Identifier : MIT

#C API compatibility layer for legacy XPUM applications
#This layer provides backward compatibility with the original XPUM API
#while using the modern HAL layer internally

api_sources = files(
  'xpum_api.cpp',
)

api_headers = files(
  'xpum_api.h',
  'xpum_structs.h',
)

#Dependencies
threads_dep = dependency('threads', required: true)
level_zero_dep = dependency('level-zero', required: true)

#Build the shared library
libxpum_api = shared_library(
  'xpum_api',
  api_sources,
  include_directories: [
    include_directories('.'),
    include_directories('../..'),
    include_directories('../../hal/core'),
    include_directories('../../hal/fwupd'),
    include_directories('../../oal'), 
  ],
  dependencies: [
    threads_dep,
    level_zero_dep,
    libxpum_dep,  # Link against HAL library
  ],
  install: true,
  install_dir: get_option('libdir'),
  version: xpum_version,
  soversion: xpum_version.split('.')[0],
  gnu_symbol_visibility: 'default',
  cpp_args: ['-DXPUM_api_EXPORTS'],
)

#Install headers
install_headers(
  api_headers,
  subdir: 'xpum',
)

#Generate pkg - config file
pkg = import('pkgconfig')
pkg.generate(
  libxpum_api,
  name: 'xpum_api',
  description: 'XPUM C API compatibility layer for Go daemon and legacy applications',
  version: '1.0.0',
  subdirs: 'xpum',
)

#Create a dependency object for other parts of the build
xpum_api_dep = declare_dependency(
  link_with: libxpum_api,
  include_directories: include_directories('.'),
)

#Build C example
c_example = executable(
  'xpum_c_example',
  'examples/c/example.c',
  include_directories: include_directories('.'),
  link_with: libxpum_api,
  dependencies: [level_zero_dep],
  build_by_default: false,
  install: false,
)

#Build Go example(requires Go compiler)
go_prog = find_program('go', required: false)
if go_prog.found()
  go_example = custom_target(
    'xpum_go_example',
    output: 'xpum_go_example',
    input: 'examples/go/example.go',
    command: [
      go_prog,
      'build',
      '-o', '@OUTPUT@',
      '@INPUT@',
    ],
    env: {
      'CGO_CFLAGS': '-I' + meson.current_source_dir(),
      'CGO_LDFLAGS': '-L' + meson.current_build_dir() + ' -Wl,-rpath,' + meson.current_build_dir(),
    },
    depends: libxpum_api,
    build_by_default: false,
  )
  
  run_target('go-example',
    command: ['sh', '-c', '@0@'.format(join_paths(meson.current_build_dir(), 'xpum_go_example'))],
    depends: go_example,
  )
endif

#Convenience targets
run_target('c-example',
  command: c_example,
)
