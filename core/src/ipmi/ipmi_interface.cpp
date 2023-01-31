/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file ipmi_interface.cpp
 */

#ifdef __linux__
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#elif _WIN32
#if (_MSC_VER < 1910)
//
// EFI Headers go here
//
#include "EfiIpmiInterface.h"
#else
#include <limits.h>
#include <winfcntl.h>
#include <winipmi.h>
#endif
#endif

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "tool.h"

namespace xpum {

#define RESPONSE_TIMEOUT_SEC 5
#define MAX_RETRIES 5
#define RETRY_SLEEP_TIME_US 100
#define SLOT_IPMB_NETFN 0x3e
#define SLOT_IPMB_CMD 0x51

static int g_ipmi_dev = -1;

unsigned char gNetfn;
unsigned char gCmd;
uint8_t gSensorIndex;
uint8_t gUpdateType;
uint8_t gData[267];
unsigned short gSize;
uint8_t gReqData[300];
uint8_t gDeviceId;
uint8_t gOffsetLsb;
uint8_t gOffsetMsb;
uint8_t gReadCount;
uint8_t gRequestType;
uint16_t gEntryType;

static int ipmi_init();
static int ipmi_cmd(bsmc_req *req, bsmc_res *res);
static int ipmi_validate_res(bsmc_res res, uint16_t res_size);
static void ipmi_oem_req_init(bsmc_req *req, void *addr, uint8_t cmd);

bsmc_hal_t ipmi_hal = {
    .init = ipmi_init,
    .cmd = ipmi_cmd,
    .validate_res = ipmi_validate_res,
    .oem_req_init = ipmi_oem_req_init,
};

static void ipmi_cleanup() {
#ifdef __linux__
    close(g_ipmi_dev);
#elif _WIN32
#if (_MSC_VER < 1910)
    //
    // ipmi_close_efi(g_ipmi_dev);
    //
    EfiCleanup();
#else
    ipmi_close_win(g_ipmi_dev);
#endif
#endif
}

static int ipmi_init() {
    const char *IPMI_DEV0 = "/dev/ipmi0";

    if (g_ipmi_dev >= 0)
        return NRV_SUCCESS;

#ifdef __linux__
    g_ipmi_dev = open(IPMI_DEV0, O_RDWR);

    if (g_ipmi_dev < 0) {
        XPUM_LOG_ERROR("Unable to open {}. errno: {}({})\n",
                       IPMI_DEV0, errno, strerror(errno));
        return NRV_IPMI_ERROR;
    }

    if (atexit(ipmi_cleanup)) {
        XPUM_LOG_WARN("Cannot register function for process termination\n");
        return NRV_IPMI_ERROR;
    }
#elif (_MSC_VER < 1910)
    //
    // g_ipmi_dev = ipmi_open_efi();
    //

    EfiIpmiInit();

    if (g_ipmi_dev < 0) {
        log_verbose("IPMI_DEV0: %s", IPMI_DEV0);
    }
#elif (_WIN32) && (_MSC_VER >= 1910)
    g_ipmi_dev = ipmi_open_win();

    if (g_ipmi_dev < 0) {
        char errMsg[ERRNO_SIZE_MAX];
        log_error("Unable to open %s. errno: %d(%s)\n",
                  IPMI_DEV0, errno, strerror_s(errMsg, sizeof(errMsg), errno));
        return NRV_IPMI_ERROR;
    }
#endif
    return NRV_SUCCESS;
}

#ifdef __linux__
static bool timeout_expired(struct timespec *to) {
    struct timespec t;

    clock_gettime(CLOCK_MONOTONIC, &t);
    if (t.tv_sec > to->tv_sec)
        return true;
    else if ((to->tv_sec == t.tv_sec) && (t.tv_nsec >= to->tv_nsec))
        return true;
    else
        return false;
}

static int slot_ipmb_send_devid() {
    struct ipmi_req req;
    struct ipmi_system_interface_addr req_addr;
    int err;

    req_addr.addr_type = IPMI_SYSTEM_INTERFACE_ADDR_TYPE;
    req_addr.channel = IPMI_BMC_CHANNEL;
    req_addr.lun = 0;

    req.addr = (unsigned char *)&req_addr;
    req.addr_len = sizeof(req_addr);
    req.msg.netfn = IPMI_GET_DEVID_OEM_NETFN;
    req.msg.cmd = IPMI_FW_GET_INFO_CMD;
    req.msg.data_len = 0;

    XPUM_LOG_DEBUG("SlotIPMB Request (len: {}):", req.msg.data_len);

    err = ioctl(g_ipmi_dev, IPMICTL_SEND_COMMAND, &req);
    if (err) {
        XPUM_LOG_WARN(
            "Ioctl IPMICTL_SEND_COMMAND return error:{}, "
            "errno: {}({})\n",
            err, errno, strerror(errno));
        return NRV_IPMI_ERROR;
    }

    return 0;
}

static int slot_ipmb_send(unsigned char *request_buf, unsigned short request_len) {
    struct ipmi_req req;
    struct ipmi_system_interface_addr req_addr;
    int err;

    req_addr.addr_type = IPMI_SYSTEM_INTERFACE_ADDR_TYPE;
    req_addr.channel = IPMI_BMC_CHANNEL;
    req_addr.lun = 0;
    request_buf[3] = gNetfn;
    request_buf[4] = gCmd;
    if (gNetfn == IPMI_INTEL_OEM_NETFN && gCmd == IPMI_READ_SENSOR_CMD)
        request_buf[5] = gSensorIndex;
    if (gNetfn == IPMI_INTEL_OEM_NETFN && gCmd == IPMI_FW_UPDATE_START_CMD)
        request_buf[5] = gUpdateType;
    if (gNetfn == IPMI_INTEL_OEM_NETFN && gCmd == IPMI_TRANSFER_SIZE_DETECT) {
        for (int index = 0; index < 267; index++)
            request_buf[5 + index] = gData[index];
    }
    if (gNetfn == IPMI_INTEL_OEM_NETFN && gCmd == IPMI_FW_UPDATE_SEND_DATA_CMD) {
        for (int index = 0; index < gSize; index++)
            request_buf[5 + index] = gReqData[index];
    }
    if (gNetfn == IPMI_STORAGE_NETFN && gCmd == IPMI_FRU_GET_INFO)
        request_buf[5] = gDeviceId;
    if (gNetfn == IPMI_STORAGE_NETFN && gCmd == IPMI_FRU_READ_DATA) {
        request_buf[5] = gDeviceId;
        request_buf[6] = gOffsetLsb;
        request_buf[7] = gOffsetMsb;
        request_buf[8] = gReadCount;
    }
    if (gNetfn == IPMI_INTEL_OEM_NETFN && gCmd == IPMI_DEBUG_CMD) {
        request_buf[5] = gRequestType;
        if (gRequestType == DEBUG_INFO_PLOG_GET_ENTRY) {
            request_buf[6] = gEntryType;
            request_buf[7] = 0x00;
        }
    }
    req.addr = (unsigned char *)&req_addr;
    req.addr_len = sizeof(req_addr);
    req.msg.netfn = SLOT_IPMB_NETFN;
    req.msg.cmd = SLOT_IPMB_CMD;
    req.msg.data = request_buf;
    req.msg.data_len = request_len;

    err = ioctl(g_ipmi_dev, IPMICTL_SEND_COMMAND, &req);
    if (err) {
        XPUM_LOG_WARN(
            "Ioctl IPMICTL_SEND_COMMAND return error:{}, "
            "errno: {}({})\n",
            err, errno, strerror(errno));
        return NRV_IPMI_ERROR;
    }

    return 0;
}

static int slot_ipmb_recv(unsigned char *response_buf, unsigned short *response_len) {
    struct ipmi_recv res;
    struct ipmi_addr res_addr;
    struct timespec timeout;
    int err;

    res.addr = (unsigned char *)&res_addr;
    res.addr_len = sizeof(res_addr);
    res.msg.data = response_buf;
    res.msg.data_len = *response_len;
    memset(res.msg.data, 0, res.msg.data_len);

    clock_gettime(CLOCK_MONOTONIC, &timeout);
    timeout.tv_sec += RESPONSE_TIMEOUT_SEC;
retry:
    /*
     * IPMICTL_RECEIVE_MSG_TRUNC grab message from queue when response
     * length is too small. It helps to avoid plugging message queue.
     */
    err = ioctl(g_ipmi_dev, IPMICTL_RECEIVE_MSG_TRUNC, &res);
    if (err && errno == EAGAIN) {
        if (timeout_expired(&timeout))
            return NRV_IPMI_ERROR;
        usleep(1);
        goto retry;
    } else if (err) {
        XPUM_LOG_WARN(
            "Ioctl call IPMICTL_RECEIVE_MSG return error: {}, "
            "errno: {}({})\n",
            err, errno, strerror(errno));
        return NRV_IPMI_ERROR;
    }

    *response_len = res.msg.data_len;
    return err;
}
#endif

/*
 * ipmi_cmd is responsible for sending and receiving ipmi messages.
 *
 * If success than return NRV_SUCCESS
 * If ioctl error than return NRV_IPMI_ERROR
 */
static int ipmi_cmd(bsmc_req *req, bsmc_res *res) {
    unsigned short response_len;

#if !(__linux__) && (_MSC_VER < 1910)
    //
    // ipmi_cmd_efi();
    //
    response_len = sizeof(*res);

    if (EfiIpmiCmd(req, (UINT32)req->data_len + REQUEST_HEADER_SIZE, res, (UINT32 *)&response_len) != NRV_SUCCESS) {
        return NRV_IPMI_ERROR;
    }
    res->data_len = response_len;

#elif (_WIN32) && (_MSC_VER >= 1910)

    struct ipmi_req request_buf;
    unsigned char *req_buf;
    struct ipmi_ipmb_addr request_addr;
    struct ipmi_recv response_buf;
    struct ipmi_addr response_addr;

    request_addr.addr_type = IPMI_SYSTEM_INTERFACE_ADDR_TYPE;
    request_addr.channel = IPMI_BMC_CHANNEL;
    request_addr.slave_addr = IPMI_BMC_SLAVE_ADDR;
    request_addr.lun = 0;

    req_buf = (unsigned char *)req;
    req_buf[3] = gNetfn;
    req_buf[4] = gCmd;
    if (gNetfn == IPMI_STORAGE_NETFN && gCmd == IPMI_FRU_GET_INFO)
        req_buf[5] = gDeviceId;
    if (gNetfn == IPMI_STORAGE_NETFN && gCmd == IPMI_FRU_READ_DATA) {
        req_buf[5] = gDeviceId;
        req_buf[6] = gOffsetLsb;
        req_buf[7] = gOffsetMsb;
        req_buf[8] = gReadCount;
    }
    if (gNetfn == IPMI_INTEL_OEM_NETFN && gCmd == IPMI_READ_SENSOR_CMD)
        req_buf[5] = gSensorIndex;
    if (gNetfn == IPMI_INTEL_OEM_NETFN && gCmd == IPMI_DEBUG_CMD) {
        req_buf[5] = gRequestType;
        if (gRequestType == DEBUG_INFO_PLOG_GET_ENTRY) {
            req_buf[6] = gEntryType;
            req_buf[7] = 0x00;
        }
    }
    if (gNetfn == IPMI_INTEL_OEM_NETFN && gCmd == IPMI_FW_UPDATE_START_CMD)
        req_buf[5] = gUpdateType;
    if (gNetfn == IPMI_INTEL_OEM_NETFN && gCmd == IPMI_TRANSFER_SIZE_DETECT) {
        for (int index = 0; index <= 267; index++)
            req_buf[5 + index] = gData[index];
    }
    if (gNetfn == IPMI_INTEL_OEM_NETFN && gCmd == IPMI_FW_UPDATE_SEND_DATA_CMD) {
        for (int index = 0; index < gSize; index++)
            req_buf[5 + index] = gReqData[index];
    }
    request_buf.addr = (void *)&request_addr;
    request_buf.addr_len = sizeof(request_addr);
    request_buf.msg.netfn = SLOT_IPMB_NETFN;
    request_buf.msg.cmd = SLOT_IPMB_CMD;
    request_buf.msg.data = (unsigned char *)req;
    request_buf.msg.data_len = REQUEST_HEADER_SIZE + req->data_len;

    response_buf.addr = (unsigned char *)&response_addr;
    response_buf.addr_len = sizeof(response_addr);
    response_buf.msg.data = (unsigned char *)res;

    response_len = sizeof(*res);
    response_buf.msg.data_len = response_len;
    memset(response_buf.msg.data, 0, response_buf.msg.data_len);

    log_debug_array(request_buf.msg.data, request_buf.msg.data_len,
                    "SlotIPMB Request (len: %i):", request_buf.msg.data_len);

    if (ipmi_cmd_win(&request_buf, &request_addr, &response_buf)) {
        return NRV_IPMI_ERROR;
    }

    log_debug_array(response_buf.msg.data, response_buf.msg.data_len,
                    "SlotIPMB Response (len: %i):", response_buf.msg.data_len);
    res->data_len = response_buf.msg.data_len;
    if (gNetfn == IPMI_INTEL_OEM_NETFN && gCmd == IPMI_DEBUG_CMD && gRequestType == DEBUG_INFO_PLOG_GET_ENTRY) {
        gEntryType = response_buf.msg.data[13];
    }
#elif __linux__

    int retries = MAX_RETRIES;
    bool super_micro_wa_retry;

retry:
    super_micro_wa_retry = false;

    if (gNetfn == IPMI_GET_DEVID_OEM_NETFN) {
        if (slot_ipmb_send_devid())
            return NRV_IPMI_ERROR;
    } else {
        if (slot_ipmb_send((unsigned char *)req, REQUEST_HEADER_SIZE + req->data_len))
            return NRV_IPMI_ERROR;
    }

    response_len = sizeof(*res);
    if (slot_ipmb_recv((unsigned char *)res, &response_len))
        return NRV_IPMI_ERROR;

    if (response_len < RESPONSE_HEADER_SIZE) {
        XPUM_LOG_WARN("Invalid IPMI response header size\n");
        return NRV_IPMI_ERROR;
    }

    if (gNetfn != IPMI_GET_DEVID_OEM_NETFN) {
        if (res->slot_ipmb_completion_code == IPMB_CC_INVALID_PCIE_SLOT_NUM)
            return NRV_IPMI_ERROR;

        /*
         * SuperMicro BMC firmware returns invalid command code in SlotIPMB
         * response when there is a heavy trafic of IPMI messages. It is only
         * occured in firmware update process with very low reproduction ratio.
         */
        if (res->slot_ipmb_completion_code == IPMI_CC_INVALID_COMMAND)
            super_micro_wa_retry = true;

        if (res->slot_ipmb_completion_code == IPMB_CC_BUS_ERROR ||
            super_micro_wa_retry) {
            if (retries) {
                retries--;
                usleep(RETRY_SLEEP_TIME_US);
                goto retry;
            }
            return NRV_IPMI_ERROR;
        } else if (res->slot_ipmb_completion_code != IPMI_CC_SUCCESS) {
            return NRV_IPMI_ERROR;
        }

        res->data_len = response_len - RESPONSE_HEADER_SIZE;
    }
#endif
    return NRV_SUCCESS;
}

static int ipmi_validate_res(bsmc_res res, uint16_t res_size) {
    if (res.completion_code != IPMI_CC_SUCCESS) {
        XPUM_LOG_WARN("Non-zero completion code from BSMC: {}\n",
                      std::to_string(res.completion_code));
        return NRV_IPMI_ERROR;
    }

    if (res.data_len < res_size) {
        XPUM_LOG_WARN("Size of response is too small ({} < {})\n",
                      std::to_string(res.data_len), std::to_string(res_size));
        return NRV_IPMI_ERROR;
    }

    return NRV_SUCCESS;
}

static void ipmi_oem_req_init(bsmc_req *req, void *ipmi_address, uint8_t cmd) {
    req->ipmi_address = *(ipmi_address_t *)ipmi_address;
    req->netfn = IPMI_INTEL_OEM_NETFN;
    req->cmd = cmd;
    req->data_len = 0;
}
} // namespace xpum
