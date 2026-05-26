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
> ../core/core_static.go:26:5: invalid array length ... (constant -1 of type int)
> ```

## Development

Re-generate the bindings with

```base
make generate
```

or

```base
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

## FAQ

**Is Windows supported?**

No yet.

**What Level-Zero features are supported?**

Go bindings currently cover the Sysman API part of Level-Zero, except
for the functions for custom extensions (which are not yet supported),
and deprecated functions (which will not be supported).
