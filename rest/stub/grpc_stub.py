#
# Copyright (C) 2021-2022 Intel Corporation
# SPDX-License-Identifier: MIT
# @file grpc_stub.py
#

import grpc
import core_pb2_grpc
import os
import xpum_logger as logger
import signal

unixSockName = os.environ.get('XPUM_SOCKET_FILE', '/tmp/xpum_p.sock')
logger.info('using socket file: %s', unixSockName)
channel = grpc.insecure_channel('unix://' + unixSockName)
stub = core_pb2_grpc.XpumCoreServiceStub(channel)

gunicorn_pid_file = os.path.dirname(os.path.realpath(__file__)) + "/../gunicorn.pid"


def exit_on_disconnect(func):
    def wrapper(*args, **kwargs):
        g = func.__globals__
        try:
            res = func(*args, **kwargs)
            g["fail_times"] = 0
            return res
        except grpc._channel._InactiveRpcError as e:
            g = func.__globals__
            old_fail_times = g.get("fail_times", 0)
            ppid = os.getppid()
            if os.path.exists(gunicorn_pid_file) and os.path.isfile(gunicorn_pid_file):
                with open(gunicorn_pid_file, "r") as pidFile:
                    pidStr = pidFile.read()
                    if int(pidStr) == ppid:
                        if old_fail_times >= 10:
                            os.kill(ppid, signal.SIGINT)
                        g["fail_times"] = old_fail_times+1
            raise e
    return wrapper
