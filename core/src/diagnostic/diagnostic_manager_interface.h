#pragma once

#include <vector>
#include <string>

#include "infrastructure/init_close_interface.h"
#include "diagnostic_data_type.h"
#include "../include/xpum_structs.h"

class DiagnosticManagerInterface : public InitCloseInterface {
 public:
  virtual ~DiagnosticManagerInterface() {}

  virtual xpum_result_t runDiagnostics(xpum_device_id_t deviceId, xpum_diag_level_t level) = 0;

  virtual xpum_result_t getDiagnosticsResult(xpum_device_id_t deviceId, xpum_diag_task_info_t *result) = 0;

};