#include "core_stub.h"

#include "core.grpc.pb.h"
#include "xpum_structs.h"

#include <cassert>
#include <grpc++/grpc++.h>

CoreStub::CoreStub() {
    std::string unixSockName{"/tmp/xpum.sock"};
    std::string serverAddr{"unix://" + unixSockName};
    auto channel = grpc::CreateChannel(serverAddr, grpc::InsecureChannelCredentials());
    this->stub = XpumCoreService::NewStub(channel);
}

std::unique_ptr<nlohmann::json> CoreStub::getVersion() {

    assert(this->stub != nullptr);

    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());

    const std::string notDetected = "Not Detected";

    (*json)["xpum_version"] = notDetected;
    (*json)["xpum_version_git"] = notDetected;
    (*json)["level_zero_version"] = notDetected;

    grpc::ClientContext context;
    XpumVersionInfoArray response;
    grpc::Status status = stub->getVersion(&context, google::protobuf::Empty(), &response);
    if (status.ok()) {
        if (response.errormsg().length() == 0) {
            for (int i{0}; i < response.versions_size(); ++i) {
                switch (response.versions(i).version().value()) {
                case XPUM_VERSION:
                    (*json)["xpum_version"] = response.versions(i).versionstring();
                    break;
                case XPUM_VERSION_GIT:
                    (*json)["xpum_version_git"] = response.versions(i).versionstring();
                    break;
                case XPUM_VERSION_LEVEL_ZERO:
                    (*json)["level_zero_version"] = response.versions(i).versionstring();
                    break;
                default:
                    assert(0);
                }
            }
        }
    }

    return json;
}


std::unique_ptr<nlohmann::json> CoreStub::getTopology(DeviceId deviceId) {
    assert(this->stub != nullptr);

    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());

    (*json)["deviceId"] = deviceId.id();

    grpc::ClientContext context;
    XpumTopologyInfo response;
    grpc::Status status = stub->getTopology(&context, deviceId, &response);
    if (status.ok()) {
        if (response.errormsg().length() == 0) {
            (*json)["xpum_affinity_localcpulist"] = response.cpuaffinity().localcpulist();
            (*json)["xpum_affinity_localcpus"] = response.cpuaffinity().localcpus();
            (*json)["xpum_parent_switch_count"] = response.switchcount();
            for (int i{0}; i < response.switchinfo_size(); ++i) {
                std::string switchIdx = std::string("xpum_switch_") + std::to_string(i);
                (*json)[switchIdx] = response.switchinfo(i).switchdevicepath();
            }
        }
    }

    return json;
}
