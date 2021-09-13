#include <iostream>
#include <memory>
#include <vector>

#include <grpc/grpc.h>
#include <grpc++/grpc++.h>
#include <grpc++/server.h>
#include <grpc++/server_builder.h>
#include <grpc++/server_context.h>

#include "core.pb.h"
#include "core.grpc.pb.h"

class ClientSample {
public:
    ClientSample( std::shared_ptr<grpc::Channel> channel ) : stub( XpumCoreService::NewStub( channel ) ) {
    }

    ~ClientSample() {
    }

    void getVersion() {
        grpc::ClientContext context;
        XpumVersionInfoArray response;
        grpc::Status status = stub->getVersion( &context, google::protobuf::Empty(), &response );
        if ( status.ok() ) {
            if ( response.errormsg().length() == 0 ) {
                for ( int i{ 0 }; i < response.versions_size(); ++i ) {
                    std::cout << response.versions( i ).versionstring() << std::endl;
                }
            }
            else {
                std::cerr << response.errormsg() << std::endl;
            }
        }
        else {
            std::cerr << "RPC error" << std::endl;
        }
     }

private:
    std::unique_ptr<XpumCoreService::Stub> stub;
};
 
static const std::string unixSockName { "/tmp/xpum.sock" };

#ifdef DEBUG
int main( int argc, char* argv[] ) {
    std::string serverAddr{ "unix://" + unixSockName };
    ClientSample sample( grpc::CreateChannel( serverAddr, grpc::InsecureChannelCredentials() ) );
    sample.getVersion();

    return 0;
}
#endif
