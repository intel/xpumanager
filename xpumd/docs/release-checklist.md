# XPUMD release checklist

Before release:

* Check version(s) currently specified for XPUMD with `make versions`
* Run `make versions-update` + `make generate-mods` to update version for files under `charts/` [^1]
* Check / update version for the [gRPC API](../exporter/intelxpuinfo/api/deviceinfo/v1alpha1/deviceinfo.proto) module
* Add / update notes for the new release to [CHANGES.md](CHANGES.md)

[^1]: Chart version needs to be a valid sem-version, so version based
on a git tag may be rejected, and needs to be specified explicitly:

```bash
make versions-update XPUMD_VERSION=2.3.1
```
