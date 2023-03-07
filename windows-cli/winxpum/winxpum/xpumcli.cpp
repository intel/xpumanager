/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file xpumcli.cpp
 */

// winxpum.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <vector>
#include <iomanip>
#include <string>
#include <CLI/App.hpp>
#include <nlohmann/json.hpp>
#include "cli_wrapper.h"
#include "comlet_discovery.h"
#include "cli_resource.h"
#include "comlet_config.h"
#include "comlet_statistics.h"
#include "comlet_dump.h"
#include "comlet_firmware.h"
#include "Windows.h"

#define MAKE_COMLET_PTR(comlet_type) (std::static_pointer_cast<ComletBase>(std::make_shared<comlet_type>()))

std::wstring ReadRegValue(HKEY root, std::wstring key, std::wstring name) {
    std::wstring ret;
    HKEY hKey;
    if (RegOpenKeyEx(root, key.c_str(), 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return ret;

    DWORD type;
    DWORD cbData;
    if (RegQueryValueEx(hKey, name.c_str(), NULL, &type, NULL, &cbData) != ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return ret;
    }

    std::wstring value(cbData / sizeof(wchar_t), L'\0');
    if (RegQueryValueEx(hKey, name.c_str(), NULL, NULL, reinterpret_cast<LPBYTE>(&value[0]), &cbData) != ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return ret;
    }

    RegCloseKey(hKey);

    size_t firstNull = value.find_first_of(L'\0');
    if (firstNull != std::string::npos)
        value.resize(firstNull);

    return value;
}

void initIGSCDllPath() {
    SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    std::wstring igsc_path = ReadRegValue(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\igfxnd", L"ImagePath");
    if (igsc_path.size() == 0)
        igsc_path = ReadRegValue(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\igfxn", L"ImagePath");
    if (igsc_path.find(L"igdkmdn") != std::string::npos)
        igsc_path = igsc_path.substr(0, igsc_path.find_last_of(L"\\"));
    TCHAR windir[MAX_PATH];
    GetWindowsDirectory(windir, MAX_PATH);
    igsc_path = std::wstring(windir) + L"\\" + igsc_path.substr(igsc_path.find(L"System32"));
    SetDllDirectory(igsc_path.c_str());
}

int main(int argc, char** argv)
{
    initIGSCDllPath();

    _putenv(const_cast<char*>("ZES_ENABLE_SYSMAN=1"));
    _putenv(const_cast<char*>("ZE_ENABLE_PCI_ID_DEVICE_ORDER=1"));

    CLI::App app{ getResourceString("CLI_APP_DESC") };

    CLIWrapper wrapper(app);
    wrapper.addComlet(MAKE_COMLET_PTR(ComletDiscovery)).
        addComlet(MAKE_COMLET_PTR(ComletFirmware)).
        addComlet(MAKE_COMLET_PTR(ComletConfig)).
        addComlet(MAKE_COMLET_PTR(ComletStatistics)).
        addComlet(MAKE_COMLET_PTR(ComletDump));
    app.require_subcommand(0, 1);

    if (argc == 1) {
        std::cout << app.help();
    }
    else {
        CLI11_PARSE(app, argc, argv);
    }

    wrapper.printResult(std::cout);
    return 0;
}