
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
    DESTINATION 
      py
)

install(
    FILES 
		client/xpumcli
    DESTINATION 
      py/client
)