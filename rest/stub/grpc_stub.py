import grpc
import core_pb2_grpc
import os
import xpum_logger as logger
import os
import signal
import subprocess

unixSockName = os.environ.get('XPUM_SOCKET_FILE', '/tmp/xpum.sock')
logger.info('using socket file: %s', unixSockName)
channel = grpc.insecure_channel('unix://' + unixSockName)
stub = core_pb2_grpc.XpumCoreServiceStub(channel)


def exit_on_disconnect(func):
    def wrapper(*args, **kwargs):
        g = func.__globals__
        try:
            res = func(*args, **kwargs)
            g["fail_times"] = 0
            return res
        except grpc._channel._InactiveRpcError:
            g = func.__globals__
            old_fail_times = g.get("fail_times", 0)
            ppid = os.getppid()
            process_name = subprocess.check_output(
                "ps -o cmd= {}".format(ppid), shell=True)
            if "gunicorn" in process_name.decode("utf-8") and old_fail_times >= 10:
                os.kill(ppid, signal.SIGINT)
            g["fail_times"] = old_fail_times+1
    return wrapper
