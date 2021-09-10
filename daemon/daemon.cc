#include <memory>
#include <string>
#include <thread>
#include <chrono>
#include <pwd.h>
#include <unistd.h>

#include <grpc/grpc.h>
#include <grpc++/grpc++.h>
#include <grpc++/server.h>
#include <grpc++/server_builder.h>
#include <grpc++/server_context.h>

#include "XpumCoreServiceImpl.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

using std::string;
using std::unique_ptr;
using std::cout;
using std::endl;

static const string unixSockName { "/tmp/xpum.sock" };

void runRPCServer() {
    unlink( unixSockName.c_str() );

    string serverAddr( "unix://" + unixSockName );
    
    XpumCoreServiceImpl service;

    grpc::ServerBuilder builder;
    builder.AddListeningPort( serverAddr, grpc::InsecureServerCredentials() );
    //TODO: limit thread pool size
    builder.RegisterService( &service );

    unique_ptr<grpc::Server> server{ builder.BuildAndStart() };
    cout << "RPC server is listening" << endl;

    /*
    passwd* pwd = getpwnam( "xpum_user" );
    int uid = pwd->pw_uid;
    chown( serverAddr, uid, -1 );
    */

    server->Wait();
}

int main( int argc, char* argv[] ) {
    runRPCServer();

    return 0;
}

