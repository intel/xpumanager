#pragma once

#include <grpc/grpc.h>
#include <grpc++/grpc++.h>
#include <grpc++/server.h>
#include <grpc++/server_builder.h>
#include <grpc++/server_context.h>

#include "core.pb.h"
#include "core.grpc.pb.h"

class XpumCoreServiceImpl final : public XpumCoreService::Service {
public:
    XpumCoreServiceImpl( void );
    virtual ~XpumCoreServiceImpl();

    virtual grpc::Status getVersion( grpc::ServerContext* context, const google::protobuf::Empty* request,
            XpumVersionInfoArray* response ) override;

    virtual grpc::Status getDeviceList( grpc::ServerContext* context, const google::protobuf::Empty* request,
            XpumDeviceBasicInfoArray* response ) override;
            
    virtual grpc::Status getDeviceProperties( grpc::ServerContext* context, const DeviceId* request, XpumDeviceProperties* response) override;

    virtual grpc::Status getTopology( grpc::ServerContext* context, const DeviceId* request, 
            XpumTopologyInfo* response) override;
};
