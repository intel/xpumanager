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
