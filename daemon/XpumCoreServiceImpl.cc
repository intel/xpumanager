#include "XpumCoreServiceImpl.h"

#include "xpum_api.h"
#include "xpum_structs.h"

#include <iostream>

XpumCoreServiceImpl::XpumCoreServiceImpl( void ) : XpumCoreService::Service() {
}

XpumCoreServiceImpl::~XpumCoreServiceImpl() {
}

grpc::Status XpumCoreServiceImpl::getVersion( grpc::ServerContext* context, const google::protobuf::Empty* request,
        XpumVersionInfoArray* response ) {
    std::cout << "call get version" << std::endl;
    
    int count{ 0 };
    xpum_result_t res = xpumVersionInfo( nullptr, &count );
    if ( res == XPUM_OK ) {
        xpum_version_info versions[count];
        res = xpumVersionInfo( versions, &count );
        if ( res == XPUM_OK ) {
            for ( int i{ 0 }; i < count; ++i ) {
                XpumVersionInfoArray_XpumVersionInfo* info = response->add_versions();
                info->mutable_version()->set_value( versions[i].version );
                info->set_versionstring( versions[i].versionString );
                std::cout << "version: " << versions[i].versionString << std::endl;
            }
        }
    }

    if ( res != XPUM_OK ) {
        response->set_errormsg( "Error" );
    }

    return grpc::Status::OK;
}

grpc::Status XpumCoreServiceImpl::getDeviceList( grpc::ServerContext* context, const google::protobuf::Empty* request,
        XpumDeviceBasicInfoArray* response ) {

    int count { 0 };
    xpum_device_basic_info devices[XPUM_MAX_NUM_DEVICES];

    xpum_result_t res = xpumGetDeviceList( devices, &count );
    if ( res == XPUM_OK ) {
        for ( int i{ 0 }; i < count; ++i ) {
            XpumDeviceBasicInfoArray_XpumDeviceBasicInfo* device = response->add_info();
            device->mutable_id()->set_id( devices[i].deviceId );
            device->mutable_type()->set_value( devices[i].type );
            device->set_uuid( std::string(devices[i].uuid,sizeof(devices[i].uuid)) );
            device->set_devicename( devices[i].deviceName );
            device->set_pciedeviceid( devices[i].PCIDeviceId );
            device->set_subdeviceid( devices[i].SubDeviceId );
            device->set_pcibdfaddress( devices[i].PCIBDFAddress );
            device->set_vendorname( devices[i].VendorName );
        }
    }
    else {
        response->set_errormsg( "Error" );
    }

    return grpc::Status::OK;
}

grpc::Status XpumCoreServiceImpl::getDeviceProperties( grpc::ServerContext* context, const DeviceId* request, XpumDeviceProperties* response) {
    xpum_device_properties_t data;
    auto res = xpumGetDeviceProperties(request->id(), &data);
    if (res == XPUM_OK) {
        for (int i = 0; i < data.propertyLen; i++) {
            auto &prop = data.properties[i];
            auto propRpc = response->add_properties();
            propRpc->set_name(prop.name);
            propRpc->set_value(prop.value);
        }
    } else {
        response->set_errormsg("Error");
    }
    return grpc::Status::OK;
}

grpc::Status XpumCoreServiceImpl::getTopology( grpc::ServerContext* context, const DeviceId* request, 
        XpumTopologyInfo* response) {
    std::cout << "call get topology" << std::endl;
    xpum_topology_t * topo = (xpum_topology_t*) malloc(sizeof(xpum_topology_t));
    std::size_t size = sizeof(xpum_topology_t);
    xpum_result_t res = xpumGetTopology(request->id(), topo, &size);

    if(res == XPUM_BUFFER_TOO_SMALL){
        free(topo);
        topo = (xpum_topology_t*) malloc(size);
        res = xpumGetTopology(request->id(), topo, &size);
    }

    if ( res == XPUM_OK ) {
        response->mutable_id()->set_id(topo->deviceId);
        response->mutable_cpuaffinity()->set_localcpulist(topo->cpuAffinity.localCPUList);
        response->mutable_cpuaffinity()->set_localcpus(topo->cpuAffinity.localCPUs);
        response->set_switchcount(topo->switchCount);
        for(uint32_t i{ 0 }; i < topo->switchCount; ++i){
            XpumTopologyInfo_XpumSwitchInfo * parentSwitch = response->add_switchinfo();
            parentSwitch->set_switchdevicepath(topo->switches[i].switchDevicePath);
        }
    } else {
        response->set_errormsg( "Error" );
    }

    free(topo);
    topo = nullptr;
    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::groupCreate(::grpc::ServerContext* context, const ::GroupName* request, 
                ::GroupInfo* response) {
    std::cout << "call group create" << std::endl;
    xpum_group_id_t id;
    xpum_result_t res = xpumGroupCreate(request->name().c_str(), &id);
    if ( res == XPUM_OK ) {
        response->set_id(id);
        response->set_groupname(request->name());
        response->set_count(0);
    } else {
        response->set_errormsg( "Error" );
    }

    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::groupDestory(::grpc::ServerContext* context, const ::GroupId* request, 
                ::GroupInfo* response) {
    std::cout << "call group destory" << std::endl;
    xpum_result_t res = xpumGroupDestroy(request->id());

    if ( res == XPUM_OK ) {
        response->set_id(request->id());
    } else {
        response->set_errormsg( "Error" );
    }

    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::groupAddDevice(::grpc::ServerContext* context, const ::GroupAddRemoveDevice* request,
                ::GroupInfo* response)  {
    std::cout << "call group add device" << std::endl;

    xpum_result_t res = xpumGroupAddDevice(request->groupid(), request->deviceid());
    if ( res == XPUM_OK ) {
        response->set_id(request->groupid());
    } else {
        response->set_errormsg( "Error" );
    }

    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::groupRemoveDevice(::grpc::ServerContext* context, const ::GroupAddRemoveDevice* request, 
                ::GroupInfo* response)  {
    std::cout << "call group remove device" << std::endl;

    xpum_result_t res = xpumGroupRemoveDevice(request->groupid(), request->deviceid());
    if ( res == XPUM_OK ) {
        response->set_id(request->groupid());
    } else {
        response->set_errormsg( "Error" );
    }

    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::groupGetInfo(::grpc::ServerContext* context, const ::GroupId* request, 
                ::GroupInfo* response)  {
    std::cout << "call group get info" << std::endl;

    xpum_group_info_t info;
    xpum_result_t res = xpumGroupGetInfo(request->id(), &info);
    if ( res == XPUM_OK ) {
        response->set_id(request->id());
        response->set_groupname(info.groupName);
        response->set_count(info.count);

        for(int i{0}; i<info.count; i++){
            DeviceId * deviceid = response->add_devicelist();
            deviceid->set_id(info.deviceList[i]);
        }
    } else {
        response->set_errormsg( "Error" );
    }

    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::getAllGroupIds(::grpc::ServerContext* context, const ::google::protobuf::Empty* request,
                ::GroupIdArray* response)  {
    std::cout << "call get all group id" << std::endl;

    xpum_group_id_t groups[XPUM_MAX_NUM_GROUPS];
    int count = XPUM_MAX_NUM_GROUPS;
    xpum_result_t res = xpumGetAllGroupIds(groups, &count);
    if ( res == XPUM_OK ) {
        response->set_count(count);

        for(int i{0}; i<count; i++){
            GroupId * groupid = response->add_grouplist();
            groupid->set_id(groups[i]);
        }
    } else {
        response->set_errormsg( "Error" );
    }

    return grpc::Status::OK;
}