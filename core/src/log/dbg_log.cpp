/* 
 *  Copyright (C) 2022-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file dbg_log.cpp
 */

#include <sys/stat.h>
#include <sys/types.h>

#include <iostream>
#include <fstream>

#include "dbg_log.h"
#include "firmware/system_cmd.h"

#define ERR_UUID -1
#define ERR_TMP_DIR -2
#define ERR_COPY_FILES -3
#define ERR_GEN_OUT -5
#define ERR_TAR -6
#define ERR_RM_TMP -7

using namespace xpum;
using namespace std;

// The shell scritp fucntions string to 
// copy filds under under dir:
// /sys/class/drm/*/
// /sys/kernel/debug/dri/*/
// /sys/kernel/debug/dri/*/i915_params
// /sys/kernel/debug/dri/*/gt0
// /sys/kernel/debug/dri/*/gt1

const string SHELL_FUNCS = "#!/bin/sh\n\
copy_files_d1() {\n\
    for f1 in `ls $1 2> /dev/null;`; do\n\
        if [ ! -f \"$1/$f1\" ]; then\n\
            continue\n\
        fi\n\
        if [ ! -r \"$1/$f1\" ]; then\n\
            continue\n\
        fi\n\
#       echo \"cp $1/$f1 to $2\"\n\
        cp \"$1/$f1\" \"$2\"\n\
    done\n\
}\n\n\
\
copy_files_d2() {\n\
    for d1 in `ls $1 2> /dev/null;`; do\n\
        if [ -d \"$1/$d1\" ]; then\n\
            mkdir -p \"$2$1/$d1\"\n\
            copy_files_d1 \"$1/$d1\" \"$2$1/$d1\"\n\
        fi\n\
    done\n\
}\n\n\
copy_a_dir() { \n\
    for d1 in `ls $1 2> /dev/null;`; do\n\
        if [ -d \"$1/$d1\" ]; then \n\
            for d2 in `ls $1/$d1 2> /dev/null;`; do\n\
                if [ -d \"$1/$d1/$d2\" ] && [ \"$d2\" = \"$2\" ]; then\n\
                     mkdir -p \"$3$1/$d1/\"\n\
                     cp -r  \"$1/$d1/$d2\" \"$3$1/$d1/\"\n\
                fi\n\
            done\n\
        fi\n\
    done\n\
}\n\n\
copy_files_d2 /sys/class/drm $1\n\
copy_files_d2 /sys/kernel/debug/dri $1\n\
copy_a_dir /sys/kernel/debug/dri i915_params $1\n\
#copy_a_dir /sys/kernel/debug/dri gt0 $1\n\
#copy_a_dir /sys/kernel/debug/dri gt1 $1\n\
";

const string PACKS = "\'intel-915\\|intel-gsc\\|libmetee\\|level-zero\\|intel-level-zero-gpu\\|intel-gmmlib\\|intel-igc-core\\|intel-igc-opencl\\|intel-mediasdk-utils\\|ocl-icd\\|intel-mediasdk\\|libX11-xcb\\|libXfixes\\|libXxf86vm\\|libdrm\\|libglvnd\\|libglvnd-glx\\|libpciaccess\\|libva\\|libwayland-client\\|libxshmfence\\|mesa-filesystem\\|mesa-libGL\\|mesa-libglapi\\|intel-media-driver\\|libmfxgen1\\|libmfx1\\|libmfx-utils\\|libmfx-tools\\|intel-media-va-driver-non-free\'";

int getUUID(std::string &uuid) {
    SystemCommandResult scr = execCommand("cat /proc/sys/kernel/random/uuid");
    uuid = scr.output();
    return scr.exitStatus();
}

int createTmpDir(const string &uuid) {
    string dirName = "/var/tmp/xpum-" + uuid;
    return mkdir(dirName.c_str(), S_IRUSR | S_IWUSR);
}

int copyFiles(const string &uuid) {
    int ret = 0;
    string dirName = "/var/tmp/xpum-" + uuid + "/proc";
    ret = mkdir(dirName.c_str(), S_IRUSR | S_IWUSR);
    if (ret != 0) {
        return ret;
    }
    string cmd = "cp /proc/cpuinfo " + dirName;
    SystemCommandResult scr = execCommand(cmd.c_str()) ;
    cmd = "cp /proc/interrupts " + dirName;
    scr = execCommand(cmd.c_str()) ;
    cmd = "cp /proc/meminfo " + dirName;
    scr = execCommand(cmd.c_str()) ;
    cmd = "cp /proc/modules " + dirName;
    scr = execCommand(cmd.c_str()) ;
    cmd = "cp /proc/version " + dirName;
    scr = execCommand(cmd.c_str()) ;
    cmd = "cp /proc/pci " + dirName + " 2>&1";
    scr = execCommand(cmd.c_str()) ;
    cmd = "cp /proc/iomem " + dirName;
    scr = execCommand(cmd.c_str()) ;
    cmd = "cp /proc/mtrr " + dirName;
    scr = execCommand(cmd.c_str()) ;
    cmd = "cp /proc/cmdline " + dirName;
    scr = execCommand(cmd.c_str()) ;

    dirName = "/var/tmp/xpum-" + uuid;
    cmd = "cp /etc/os-release " + dirName;
    scr = execCommand(cmd.c_str());
    cmd = "cp /var/log/syslog " + dirName;
    scr = execCommand(cmd.c_str());
    cmd = "cp /var/log/kern*.log " + dirName;
    scr = execCommand(cmd.c_str());

    string shName = "/var/tmp/xpum-" + uuid + "/cp.sh" ; 
    ofstream os(shName);
    os << SHELL_FUNCS;
    os.close();
    cmd = "chmod u+x " + shName;
    scr = execCommand(cmd.c_str());
    cmd = shName + " /var/tmp/xpum-" + uuid + " 2>&1";
    scr = execCommand(cmd.c_str());
    cmd = "rm -f " + shName;
    scr = execCommand(cmd.c_str());

    return ret;
}

int genCmdOut(const string &uuid) {
    /*
    Orgnize the output to 4 files:
        driver-info (file names under /dev/dri, modinfo -n i915, uname -r)
        dmesg-output
        package-info
        system-info (lspci, dmidecode, lsusb, xpu-smi, clinfo, vainfo)
    */
    int ret = 0;

    string fileName = "/var/tmp/xpum-" + uuid + "/driver-info";
    ofstream os(fileName);
    string cmd = "modinfo -n i915";
    SystemCommandResult scr = execCommand(cmd.c_str()) ;
    os << cmd + "\n" + scr.output();
    cmd = "uname -r";
    scr = execCommand(cmd.c_str());
    os << "\n" + cmd + "\n" + scr.output();
    cmd = "ls /dev/dri";
    scr = execCommand(cmd.c_str());
    os << "\n" + cmd + "\n" + scr.output();
    os.close();

    fileName = "/var/tmp/xpum-" + uuid + "/dmesg-output";
    os.open(fileName);
    cmd = "dmesg";
    scr = execCommand(cmd.c_str()) ;
    os << cmd + "\n" + scr.output();
    os.close();

    fileName = "/var/tmp/xpum-" + uuid + "/package-info";
    os.open(fileName);
    cmd = "which rpm";
    scr = execCommand(cmd.c_str());
    if (scr.exitStatus() == 0) {
        cmd = "rpm -qa|grep ";
    } else {
        cmd = "apt list --installed 2>&1|grep ";
    }
    cmd += PACKS;
    scr = execCommand(cmd.c_str());
    os << cmd + "\n" + scr.output();
    os.close();

    fileName = "/var/tmp/xpum-" + uuid + "/system-info";
    os.open(fileName);
    cmd = "lspci -v -xxx";
    scr = execCommand(cmd.c_str());
    os << cmd + "\n" + scr.output();
    cmd = "dmidecode 2>&1";
    scr = execCommand(cmd.c_str());
    os << "\n" + cmd + "\n" + scr.output();
    cmd = "lsusb";
    scr = execCommand(cmd.c_str());
    os << "\n" + cmd + "\n" + scr.output();
    cmd = "xpu-smi discovery 2>&1";
    scr = execCommand(cmd.c_str());
    os << "\n" + cmd + "\n" + scr.output();
    cmd = "clinfo 2>&1";
    scr = execCommand(cmd.c_str());
    os << "\n" + cmd + "\n" + scr.output();
    cmd = "vainfo 2>&1";
    scr = execCommand(cmd.c_str());
    os << "\n" + cmd + "\n" + scr.output();
    os.close();

    return ret;
}

int tarBall(const string &uuid, const char *fileName) {
    string cmd = "tar -C /var/tmp/ -czf " + string(fileName) + " xpum-" + uuid;
    SystemCommandResult scr = execCommand(cmd.c_str()) ;
    return scr.exitStatus();
}

int removeTmp(const string &uuid) {
    string cmd = "rm -rf /var/tmp/xpum-" + uuid;
    SystemCommandResult scr = execCommand(cmd.c_str()) ;
    return scr.exitStatus();
}

int genDebugLog(const char *fileName) {
    int ret = 0;
    string uuid;
    if (getUUID(uuid) != 0 || uuid.empty() == true) {
        ret = ERR_UUID; 
        goto ERR;
    }
    uuid.pop_back();
    if (createTmpDir(uuid) != 0) {
        ret = ERR_TMP_DIR;
        goto ERR;
    }
    if (copyFiles(uuid) != 0) {
        ret = ERR_COPY_FILES;
        goto ERR;
    }
    if (genCmdOut(uuid) != 0) {
        ret = ERR_GEN_OUT;
        goto ERR;
    }
    if (tarBall(uuid, fileName) != 0) {
        ret = ERR_TAR;
        goto ERR;
    }
    if (removeTmp(uuid) != 0) {
        ret = ERR_RM_TMP;
        goto ERR;
    }

ERR:
    return ret;
}
