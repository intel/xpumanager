/*
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file win_native.cpp
 */

#include "pch.h"
#include <conio.h>
#include <dxgi.h>
#include <initguid.h>
#include <malloc.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <mutex>
#include "infrastructure/logger.h"


#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)     \
    {                       \
        if (p) {            \
            (p)->Release(); \
            (p) = nullptr;  \
        }                   \
    }
#endif

#include "win_native.h"

#pragma comment(lib, "pdh.lib")

static HQUERY lastQuery = NULL;

static std::chrono::time_point<std::chrono::system_clock> lastTimeStamp = std::chrono::system_clock::now();

bool queryOpened = false;

std::recursive_mutex queryMutex;

#define COPY_ENGINE_COUNTER_INDEX 0
#define MEDIA_ENGINE_COUNTER_INDEX 1
#define COMPUTE_ENGINE_COUNTER_INDEX 2
#define MEM_USED_COUNTER_INDEX 3
#define RENDER_ENGINE_COUNTER_INDEX 4
#define MAX_COUNTER_INDEX 5

static std::vector<HCOUNTER> lastCounterList[MAX_COUNTER_INDEX];

static double values[MAX_COUNTER_INDEX];

static std::vector<std::string> expandWildCardPath(LPCSTR WildCardPath) {
    PDH_STATUS Status = ERROR_SUCCESS;
    DWORD PathListLength = 0;
    DWORD PathListLengthBufLen;
    std::vector<std::string> pathList;
    Status = PdhExpandWildCardPathA(NULL, WildCardPath, NULL, &PathListLength, 0);
    if (Status != ERROR_SUCCESS && Status != PDH_MORE_DATA) {
        //wprintf(L"PdhExpandWildCardPathA failed with 0x%x.\n", Status);
        return pathList;
    }
    PathListLengthBufLen = PathListLength + 100;
    PZZSTR ExpandedPathList = (PZZSTR)malloc(PathListLengthBufLen);
    Status = PdhExpandWildCardPathA(NULL, WildCardPath, ExpandedPathList, &PathListLength, 0);
    if (Status != ERROR_SUCCESS) {
        //wprintf(L"PdhExpandWildCardPathA failed with 0x%x.\n", Status);
        return pathList;
    }
    for (int i = 0; i < PathListLength;) {
        std::string path(ExpandedPathList + i);
        if (path.length() > 0) {
            //std::cout << path << std::endl;
            pathList.push_back(path);
            i += path.length() + 1;
        } else {
            break;
        }
    }
    return pathList;
}

static std::vector<HCOUNTER> addCounter(HQUERY Query, std::vector<std::string> pathList) {
    PDH_STATUS Status;
    std::vector<HCOUNTER> CounterList;
    for (std::string path : pathList) {
        HCOUNTER Counter;
        Status = PdhAddCounterA(Query, path.c_str(), NULL, &Counter);
        if (Status != ERROR_SUCCESS) {
            //wprintf(L"PdhAddCounterA failed with 0x%x.\n", Status);
            return CounterList;
        }
        CounterList.push_back(Counter);
    }
    return CounterList;
}

void openPDHQuery() {
    std::lock_guard<std::recursive_mutex> lck(queryMutex);
    XPUM_LOG_DEBUG("enter initPDHQuery");
    if (queryOpened)
        return;
    lastTimeStamp = std::chrono::system_clock::now();
    PDH_STATUS Status = ERROR_SUCCESS;
    Status = PdhOpenQuery(NULL, NULL, &lastQuery);
    if (Status != ERROR_SUCCESS) {
        XPUM_LOG_DEBUG("PdhOpenQuery failed, return code: {}", Status);
        PdhCloseQuery(lastQuery);
        return;
    }
    lastCounterList[COPY_ENGINE_COUNTER_INDEX] = addCounter(lastQuery, expandWildCardPath("\\GPU Engine(*engtype_Copy)\\Utilization Percentage"));
    lastCounterList[MEDIA_ENGINE_COUNTER_INDEX] = addCounter(lastQuery, expandWildCardPath("\\GPU Engine(*engtype_VideoDecode)\\Utilization Percentage"));
    lastCounterList[COMPUTE_ENGINE_COUNTER_INDEX] = addCounter(lastQuery, expandWildCardPath("\\GPU Engine(*engtype_Compute)\\Utilization Percentage"));
    lastCounterList[RENDER_ENGINE_COUNTER_INDEX] = addCounter(lastQuery, expandWildCardPath("\\GPU Engine(*engtype_3D)\\Utilization Percentage"));
    lastCounterList[MEM_USED_COUNTER_INDEX] = addCounter(lastQuery, expandWildCardPath("\\GPU Adapter Memory(*)\\Dedicated Usage"));

    Status = PdhCollectQueryData(lastQuery);
    if (Status != ERROR_SUCCESS) {
        XPUM_LOG_DEBUG("PdhCollectQueryData failed, return code: {}", Status);
        PdhCloseQuery(lastQuery);
        return;
    }
    queryOpened = true;
    XPUM_LOG_DEBUG("leave initPDHQuery");
}

void updatePDHQuery() {
    std::lock_guard<std::recursive_mutex> lck(queryMutex);
    XPUM_LOG_DEBUG("enter updatePDHQuery");
    // check time duration
    auto ts = std::chrono::system_clock::now();
    if (ts > lastTimeStamp) {
        auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - lastTimeStamp);
        XPUM_LOG_DEBUG("time delta: {}", delta.count());
        if (delta.count() < 500) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500 - delta.count()));
        }
    } else {
        XPUM_LOG_DEBUG("lastTimeStamp is bigger than now");
    }
    PDH_STATUS Status = ERROR_SUCCESS;
    DWORD CounterType;
    PDH_FMT_COUNTERVALUE DisplayValue;
    if (lastQuery) {
        XPUM_LOG_DEBUG("lastQuery handler is valid");
        Status = PdhCollectQueryData(lastQuery);
        if (Status == ERROR_SUCCESS) {
            XPUM_LOG_DEBUG("PdhCollectQueryData success");
            for (int i = 0; i < MAX_COUNTER_INDEX; i++) {
                auto CounterList = lastCounterList[i];
                double value = 0;
                for (auto Counter : CounterList) {
                    Status = PdhGetFormattedCounterValue(Counter,
                                                         PDH_FMT_DOUBLE,
                                                         &CounterType,
                                                         &DisplayValue);
                    if (Status != ERROR_SUCCESS) {
                        XPUM_LOG_DEBUG("PdhGetFormattedCounterValue failed, return code: {}", Status);
                        continue;
                    }

                    value += DisplayValue.doubleValue;
                }
                if (i != MEM_USED_COUNTER_INDEX)
                    values[i] = value <= 100 ? value : 100;
                else
                    values[i] = value;
            }
        } else {
            XPUM_LOG_DEBUG("PdhCollectQueryData failed, return code: {}", Status);
        }
        Status = PdhCloseQuery(lastQuery);
        if (Status != ERROR_SUCCESS) {
            XPUM_LOG_DEBUG("PdhCloseQuery failed, return code: {}", Status);
        }
    } else {
        XPUM_LOG_DEBUG("lastQuery handler is NULL");
        for (int i = 0; i < MAX_COUNTER_INDEX; i++) {
            values[i] = 0;
        }
    }
    queryOpened = false;
    openPDHQuery();
    XPUM_LOG_DEBUG("leave updatePDHQuery");
}

void closePDHQuery() {
    std::lock_guard<std::recursive_mutex> lck(queryMutex);
    PdhCloseQuery(lastQuery);
    queryOpened = false;
}

double getCopyEngineUtilByNativeAPI() {
    std::lock_guard<std::recursive_mutex> lck(queryMutex);
    XPUM_LOG_DEBUG("enter getCopyEngineUtilByNativeAPI, value: {}", values[COPY_ENGINE_COUNTER_INDEX]);
    return values[COPY_ENGINE_COUNTER_INDEX];
}

double getComputeEngineUtilByNativeAPI() {
    std::lock_guard<std::recursive_mutex> lck(queryMutex);
    XPUM_LOG_DEBUG("enter getComputeEngineUtilByNativeAPI, value: {}", values[COMPUTE_ENGINE_COUNTER_INDEX]);
    return values[COMPUTE_ENGINE_COUNTER_INDEX];
}

double getMediaEngineUtilByNativeAPI() {
    std::lock_guard<std::recursive_mutex> lck(queryMutex);
    XPUM_LOG_DEBUG("enter getMediaEngineUtilByNativeAPI, value: {}", values[MEDIA_ENGINE_COUNTER_INDEX]);
    return values[MEDIA_ENGINE_COUNTER_INDEX];
}

double getMemUsedByNativeAPI() {
    std::lock_guard<std::recursive_mutex> lck(queryMutex);
    XPUM_LOG_DEBUG("enter getMemUsedByNativeAPI, value: {}", values[MEM_USED_COUNTER_INDEX]);
    return values[MEM_USED_COUNTER_INDEX];
}

double getRenderEngineUtilByNativeAPI() {
    std::lock_guard<std::recursive_mutex> lck(queryMutex);
    XPUM_LOG_DEBUG("enter getRenderEngineUtilByNativeAPI, value: {}", values[RENDER_ENGINE_COUNTER_INDEX]);
    return values[RENDER_ENGINE_COUNTER_INDEX];
}

double getMemSizeByNativeAPI() {
    XPUM_LOG_DEBUG("enter getMemSizeByNativeAPI");
    HINSTANCE hDXGI = LoadLibrary(L"dxgi.dll");
    if (!hDXGI)
        return 0;
    typedef HRESULT(WINAPI * LPCREATEDXGIFACTORY)(REFIID, void**);

    LPCREATEDXGIFACTORY pCreateDXGIFactory = nullptr;
    IDXGIFactory* pDXGIFactory = nullptr;

    // We prefer the use of DXGI 1.1
    pCreateDXGIFactory = (LPCREATEDXGIFACTORY)GetProcAddress(hDXGI, "CreateDXGIFactory1");

    if (!pCreateDXGIFactory) {
        pCreateDXGIFactory = (LPCREATEDXGIFACTORY)GetProcAddress(hDXGI, "CreateDXGIFactory");

        if (!pCreateDXGIFactory) {
            FreeLibrary(hDXGI);
            return 0;
        }
    }

    HRESULT hr = pCreateDXGIFactory(__uuidof(IDXGIFactory), (LPVOID*)&pDXGIFactory);

    if (SUCCEEDED(hr)) {
        if (pDXGIFactory == 0) {
            XPUM_LOG_DEBUG("pDXGIFactory == 0");
            return 0;
        }

        for (UINT index = 0;; ++index) {
            IDXGIAdapter* pAdapter = nullptr;
            HRESULT hr = pDXGIFactory->EnumAdapters(index, &pAdapter);
            if (FAILED(hr)) // DXGIERR_NOT_FOUND is expected when the end of the list is hit
                break;

            DXGI_ADAPTER_DESC desc;
            memset(&desc, 0, sizeof(DXGI_ADAPTER_DESC));
            if (SUCCEEDED(pAdapter->GetDesc(&desc))) {
                std::wstring ws(desc.Description);
                std::string name(ws.begin(), ws.end());
                XPUM_LOG_DEBUG("find adapter {}", name);
                if (name.find("Intel(R) Data Center GPU Flex Series") != name.npos || name.find("Intel(R) Iris(R) Xe Graphics") != name.npos) {
                    SAFE_RELEASE(pAdapter);
                    SAFE_RELEASE(pDXGIFactory);
                    FreeLibrary(hDXGI);
                    XPUM_LOG_DEBUG("name matched, and get mem size {}", desc.DedicatedVideoMemory);
                    return desc.DedicatedVideoMemory;
                }
            }

            SAFE_RELEASE(pAdapter);
        }

        SAFE_RELEASE(pDXGIFactory);
    }

    FreeLibrary(hDXGI);
    return 0;
}