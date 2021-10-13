#include <memory>
#include <string>
#include <thread>
#include <chrono>
#include <pwd.h>
#include <unistd.h>

#include <signal.h>
#include <syslog.h>
#include <fcntl.h>
#include <getopt.h>

#include <grpc/grpc.h>
#include <grpc++/grpc++.h>
#include <grpc++/server.h>
#include <grpc++/server_builder.h>
#include <grpc++/server_context.h>

#include "xpum_api.h"
#include "xpum_structs.h"
#include "XpumCoreServiceImpl.h"

#pragma GCC diagnostic ignored "-Wunused-result"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

using std::string;
using std::unique_ptr;
using std::cout;
using std::endl;

static const string unixSockName { "/tmp/xpum.sock" };

unique_ptr<grpc::Server> server;
int pidFilehandle = -1;
char *pid_file_name = nullptr;
char *cfg_file_name = nullptr;

void print_help(const char * app_name)
{
	printf("\n Usage: %s [OPTIONS]\n\n", app_name);
	printf("  Options:\n");
	printf("   -h --help                 Print this help\n");
	printf("   -p --pid_file  filename   PID file used by daemonized app\n");
	printf("\n");
}

void runRPCServer() {
    syslog(LOG_INFO, "XPUM: start RPC server ...");
    unlink( unixSockName.c_str() );

    string serverAddr( "unix://" + unixSockName );
    
    XpumCoreServiceImpl service;

    grpc::ServerBuilder builder;
    builder.AddListeningPort( serverAddr, grpc::InsecureServerCredentials() );
    //TODO: limit thread pool size
    builder.RegisterService( &service );

    server = builder.BuildAndStart();
    syslog(LOG_INFO, "XPUM: RPC server is listening");

    /*
    passwd* pwd = getpwnam( "xpum_user" );
    int uid = pwd->pw_uid;
    chown( serverAddr, uid, -1 );
    */

    server->Wait();    
}

void daemonProcess(){
    pid_t pid, sid;
    int fd;
    
    pid = fork();
    if(pid < 0) {
        syslog(LOG_ERR, "XPUM: An error occurred fork %d.", pid);
        exit(EXIT_FAILURE);
    } else if(pid > 0) {
        exit(EXIT_SUCCESS);
    }
    // let the child process becomes session leader
    sid = setsid();
    if(sid < 0){
        syslog(LOG_ERR, "XPUM: An error occurred setsid %d.", sid);
        exit(EXIT_FAILURE);
    }

    // Ignore signal sent from child to parent process
    signal(SIGCHLD, SIG_IGN);

    // second time
    pid = fork();
    if(pid < 0) {
        syslog(LOG_ERR, "XPUM: An error occurred fork %d.", pid);
        exit(EXIT_FAILURE);
    } else if(pid > 0) {
        exit(EXIT_SUCCESS);
    }

    //umask(22);
    chdir("/");
    
    for (fd = sysconf(_SC_OPEN_MAX); fd > 0; fd--) {
		close(fd);
	}
    /* Reopen stdin (fd = 0), stdout (fd = 1), stderr (fd = 2) */
	stdin = fopen("/dev/null", "r");
	stdout = fopen("/dev/null", "w+");
	stderr = fopen("/dev/null", "w+");
	
    syslog(LOG_INFO, "XPUM: XPUM daemon started.");
}

void writePID() {
	if (pid_file_name != nullptr)
	{
		char str[256];
		pidFilehandle = open(pid_file_name, O_RDWR|O_CREAT, 0600);
		if (pidFilehandle < 0) {
            syslog(LOG_ERR, "XPUM: Could not open PID file %s.", pid_file_name);
			exit(EXIT_FAILURE);
		}
		if (lockf(pidFilehandle, F_TLOCK, 0) < 0) {
            syslog(LOG_ERR, "XPUM: Could not lock PID file %s.", pid_file_name);
			exit(EXIT_FAILURE);
		}
		sprintf(str, "%d\n", getpid());
		//Write PID to lockfile 
		write(pidFilehandle, str, strlen(str));
	}
}

void signalHandler(int sig){
    switch(sig){
        case SIGINT:
        case SIGTERM:
        case SIGKILL:
            server->Shutdown();
            syslog(LOG_WARNING, "XPUM: recieved SIGTERM signal %d, service shutdown.", sig);
            break;        
        default: 
            syslog(LOG_INFO, "XPUM: recieved unhandled signal %d", sig);
            break;
    }
}

int main( int argc, char* argv[] ) {
    static struct option long_options[] = {
		{"conf_file", required_argument, 0, 'c'},
		{"help", no_argument, 0, 'h'},
		{"pid_file", required_argument, 0, 'p'},
		{NULL, 0, 0, 0}
	};
	int value, option_index = 0;

	while ((value = getopt_long(argc, argv, "c:p:h", long_options, &option_index)) != -1) {
		switch (value) {
			case 'c':
				cfg_file_name = strdup(optarg);
				break;
            case 'p':
                pid_file_name = strdup(optarg);
                break;
			case 'h':
				print_help(argv[0]);
				return EXIT_SUCCESS;
			case '?':
				print_help(argv[0]);
				return EXIT_FAILURE;
			default:
				break;
		}
	}
    writePID();
    signal(SIGINT, signalHandler);
    signal(SIGKILL, signalHandler);
    signal(SIGTERM, signalHandler);

    syslog(LOG_INFO, "XPUM: Load DGMCore library");
    xpum_result_t res = xpumInit();
    if(res != XPUM_OK){
        syslog(LOG_ERR, "XPUM: Load DGMCore failed! %d", res);
        return 1;
    }
    
    syslog(LOG_INFO, "XPUM: start XPUM RPC Server.");
    runRPCServer();

    syslog(LOG_INFO, "XPUM: Shut down DGMCore.");
    res = xpumShutdown();
    syslog(LOG_INFO, "XPUM: xpum service is closed.");
    if(pidFilehandle != -1) {
        close(pidFilehandle);
    }
    return 0;
}

