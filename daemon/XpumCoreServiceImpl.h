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

    virtual ::grpc::Status groupCreate(::grpc::ServerContext* context, const ::GroupName* request, 
                ::GroupInfo* response) override;
    virtual ::grpc::Status groupDestory(::grpc::ServerContext* context, const ::GroupId* request, 
                ::GroupInfo* response) override;
    virtual ::grpc::Status groupAddDevice(::grpc::ServerContext* context, const ::GroupAddRemoveDevice* request,
                ::GroupInfo* response) override;
    virtual ::grpc::Status groupRemoveDevice(::grpc::ServerContext* context, const ::GroupAddRemoveDevice* request, 
                ::GroupInfo* response) override;
    virtual ::grpc::Status groupGetInfo(::grpc::ServerContext* context, const ::GroupId* request, 
                ::GroupInfo* response) override;
    virtual ::grpc::Status getAllGroupIds(::grpc::ServerContext* context, const ::google::protobuf::Empty* request,
                ::GroupIdArray* response) override;

};
