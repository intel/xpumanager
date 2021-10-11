from ctypes import *
import os
import uuid
import string
from enum import Enum, IntEnum, unique
import datetime

import grpc
import core_pb2_grpc

from google.protobuf import empty_pb2

class DGMCore:
    def __init__(self):
        unixSockName = '/tmp/xpum.sock'
        channel = grpc.insecure_channel( 'unix://' + unixSockName )
        self.stub = core_pb2_grpc.XpumCoreServiceStub( channel ) 

        self.rawDumpTaskMap = dict()
    
    def getVersion(self):
        resp = self.stub.getVersion( empty_pb2.Empty() )
        if len( resp.errorMsg ) == 0:
            print( "good" )
            for version in resp.versions:
                print( version.versionString )
        else:
            print( "error msg " + resp.errorMsg )
            return 1, "Fail to get version info", None

        return 0, "OK", "NICE"
    
