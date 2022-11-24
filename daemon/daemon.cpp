/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file daemon.cpp
 */

#include <fcntl.h>
#include <getopt.h>
#include <grpc++/grpc++.h>
#include <grpc++/server.h>
#include <grpc++/server_builder.h>
#include <grpc++/server_context.h>
#include <grpc/grpc.h>
#include <pwd.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <memory>
#include <sstream>
#include <string>
#include <thread>

#include "logger.h"
#include "xpum_api.h"
#include "xpum_core_service_impl.h"
#include "xpum_core_service_unprivileged_impl.h"
#include "xpum_structs.h"

#pragma GCC diagnostic ignored "-Wunused-result"

namespace xpum::daemon {

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

using std::cout;
using std::endl;
using std::string;
using std::unique_ptr;

static const string defaultSockDir{"/tmp/"};
static const string defaultPrivilegedSockName{"xpum_p.sock"};
static const string defaultUnprivilegedSockName{"xpum_up.sock"};

int pidFilehandle = -1;
char* pid_file_name = nullptr;
char* sock_file_dir = nullptr;
char* dump_folder_name = nullptr;
char* log_file_name = nullptr;
char* enabled_metrics = nullptr;
std::size_t log_max_size = 10 * 1024 * 1024;
std::size_t log_max_files = 3;
std::string log_level = "";

std::mutex xpummutex;
std::atomic_bool stop(false);
std::unique_lock<std::mutex> lock(xpummutex);
std::condition_variable cv;

std::string XpumCoreServiceImpl::dumpRawDataFileFolder;

void print_help(const char* app_name) {
    printf("\n Usage: %s [OPTIONS]\n\n", app_name);
    printf("  Options:\n");
    printf("   -h, --help                       print this help\n");
    printf("   -p, --pid_file=filename          PID file used by daemonized app\n");
    printf("   -s, --socket_folder=foldername   folder for socket files used by daemonized app\n");
    printf("   -d, --dump_folder=foldername     dump folder used by daemonized app\n");
    printf("       --log_level=LEVEL            log level (trace, debug, info, warn, error)\n");
    printf("   -l, --log_file=filename          logfile to write\n");
    printf("       --log_max_size=number        max size of log file in MB\n");
    printf("       --log_max_files=number       max number of log files\n");
    printf("   -m, --enable_metrics=METRICS     list enabled metric indexes, seperated by comma,\n");
    printf("                                    use hyphen to indicate a range (e.g., 0,4-7,27-29)\n");
    printf("        Index   Metric                                              Default\n");
    printf("        -----   --------------------------------------------------  -------\n");
    printf("        0       GPU_UTILIZATION                                     on\n");
    printf("        1       EU_ACTIVE                                           off\n");
    printf("        2       EU_STALL                                            off\n");
    printf("        3       EU_IDLE                                             off\n");
    printf("        4       POWER                                               on\n");
    printf("        5       ENERGY                                              on\n");
    printf("        6       GPU_FREQUENCY                                       on\n");
    printf("        7       GPU_CORE_TEMPERATURE                                on\n");
    printf("        8       MEMORY_USED                                         on\n");
    printf("        9       MEMORY_UTILIZATION                                  on\n");
    printf("        10      MEMORY_BANDWIDTH                                    on\n");
    printf("        11      MEMORY_READ                                         on\n");
    printf("        12      MEMORY_WRITE                                        on\n");
    printf("        13      MEMORY_READ_THROUGHPUT                              on\n");
    printf("        14      MEMORY_WRITE_THROUGHPUT                             on\n");
    printf("        15      ENGINE_GROUP_COMPUTE_ALL_UTILIZATION                on\n");
    printf("        16      ENGINE_GROUP_MEDIA_ALL_UTILIZATION                  on\n");
    printf("        17      ENGINE_GROUP_COPY_ALL_UTILIZATION                   on\n");
    printf("        18      ENGINE_GROUP_RENDER_ALL_UTILIZATION                 on\n");
    printf("        19      ENGINE_GROUP_3D_ALL_UTILIZATION                     on\n");
    printf("        20      RAS_ERROR_CAT_RESET                                 on\n");
    printf("        21      RAS_ERROR_CAT_PROGRAMMING_ERRORS                    on\n");
    printf("        22      RAS_ERROR_CAT_DRIVER_ERRORS                         on\n");
    printf("        23      RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE              on\n");
    printf("        24      RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE            on\n");
    printf("        25      RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE            on\n");
    printf("        26      RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE          on\n");
    printf("        27      RAS_ERROR_CAT_NON_COMPUTE_ERRORS_CORRECTABLE        on\n");
    printf("        28      RAS_ERROR_CAT_NON_COMPUTE_ERRORS_UNCORRECTABLE      on\n");
    printf("        29      GPU_REQUEST_FREQUENCY                               on\n");
    printf("        30      MEMORY_TEMPERATURE                                  on\n");
    printf("        31      FREQUENCY_THROTTLE                                  on\n");
    printf("        32      PCIE_READ_THROUGHPUT                                off\n");
    printf("        33      PCIE_WRITE_THROUGHPUT                               off\n");
    printf("        34      PCIE_READ                                           off\n");
    printf("        35      PCIE_WRITE                                          off\n");
    printf("        36      ENGINE_UTILIZATION                                  on\n");
    printf("        37      FABRIC_THROUGHPUT                                   on\n");
    printf("        38      FREQUENCY_THROTTLE_REASON_GPU                       on\n");
    printf("\n");
}

unique_ptr<grpc::Server> buildAndStartRPCServer(const string& unixSockAddr, XpumCoreServiceImpl& service) {
    grpc::ServerBuilder builder;
    builder.AddListeningPort(unixSockAddr, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    return builder.BuildAndStart();
}

void runRPCServers() {
    XPUM_LOG_INFO("XPUM: start RPC server ...");

    string unixSockDir;
    if (sock_file_dir != nullptr ) {
        unixSockDir = sock_file_dir;
        if (unixSockDir.length() == 0 || unixSockDir.back() != '/') {
            unixSockDir += "/";
        }
        delete sock_file_dir;
        sock_file_dir = nullptr;
    } else {
        unixSockDir = defaultSockDir;
    }

    string privSock{unixSockDir + defaultPrivilegedSockName};
    string upriSock{unixSockDir + defaultUnprivilegedSockName};

    unlink(privSock.c_str());
    unlink(upriSock.c_str());

    umask(S_IXUSR | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH);

    //privileged socket
    XpumCoreServiceImpl privService;
    unique_ptr<grpc::Server> privServer =  buildAndStartRPCServer("unix://" + privSock, privService);
    XPUM_LOG_INFO("XPUM: RPC server is listening at {}", privSock);

    passwd* pwd = getpwnam("xpum");
    if (pwd != nullptr) {
        chown(privSock.c_str(), pwd->pw_uid, pwd->pw_gid);
    } else {
        XPUM_LOG_ERROR("XPUM: no xpum account exists, abort");
        return;
    }

    chmod(privSock.c_str(), S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);

    //non-privileged socket
    XpumCoreServiceUnprivilegedImpl upriService;
    unique_ptr<grpc::Server> upriServer = buildAndStartRPCServer("unix://" + upriSock, upriService);

    chown(upriSock.c_str(), pwd->pw_uid, pwd->pw_gid);
    chmod(upriSock.c_str(), S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH );

    // start a background thread for the server.
    std::thread grpc_server_thread(
        [](::grpc::Server* grpc_server_ptr) {
            grpc_server_ptr->Wait();
        },
        privServer.get());

    while (!stop) {
        // Wait for the stop signal and then shut the server.
        cv.wait(lock);
    }

    // Shut down server.
    XPUM_LOG_INFO("XPUM: Shutting down RPC server...");
    // must close service before shutdown the server to avoid stuck in server->Shutdown()
    privService.close();
    privServer->Shutdown();
    upriService.close();
    upriServer->Shutdown();
    XPUM_LOG_INFO("XPUM: Waiting for RPC server shutdown...");
    grpc_server_thread.join();
}

void writePID() {
    if (pid_file_name != nullptr) {
        char str[32];
        pidFilehandle = open(pid_file_name, O_RDWR | O_CREAT, 0600);
        if (pidFilehandle < 0) {
            XPUM_LOG_ERROR("XPUM: Could not open PID file {}.", pid_file_name);
            exit(EXIT_FAILURE);
        }
        if (lockf(pidFilehandle, F_TLOCK, 0) < 0) {
            XPUM_LOG_ERROR("XPUM: Could not lock PID file %{}", pid_file_name);
            exit(EXIT_FAILURE);
        }
        sprintf(str, "%d\n", getpid());

        write(pidFilehandle, str, strlen(str));
    }
}

void signalHandler(int sig) {
    switch (sig) {
        case SIGINT:
        case SIGTERM:
        case SIGKILL:
            stop = true;
            cv.notify_all();
            XPUM_LOG_WARN("XPUM: recieved SIGTERM signal {}, service shutdown.", sig);
            break;
        default:
            XPUM_LOG_INFO("XPUM: recieved unhandled signal {}.", sig);
            break;
    }
}

void createDumpFolder() {
    std::string dumpFolder;
    if (dump_folder_name != nullptr) {
        dumpFolder = dump_folder_name;
        if (dumpFolder.back() == '/') {
            dumpFolder.pop_back();
        }
        delete dump_folder_name;
        dump_folder_name = nullptr;
    } else {
        dumpFolder = "/tmp/xpumdump";
    }
    // create dump folder
    umask(0000);
    int res = mkdir(dumpFolder.c_str(), 0775);
    if (res != 0 && errno != EEXIST) {
        std::cerr << "Fail to create folder" << std::endl;
        abort();
    }

    // chwon of folder
    passwd* pwd = getpwnam("xpum");
    if (pwd != nullptr) {
        chown(dumpFolder.c_str(), pwd->pw_uid, pwd->pw_gid);
    }

    XpumCoreServiceImpl::dumpRawDataFileFolder = dumpFolder;
}

bool to_size_t(const char* number, size_t& size) {
    size_t tmp;
    std::istringstream iss(number);
    iss >> tmp;
    if (iss.fail()) {
        return false;
    } else {
        size = tmp;
        return true;
    }
}

bool to_log_level(const char* level, std::string& log_level) {
    std::string tmp = level;
    for (auto p = tmp.begin(); tmp.end() != p; ++p)
        *p = tolower(*p);
    if (tmp != "trace" && tmp != "debug" && tmp != "info" && tmp != "warn" && tmp != "error")
        return false;
    else
        log_level = tmp;
    return true;
}

void parse_opts(int argc, char* argv[]) {
    int lopt;
    static struct option long_options[] = {
        {"socket_folder", required_argument, 0, 's'},
        {"help", no_argument, 0, 'h'},
        {"pid_file", required_argument, 0, 'p'},
        {"dump_folder", required_argument, 0, 'd'},
        {"enable_metrics", required_argument, 0, 'm'},
        // log options:
        {"log_file", required_argument, 0, 'l'},
        {"log_max_size", required_argument, &lopt, 1},
        {"log_max_files", required_argument, &lopt, 2},
        {"log_level", required_argument, &lopt, 3},
        {NULL, 0, 0, 0}};
    int value, option_index = 0;
    while ((value = getopt_long(argc, argv, "s:p:d:l:m:h", long_options, &option_index)) != -1) {
        switch (value) {
            case 0: {
                bool valid = false;
                switch (lopt) {
                    case 1:
                        valid = to_size_t(optarg, log_max_size);
                        break;
                    case 2:
                        valid = to_size_t(optarg, log_max_files);
                        break;
                    case 3:
                        valid = to_log_level(optarg, log_level);
                        break;
                    default:
                        break;
                }
                if (!valid) {
                    print_help(argv[0]);
                    exit(EXIT_FAILURE);
                }
                break;
            }
            case 's':
                if (sock_file_dir == nullptr) {
                    sock_file_dir = strdup(optarg);
                }
                break;
            case 'p':
                if (pid_file_name == nullptr) {
                    pid_file_name = strdup(optarg);
                }
                break;
            case 'l':
                if (log_file_name == nullptr) {
                    log_file_name = strdup(optarg);
                }
                break;
            case 'd':
                if (dump_folder_name == nullptr) {
                    dump_folder_name = strdup(optarg);
                }
                break;
            case 'm':
                if (enabled_metrics == nullptr) {
                    enabled_metrics = strdup(optarg);
                }
                break;
            case 'h':
                print_help(argv[0]);
                exit(EXIT_SUCCESS);
            case '?':
                print_help(argv[0]);
                exit(EXIT_FAILURE);
            default:
                break;
        }
    }
}

} // end namespace xpum::daemon

using namespace xpum::daemon;

int main(int argc, char* argv[]) {
    parse_opts(argc, argv);
    umask(S_IXUSR | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH);
    Logger::init(log_level, log_file_name, log_max_size, log_max_files);
    if (log_file_name != nullptr) {
        delete log_file_name;
        log_file_name = nullptr;
    }

    writePID();
    createDumpFolder();
    signal(SIGINT, signalHandler);
    signal(SIGKILL, signalHandler);
    signal(SIGTERM, signalHandler);

    if (enabled_metrics != nullptr) {
        setenv("XPUM_METRICS", enabled_metrics, 1);
        delete enabled_metrics;
        enabled_metrics = nullptr;
    }

    XPUM_LOG_INFO("XPUM: Init xpum library");
    xpum::xpum_result_t res = xpum::xpumInit();
    if (res != xpum::XPUM_OK) {
        XPUM_LOG_ERROR("XPUM: Load xpum library failed! {}", res);
    }

    XPUM_LOG_INFO("XPUM: start XPUM RPC Server.");
    runRPCServers();

    XPUM_LOG_INFO("XPUM: Shut down.");
    res = xpum::xpumShutdown();
    XPUM_LOG_INFO("XPUM: xpum service is closed.");
    if (pidFilehandle != -1) {
        close(pidFilehandle);
        unlink(pid_file_name);
    }

    Logger::flush();

    return 0;
}
