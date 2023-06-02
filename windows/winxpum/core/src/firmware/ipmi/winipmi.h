/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file winipmi.h
 */

#ifdef _WIN32

#ifndef _WIN_IPMI_H_
#define _WIN_IPMI_H_

#pragma once
#include "pch.h"
#define _WIN32_DCOM
#include <combaseapi.h>
#include <oaidl.h>
#include <iostream>
#include <wbemcli.h>
#include "openipmi.h"
#pragma comment(lib, "wbemuuid.lib")
#include "..\ipmi\bsmc_interface.h"

static IWbemLocator *pLoc = NULL;
static IWbemServices *pSvc = NULL;
static IWbemClassObject *pClass = NULL;
static IEnumWbemClassObject *pEnum = NULL;
static IWbemClassObject *pObject = NULL;

static VARIANT varPath;
static char ipmiOpen = 0;

#ifndef MAX_VARIANT_LIST_SIZE
#define MAX_VARIANT_LIST_SIZE 9
#endif

void ipmi_close_win(int ipmiDevice);
int ipmi_open_win();
void ipmi_clean_win();
int ipmi_cmd_win(struct ipmi_req *req, struct ipmi_ipmb_addr *req_addr, struct ipmi_recv *res);
void handle_variant(VARIANT *var);
#endif /* _WIN_IPMI_H_ */

#endif /* _WIN32 */
