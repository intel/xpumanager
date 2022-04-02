# Run XPU Manager in Docker
*So far, XPUM container image is targeted as a Prometheus exporter.*

## Enable TLS
Generate certificate for TLS and configure REST user credential:
```sh
xpum_src_root=... # e.g., $HOME/XPUManagerGit
mkdir -p rest/conf
sudo ${xpum_src_root}/install/tools/rest/keytool.sh --owner=root --group=root
sudo ${xpum_src_root}/install/tools/rest/rest_config.py --owner=root --group=root
```
Run the XPUM container:
```sh
xpum_image=... # e.g., ccr-registry.caas.intel.com/dms/intel-xpumanager:20211209.163631-2d46da67
docker run --rm --cap-drop ALL --cap-add=SYS_ADMIN \
--publish 29999:29999 \
--device /dev/dri:/dev/dri \
-v $(pwd)/rest/conf:/opt/xpum/rest/conf:ro \
${xpum_image}
```

## Disable TLS
Configure REST user credential:
```sh
xpum_src_root=... # e.g., $HOME/XPUManagerGit
mkdir -p rest/conf
sudo ${xpum_src_root}/install/tools/rest/rest_config.py --owner=root --group=root
```
Run the XPUM container by passing environment variable XPUM_REST_NO_TLS=1:
```sh
xpum_image=...
docker run --rm --cap-drop ALL --cap-add=SYS_ADMIN \
--publish 29999:29999 \
--device /dev/dri:/dev/dri \
-v $(pwd)/rest/conf:/opt/xpum/rest/conf:ro \
-e XPUM_REST_NO_TLS=1 \
${xpum_image}
```

## Support Fabric Throughput
To enable XPUM for reporting fabric throughput metrics, the container must be run in the 'host' network mode (***--network host***).
To avoid port conflict with host network port assignment, you can pass environment variable ***XPUM_REST_PORT*** to the container to specify the XPUM REST server port. For example:


```sh
docker run --rm --cap-drop ALL --cap-add=SYS_ADMIN \
--network host \
--device /dev/dri:/dev/dri \
-v $(pwd)/rest/conf:/opt/xpum/rest/conf:ro \
-e XPUM_REST_NO_TLS=1 \
-e XPUM_REST_PORT=12345 \
${xpum_image}
```
