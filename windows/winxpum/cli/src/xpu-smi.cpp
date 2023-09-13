// cli.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file xpumcli.cpp
 */

#include "CLI/App.hpp"
#include "cli_resource.h"
#include "cli_wrapper.h"
#include "exit_code.h"

#include "comlet_discovery.h"
#include "comlet_version.h"
#include "comlet_firmware.h"
#include "comlet_statistics.h"
#include "comlet_config.h"
#include "comlet_dump.h"

#include "Windows.h"

#define MAKE_COMLET_PTR(comlet_type) (std::static_pointer_cast<xpum::cli::ComletBase>(std::make_shared<comlet_type>()))

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
    if (igsc_path.size() == 0) {
        return;
    }
    TCHAR windir[MAX_PATH];
    UINT ret = GetWindowsDirectory(windir, MAX_PATH);
    if (ret == 0) {
        return;
    }
    igsc_path = std::wstring(windir) + L"\\" + igsc_path.substr(igsc_path.find(L"System32"));
    SetDllDirectory(igsc_path.c_str());
}

int main(int argc, char** argv) {

    initIGSCDllPath();

    CLI::App app{ xpum::cli::getResourceString("CLI_APP_DESC") };
    app.name("xpu-smi");
    xpum::cli::CLIWrapper wrapper(app, true);

    wrapper
        .addComlet(MAKE_COMLET_PTR(xpum::cli::ComletDiscovery))
        .addComlet(MAKE_COMLET_PTR(xpum::cli::ComletFirmware))
        .addComlet(MAKE_COMLET_PTR(xpum::cli::ComletStatistics))
        .addComlet(MAKE_COMLET_PTR(xpum::cli::ComletDump))
        .addComlet(MAKE_COMLET_PTR(xpum::cli::ComletConfig))
        ;
    app.require_subcommand(0, 1);

    if (argc == 1) {
        std::cout << app.help();
        return XPUM_CLI_SUCCESS;
    }
    else {
        // CLI11_PARSE(app, argc, argv);
        try {
            app.parse(argc, argv);
        }
        catch (const CLI::ParseError& e) {
            auto err = app.exit(e);
            return err != 0 ? XPUM_CLI_ERROR_BAD_ARGUMENT : 0;
        }
        catch (...) {
            return XPUM_CLI_ERROR_GENERIC_ERROR;
        }
    }

    return wrapper.printResult(std::cout);
}
