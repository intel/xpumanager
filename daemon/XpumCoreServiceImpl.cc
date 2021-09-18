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
            device->set_uuid( devices[i].uuid );
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
