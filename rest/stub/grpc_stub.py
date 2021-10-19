import grpc
import core_pb2_grpc

unixSockName = '/tmp/xpum.sock'
channel = grpc.insecure_channel( 'unix://' + unixSockName )
stub = core_pb2_grpc.XpumCoreServiceStub( channel )