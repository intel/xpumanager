# oneAPI Level Zero Go Bindings

Go bindings for the **oneAPI Level Zero** API.

See the
[Level Zero Specification](https://oneapi-src.github.io/level-zero-spec/level-zero/latest/)
for details.

> [!CAUTION]
> Experimental. This project is in early development.

## Features

- Idiomatic Go wrappers for Level Zero functions
- Zero-copy access to structs with cgo-generated types
- Strongly typed wrappers for enums and handles
- Stringers for enum and error types

## Requirements

- Go 1.24 or later
- oneAPI [Level Zero Loader](https://github.com/oneapi-src/level-zero)
- Intel(R) [Graphics Compute Runtime](https://github.com/intel/compute-runtime)

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
   low-level cgo bindings and types from the Level Zero C headers. The
   generated code
2. Use [cgo](https://pkg.go.dev/cmd/cgo) to bootstrap Go type definitions from
   the C types.
3. Run [./hack/types-mangle](./hack/types-mangle) to adjust the generated
   types. E.g. fix types of some struct fields where we lose the original type
   information during the cgo generation.
4. Use [stringer](https://pkg.go.dev/golang.org/x/tools/cmd/stringer) to
   generate stringers for enums.

## FAQ

**Is Windows supported?**

No yet.

**What Level Zero features are supported?**

The functionality currently covers the sysman API, with the exception that
custom extensions of the functions are not yet supported.
