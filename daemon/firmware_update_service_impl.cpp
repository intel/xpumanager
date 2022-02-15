#include "xpum_core_service_impl.h"

namespace xpum::daemon {

::grpc::Status XpumCoreServiceImpl::runFirmwareFlash(::grpc::ServerContext* context, const ::XpumFirmwareFlashJob* request, ::GeneralEnum* response) {
    xpum_firmware_flash_job job;
    job.type = (xpum_firmware_type_enum)request->type().value();
    job.filePath = request->path().c_str();
    response->set_value(xpumRunFirmwareFlash(request->id().id(), &job));

    return grpc::Status::OK;
}

::grpc::Status XpumCoreServiceImpl::getFirmwareFlashResult(::grpc::ServerContext* context, const ::XpumFirmwareFlashTaskRequest* request, ::XpumFirmwareFlashTaskResult* response) {
    xpum_firmware_flash_task_result_t result;

    xpum_result_t res = xpumGetFirmwareFlashResult(request->id().id(), (xpum_firmware_type_t)request->type().value(), &result);
    if (res == XPUM_OK) {
        response->mutable_id()->set_id(request->id().id());
        response->mutable_type()->set_value(request->type().value());
        response->mutable_result()->set_value(result.result);
        response->set_desc("");
        response->set_version("");
    } else {
        response->set_errormsg("Error");
    }

    return grpc::Status::OK;
}
}