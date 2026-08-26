# Helper scripts

* [generate.sh](generate.sh) -- generate OTel code/docs + Helm docs/schema
* [generate-tools.sh](generate-tools.sh) -- Generate `tools` package content (to help with Go deps)
* [install-protoc.sh](install-protoc.sh) -- Check for / install protobuf compiler
* [make-dockerized.sh](make-dockerized.sh) -- Create build container & build [Makefile](../Makefile) targets within that
* [run-integration-tests.sh](run-integration-tests.sh) -- Run integration tests with Go
* [sync-dockerfile-args.sh](sync-dockerfile-args.sh) -- Sync the compute-runtime backend build args with a L0 Go bindings example [Dockerfile](../level-zero-go/examples/Dockerfile)
* [trivy-warmup.sh](trivy-warmup.sh) -- Warm up gomod cache for the license scan

# Script data files

`make-dockerized.sh` uses [checksum files](../level-zero-go/level-zero/) under L0 Go bindings.
