# Copyright (C) 2025-2026 Intel Corporation
# SPDX-License-Identifier: MIT

# C API examples and tests
# Enable with: -Dbuild_examples=true

if get_option('build_examples')
  c_example = executable(
    'xpum_c_example',
    'examples/c/example.c',
    include_directories: include_directories('.'),
    dependencies: [libxpum_dep, levelzero_dep],
    link_args : is_linux ? ['-pie'] : [],
    install: false,
  )

  run_target('c-example',
    command: c_example,
  )

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
        'CGO_CFLAGS': '-I' + join_paths(meson.project_source_root(), 'ial', 'api'),
        'CGO_LDFLAGS': '-L' + join_paths(meson.project_build_root(), 'hal', 'core') + ' -Wl,-rpath,' + join_paths(meson.project_build_root(), 'hal', 'core'),
      },
      depends: libxpum_shared,
    )

    run_target('go-example',
      command: [go_example],
    )
  endif
endif
