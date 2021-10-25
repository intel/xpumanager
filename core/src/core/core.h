#pragma once

#include <string>

#include "infrastructure/init_close_interface.h"

#include "data_logic/data_logic_interface.h"
#include "control/device_manager_interface.h"
#include "monitor/monitor_manager_interface.h"
#include "health/health_manager_interface.h"
#include "group/group_manager_interface.h"
#include "diagnostic/diagnostic_manager_interface.h"
#include "policy/policy_manager_interface.h"

/*
  The top controller of xpum
*/

class Core : public InitCloseInterface {
  public:
    static Core &instance();

    void init() override;

    void close() override;

    bool isInitialized();

    std::shared_ptr<DeviceManagerInterface> getDeviceManager();

    std::shared_ptr<DataLogicInterface> getDataLogic();

    std::shared_ptr<MonitorManagerInterface> getMonitorManager();

    std::shared_ptr<HealthManagerInterface> getHealthManager();

    std::shared_ptr<GroupManagerInterface> getGroupManager();

    std::shared_ptr<DiagnosticManagerInterface> getDiagnosticManager();

    std::shared_ptr<PolicyManagerInterface> getPolicyManager();

  private:
    Core();

    Core &operator=(const Core &) = delete;

    Core(const Core &other) = delete;

    ~Core();

    void close(const std::shared_ptr<InitCloseInterface> &p_init_close_interface, const std::string &p_msgPrix);

  private:
    std::shared_ptr<DeviceManagerInterface> p_device_manager;

    std::shared_ptr<DataLogicInterface> p_data_logic;

    std::shared_ptr<MonitorManagerInterface> p_monitor_manager;

    std::shared_ptr<HealthManagerInterface> p_health_manager;

    std::shared_ptr<GroupManagerInterface> p_group_manager;

    std::shared_ptr<DiagnosticManagerInterface> p_diagnostic_manager;

    std::shared_ptr<PolicyManagerInterface> p_policy_manager;

    bool initialized;

    std::mutex mutex;
};
