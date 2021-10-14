
install(
    FILES 
		DGM.py
		DGMCore.py
    api_pb2_grpc.py
    api_pb2.py
    kube_pod_resource.py
    prometheus_exporter.py
    README.md
    requirements.txt
    DESTINATION ${CPACK_PACKAGING_INSTALL_PREFIX}/py
)

install(
    FILES 
		client/xpumcli
    DESTINATION ${CPACK_PACKAGING_INSTALL_PREFIX}py/client
)

install(
    FILES 
		xpum-rest.service
    DESTINATION ${CPACK_PACKAGING_INSTALL_PREFIX}
)
