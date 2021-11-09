import grpc
import core_pb2_grpc
import os
import xpum_logger as logger

unixSockName = os.environ.get('XPUM_SOCKET_FILE', '/tmp/xpum.sock')
logger.info('using socket file: %s', unixSockName)
channel = grpc.insecure_channel('unix://' + unixSockName)
stub = core_pb2_grpc.XpumCoreServiceStub(channel)

