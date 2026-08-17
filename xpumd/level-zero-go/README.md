# oneAPI Level-Zero Go Bindings

Go bindings for the **oneAPI Level-Zero** API.

See the
[Level-Zero Specification](https://oneapi-src.github.io/level-zero-spec/level-zero/latest/)
for details.

> [!CAUTION]
> Experimental. This project is in early development.

## Features

- Idiomatic Go wrappers for the Level-Zero Sysman functions
- Zero-copy access to structs with cgo-generated types
- Strongly typed wrappers for enums and handles
- Stringers for enum and error types

## Requirements

- Go 1.24 or later
- oneAPI [Level-Zero Loader + headers](https://github.com/oneapi-src/level-zero) v1.25 or later
- Intel(R) [Graphics Compute Runtime](https://github.com/intel/compute-runtime)
  - Level-Zero backend needed for running the code using the bindings
- [IGSC](https://github.com/intel/igsc) - Intel Graphics System Controller library
  - Required by Sysman firmware functionality
    (NOTE: Sysman does not link this directly but loads it at runtime)
- libnl-genl - Generic Netlink library
  - Required by Sysman RAS and fabric functionality
    (NOTE: Sysman does not link this directly but loads it at runtime)

> [!NOTE]
> The oneAPI Level-Zero version is checked at build time. Incompatible versions
> will cause build errors like
> ```golang
> ../core/core_static.go:29:5: invalid array length _level_zero_header_minor_API_version_too_old (constant -2 of type int)
> ```

## Examples

The [examples/](./examples) directory is a separate Go module with small
programs built on top of the bindings. Build binaries with:

```bash
make -C examples build
```

Container images can be built with:

```bash
make -C examples images
```

By default, the images are built with a released and packaged version of the
Level-Zero backend GPU driver. However, the container images also support
building the backend driver from sources
([Intel Graphics Compute Runtime](https://github.com/intel/compute-runtime),
pinned to the revision that the vendored Intel headers come from):

```bash
make -C examples images BACKEND=src
```

This is needed for experimental extensions which are not yet in any released
version of the backend.

Run an image with the GPU devices passed in, and with the tracefs of the host
mounted for the info log functionality:

```bash
docker run --rm --device /dev/dri --cap-drop ALL --cap-add PERFMON \
           -v /sys/kernel/tracing:/sys/kernel/tracing \
           registry.local/level-zero-go/list-sysman-exp:latest /entrypoint -infolog-format metadata
```

> [!NOTE]
> The info log functionality does not require any additional capabilities, but the
> container has to run as root (the default) as the tracefs of the host is only
> accessible to root. `PERFMON` is needed for the engine information.

## Development

Re-generate the bindings with

```bash
make generate
```

or

```bash
make generate-dockerized
```

### Under the hood

1. [c-for-go](https://github.com/xlab/c-for-go) tool is used to generate the
   low-level _cgo_ bindings and types from the Level-Zero C headers.
2. Use [cgo](https://pkg.go.dev/cmd/cgo) to bootstrap Go type definitions from
   the C types.
3. Run [./hack/types-mangle](./hack/types-mangle) to adjust the generated
   types. E.g. fix struct field types lost in the cgo bindings generation.
4. Use [stringer](https://pkg.go.dev/golang.org/x/tools/cmd/stringer) to
   generate stringers for enums.

### Intel extension headers

The Intel-specific experimental extensions (the `sysman/exp/intel` package) are
declared in a header that is not part of the oneAPI Level-Zero release packages.
Copies of the required headers are vendored in `include/intel/`. Update them against a
[Graphics Compute Runtime](https://github.com/intel/compute-runtime) tree with

```bash
./hack/vendor-intel-header.sh <path-to-compute-runtime>
```

The script also updates the `examples/Dockerfile` so that the same git commit
and correct dependencies are used when doing the `BACKEND=src` build.

## FAQ

**Is Windows supported?**

Not yet.

**What Level-Zero features are supported?**

Go bindings currently cover the Sysman API part of Level-Zero, except
for the deprecated functions (which will not be supported). Experimental
functionality lives in the `sysman/exp` package, and the Intel-specific
experimental extensions in the `sysman/exp/intel` package.
