/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file system_cmd.h
 */

#include <string>

namespace xpum {

class SystemCommandResult {
    std::string _output;
    int _exitStatus;

   public:
    SystemCommandResult(std::string& cmd_output, int cmd_exit_status) {
        _output = cmd_output;
        _exitStatus = cmd_exit_status;
    }

    const std::string& output() {
        return _output;
    }

    const int exitStatus() {
        return _exitStatus;
    }
};

SystemCommandResult execCommand(const std::string& command);
} // namespace xpum