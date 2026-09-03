Build XPUMD image using locally downloaded driver DEB packages:

* Download new / relevant Level-Zero DEB packages to a subdir here:
  + L0 loader:
    - libze1*.deb
    - libze-dev*.deb
  + L0 GPU backend and its dependencies:
    - libigc1*.deb
    - libigdfcl1*.deb
    - intel-igc*.deb  -- alternative for above 2 compiler libraries
    - libigdgmm*.deb
    - libigsc*.deb
    - libmetee*.deb
    - libze-intel-gpu*.deb -- actual backend
* Make sure relevant XPUMD branch or tag has been checked out

Then build XPUMD image with the downloaded driver packages:
```
./docker-build.sh  <my-registry/project>  <driver-DEBs-subdir>
```

(Script will forward `http_proxy` and `https_proxy` variables in case
packages need to be loaded through proxy, set `BACKEND=local`, specify
`FILES_DIR` for driver DEB files, and set Docker image tag with XPUMD
repo source & driver backend versions.)
