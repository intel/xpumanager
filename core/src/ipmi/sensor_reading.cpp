/*
 *  Copyright (C) 2021-2022 Intel Corporation
 *
 *  Intel Simplified Software License (Version August 2021)
 *
 *  Use and Redistribution.  You may use and redistribute the software (the “Software”), without modification, provided the following conditions are met:
 *
 *  * Redistributions must reproduce the above copyright notice and the
 *    following terms of use in the Software and in the documentation and/or other materials provided with the distribution.
 *  * Neither the name of Intel nor the names of its suppliers may be used to
 *    endorse or promote products derived from this Software without specific
 *    prior written permission.
 *  * No reverse engineering, decompilation, or disassembly of this Software
 *    is permitted.
 *
 *  No other licenses.  Except as provided in the preceding section, Intel grants no licenses or other rights by implication, estoppel or otherwise to, patent, copyright, trademark, trade name, service mark or other intellectual property licenses or rights of Intel.
 *
 *  Third party software.  The Software may contain Third Party Software. “Third Party Software” is open source software, third party software, or other Intel software that may be identified in the Software itself or in the files (if any) listed in the “third-party-software.txt” or similarly named text file included with the Software. Third Party Software, even if included with the distribution of the Software, may be governed by separate license terms, including without limitation, open source software license terms, third party software license terms, and other Intel software license terms. Those separate license terms solely govern your use of the Third Party Software, and nothing in this license limits any rights under, or grants rights that supersede, the terms of the applicable license terms.
 *
 *  DISCLAIMER.  THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT ARE DISCLAIMED. THIS SOFTWARE IS NOT INTENDED FOR USE IN SYSTEMS OR APPLICATIONS WHERE FAILURE OF THE SOFTWARE MAY CAUSE PERSONAL INJURY OR DEATH AND YOU AGREE THAT YOU ARE FULLY RESPONSIBLE FOR ANY CLAIMS, COSTS, DAMAGES, EXPENSES, AND ATTORNEYS’ FEES ARISING OUT OF ANY SUCH USE, EVEN IF ANY CLAIM ALLEGES THAT INTEL WAS NEGLIGENT REGARDING THE DESIGN OR MANUFACTURE OF THE SOFTWARE.
 *
 *  LIMITATION OF LIABILITY. IN NO EVENT WILL INTEL BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  No support.  Intel may make changes to the Software, at any time without notice, and is not obligated to support, update or provide training for the Software.
 *
 *  Termination. Your right to use the Software is terminated in the event of your breach of this license.
 *
 *  Feedback.  Should you provide Intel with comments, modifications, corrections, enhancements or other input (“Feedback”) related to the Software, Intel will be free to use, disclose, reproduce, license or otherwise distribute or exploit the Feedback in its sole discretion without any obligations or restrictions of any kind, including without limitation, intellectual property rights or licensing obligations.
 *
 *  Compliance with laws.  You agree to comply with all relevant laws and regulations governing your use, transfer, import or export (or prohibition thereof) of the Software.
 *
 *  Governing law.  All disputes will be governed by the laws of the United States of America and the State of Delaware without reference to conflict of law principles and subject to the exclusive jurisdiction of the state or federal courts sitting in the State of Delaware, and each party agrees that it submits to the personal jurisdiction and venue of those courts and waives any objections. The United Nations Convention on Contracts for the International Sale of Goods (1980) is specifically excluded and will not apply to the Software.
 *
 *  @file sensor_reading.cpp
 */
 
#include "bsmc_interface.h"
// #include "bsmc_ipmi_oem_cmd.h"
#include "tool.h"
#include "xpum_structs.h"

namespace xpum {

extern unsigned char gNetfn;
extern unsigned char gCmd;
extern uint8_t gSensorIndex;

int get_sensor(ipmi_address_t *ipmi_address, uint8_t sensor_index, read_sensor_res *out_sensor) {
    bsmc_req req;
    bsmc_res res;

    bsmc_hal->oem_req_init(&req, ipmi_address, IPMI_READ_SENSOR_CMD);
    req.read_sensor.sensor_index = sensor_index;
    req.data_len = sizeof(read_sensor_req);

    gNetfn = IPMI_INTEL_OEM_NETFN;
    gCmd = IPMI_READ_SENSOR_CMD;
    gSensorIndex = sensor_index;

    if (bsmc_hal->cmd(&req, &res))
        return NRV_IPMI_ERROR;

#if !(__linux__) && (_MSC_VER < 1910)
    if (bsmc_hal->validate_res(res, SIZE_READ_SENSOR_RES))
#else
    if (bsmc_hal->validate_res(res, sizeof(read_sensor_res)))
#endif
        return NRV_IPMI_ERROR;

    *out_sensor = res.read_sensor;

    return NRV_SUCCESS;
}

const char *sensor_name_str(sensor_index_t sensor)
{
    switch (sensor) {
    /*case SENSOR_ADC_BIAS_1V8_VOLTAGE:     return "adc_bias_1v8_voltage";
    case SENSOR_ADC_BIAS_5V0_VOLTAGE:     return "adc_bias_5v0_voltage";

    case SENSOR_ADC_VDD_CORE_VOLTAGE:     return "adc_vdd_core_voltage";
    case SENSOR_VR_VDD_CORE_VOLTAGE:      return "vr_vdd_core_voltage";
    case SENSOR_VR_VDD_CORE_CURRENT:      return "vr_vdd_core_current";
    case SENSOR_VR_VDD_CORE_POWER:        return "vr_vdd_core_power";
    case SENSOR_VR_VDD_CORE_TEMP:         return "vr_vdd_core_temp";

    case SENSOR_ADC_VPP_HBM_VOLTAGE:      return "adc_vpp_hbm_voltage";

    case SENSOR_ADC_VDDQ1_HBM_VOLTAGE:    return "adc_vddq1_hbm_voltage";
    case SENSOR_VR_VDDQ1_HBM_VOLTAGE:     return "vr_vddq1_hbm_voltage";
    case SENSOR_VR_VDDQ1_HBM_POWER:       return "vr_vddq1_hbm_power";
    case SENSOR_VR_VDDQ1_HBM_CURRENT:     return "vr_vddq1_hbm_current";
    case SENSOR_VR_VDDQ_CONTROLLER_TEMP:  return "vr_vddq_controller_temp";

    case SENSOR_ADC_VDDQ2_HBM_VOLTAGE:    return "adc_vddq2_hbm_voltage";
    case SENSOR_VR_VDDQ2_HBM_VOLTAGE:     return "vr_vddq2_hbm_voltage";
    case SENSOR_VR_VDDQ2_HBM_POWER:       return "vr_vddq2_hbm_power";
    case SENSOR_VR_VDDQ2_HBM_CURRENT:     return "vr_vddq2_hbm_current";
    case SENSOR_VR_VDDQ_PWRSTAGE_TEMP:    return "vr_vddq_pwrstage_temp";

    case SENSOR_ADC_AVDD_PLL_VOLTAGE:     return "adc_avdd_analog_voltage";
    case SENSOR_VR_AVDD_ANALOG_VOLTAGE:   return "vr_avdd_analog_voltage";
    case SENSOR_VR_AVDD_ANALOG_POWER:     return "vr_avdd_analog_power";
    case SENSOR_VR_AVDD_ANALOG_CURRENT:   return "vr_avdd_analog_current";

    case SENSOR_ADC_VDDH_IO_VOLTAGE:      return "adc_vddh_io_voltage";
    case SENSOR_ADC_VDDA_PCIE_VOLTAGE:    return "adc_vdda_pcie_voltage";
    case SENSOR_ADC_VDDHA_ICL_VOLTAGE:    return "adc_vddha_icl_voltage";
    case SENSOR_VR_VDDA_ICL_VOLTAGE:      return "vr_vdda_icl_voltage";
    case SENSOR_VR_VDDA_ICL_POWER:        return "vr_vdda_icl_power";
    case SENSOR_VR_VDDA_ICL_CURRENT:      return "vr_vdda_icl_current";*/

    case SENSOR_INLET_TEMP:                           return "inlet_temp";
    case SENSOR_OUTLET_TEMP:                          return "outlet_temp";

    case SENSOR_MARGIN_TO_TCONTROL_TEMP:  return "margin_to_tcontrol_temp";
    case SENSOR_MARGIN_TO_TPROCHOT_TEMP:  return "margin_to_tprochot_temp";

    /*case SENSOR_ADC_CONN_12V0_VOLTAGE:    return "adc_conn_12v0_voltage";
    case SENSOR_PMON_CONN_12V0_VOLTAGE:   return "pmon_conn_12v0_voltage";
    case SENSOR_PMON_CONN_12V0_POWER:     return "pmon_conn_12v0_power";
    case SENSOR_PMON_CONN_12V0_CURRENT:   return "pmon_conn_12v0_current";

    case SENSOR_PMON_AUX1_12V0_VOLTAGE:   return "pmon_aux1_12v0_voltage";
    case SENSOR_PMON_AUX1_12V0_POWER:     return "pmon_aux1_12v0_power";
    case SENSOR_PMON_AUX1_12V0_CURRENT:   return "pmon_aux1_12v0_current";

    case SENSOR_PMON_AUX2_12V0_VOLTAGE:   return "pmon_aux2_12v0_voltage";
    case SENSOR_PMON_AUX2_12V0_POWER:     return "pmon_aux2_12v0_power";
    case SENSOR_PMON_AUX2_12V0_CURRENT:   return "pmon_aux2_12v0_current";

    case SENSOR_ADC_PCIE_12V0_VOLTAGE:    return "adc_pcie_12v0_voltage";
    case SENSOR_PMON_PCIE_12V0_VOLTAGE:   return "pmon_pcie_12v0_voltage";
    case SENSOR_PMON_PCIE_12V0_POWER:     return "pmon_pcie_12v0_power";
    case SENSOR_PMON_PCIE_12V0_CURRENT:   return "pmon_pcie_12v0_current";

    case SENSOR_ADC_PCIE_3V3_VOLTAGE:     return "adc_pcie_3v3_voltage";
    case SENSOR_PMON_PCIE_3V3_VOLTAGE:    return "pmon_pcie_3v3_voltage";
    case SENSOR_PMON_PCIE_3V3_POWER:      return "pmon_pcie_3v3_power";
    case SENSOR_PMON_PCIE_3V3_CURRENT:    return "pmon_pcie_3v3_current";

    case SENSOR_ADC_MEZZ_54V0_VOLTAGE:    return "adc_mezz_54v0_voltage";
    case SENSOR_PMON_MEZZ_54V0_VOLTAGE:   return "pmon_mezz_54v0_voltage";
    case SENSOR_PMON_MEZZ_54V0_POWER:     return "pmon_mezz_54v0_power";
    case SENSOR_PMON_MEZZ_54V0_CURRENT:   return "pmon_mezz_54v0_current";

    case SENSOR_PMON_TOTAL_POWER:         return "pmon_total_power";

    case SENSOR_ADC_NBM_12V0_VOLTAGE:     return "adc_nbm_12v0_voltage";

    case SENSOR_HBM0_TEMP:                return "hbm0_temp";
    case SENSOR_HBM1_TEMP:                return "hbm1_temp";
    case SENSOR_HBM2_TEMP:                return "hbm2_temp";
    case SENSOR_HBM3_TEMP:                return "hbm3_temp";

    case SENSOR_ASIC_TPC00_TEMP:          return "asic_tpc00_temp";
    case SENSOR_ASIC_TPC01_TEMP:          return "asic_tpc01_temp";
    case SENSOR_ASIC_TPC02_TEMP:          return "asic_tpc02_temp";
    case SENSOR_ASIC_TPC03_TEMP:          return "asic_tpc03_temp";
    case SENSOR_ASIC_TPC10_TEMP:          return "asic_tpc10_temp";
    case SENSOR_ASIC_TPC11_TEMP:          return "asic_tpc11_temp";
    case SENSOR_ASIC_TPC12_TEMP:          return "asic_tpc12_temp";
    case SENSOR_ASIC_TPC13_TEMP:          return "asic_tpc13_temp";
    case SENSOR_ASIC_TPC20_TEMP:          return "asic_tpc20_temp";
    case SENSOR_ASIC_TPC21_TEMP:          return "asic_tpc21_temp";
    case SENSOR_ASIC_TPC22_TEMP:          return "asic_tpc22_temp";
    case SENSOR_ASIC_TPC23_TEMP:          return "asic_tpc23_temp";
    case SENSOR_ASIC_TPC32_TEMP:          return "asic_tpc32_temp";
    case SENSOR_ASIC_TPC33_TEMP:          return "asic_tpc33_temp";
    case SENSOR_ASIC_TPC42_TEMP:          return "asic_tpc42_temp";
    case SENSOR_ASIC_TPC43_TEMP:          return "asic_tpc43_temp";
    case SENSOR_ASIC_ICG_SOUTH_TEMP:      return "asic_icg_south_temp";
    case SENSOR_ASIC_ICG_NORTH_TEMP:      return "asic_icg_north_temp";

    case SENSOR_ASIC_HBM0_PHY_TEMP:       return "asic_hbm0_phy_temp";
    case SENSOR_ASIC_HBM1_PHY_TEMP:       return "asic_hbm1_phy_temp";
    case SENSOR_ASIC_HBM2_PHY_TEMP:       return "asic_hbm2_phy_temp";
    case SENSOR_ASIC_HBM3_PHY_TEMP:       return "asic_hbm3_phy_temp";*/
    case SENSOR_PMBUS_MP2971_VOLTAGE:                  return "pmbus_mp2971_voltage";
    case SENSOR_PMBUS_MP2971_POWER:                    return "pmbus_mp2971_power";
    case SENSOR_PMBUS_MP2971_CURRENT:                  return "pmbus_mp2971_current";

    case SENSOR_PMBUS_MP2975_0_VCCIN_VOLTAGE:          return "pmbus_mp2975_0_vccin_voltage";
    case SENSOR_PMBUS_MP2975_0_VCCIN_POWER:            return "pmbus_mp2975_0_vccin_power";
    case SENSOR_PMBUS_MP2975_0_VCCIN_CURRENT:          return "pmbus_mp2975_0_vccin_current";

    case SENSOR_PMBUS_MP2975_0_VCCFA_EHV_VOLTAGE:      return "pmbus_mp2975_0_vccfa_ehv_voltage";
    case SENSOR_PMBUS_MP2975_0_VCCFA_EHV_POWER:        return "pmbus_mp2975_0_vccfa_ehv_power";
    case SENSOR_PMBUS_MP2975_0_VCCFA_EHV_CURRENT:      return "pmbus_mp2975_0_vccfa_ehv_current";

    case SENSOR_SOC_DIE_TEMP_0:                        return "soc_die_temp_0";
    case SENSOR_HBM0_TEMP_0:                           return "hbm0_temp_0";
    case SENSOR_HBM1_TEMP_0:                           return "hbm1_temp_0";

    case SENSOR_PMBUS_MP2975_1_VCCIN_VOLTAGE:          return "pmbus_mp2975_1_vccin_voltage";
    case SENSOR_PMBUS_MP2975_1_VCCIN_POWER:            return "pmbus_mp2975_1_vccin_power";
    case SENSOR_PMBUS_MP2975_1_VCCIN_CURRENT:          return "pmbus_mp2975_1_vccin_current";

    case SENSOR_PMBUS_MP2975_1_VCCFA_EHV_VOLTAGE:      return "pmbus_mp2975_1_vccfa_ehv_voltage";
    case SENSOR_PMBUS_MP2975_1_VCCFA_EHV_POWER:        return "pmbus_mp2975_1_vccfa_ehv_power";
    case SENSOR_PMBUS_MP2975_1_VCCFA_EHV_CURRENT:      return "pmbus_mp2975_1_vccfa_ehv_current";

    case SENSOR_SOC_DIE_TEMP_1:                        return "soc_die_temp_1";
    case SENSOR_HBM0_TEMP_1:                           return "hbm0_temp_1";
    case SENSOR_HBM1_TEMP_1:                           return "hbm1_temp_1";

/*    case SENSOR_PMBUS_MP2975_2_VCCIN_VOLTAGE:          return "pmbus_mp2975_2_vccin_voltage";
    case SENSOR_PMBUS_MP2975_2_VCCIN_POWER:            return "pmbus_mp2975_2_vccin_power";
    case SENSOR_PMBUS_MP2975_2_VCCIN_CURRENT:          return "pmbus_mp2975_2_vccin_current";

    case SENSOR_PMBUS_MP2975_2_VCCFA_EHV_VOLTAGE:      return "pmbus_mp2975_2_vccfa_ehv_voltage";
    case SENSOR_PMBUS_MP2975_2_VCCFA_EHV_POWER:        return "pmbus_mp2975_2_vccfa_ehv_power";
    case SENSOR_PMBUS_MP2975_2_VCCFA_EHV_CURRENT:      return "pmbus_mp2975_2_vccfa_ehv_current";

    case SENSOR_SOC_DIE_TEMP_2:                        return "soc_die_temp_2";
    case SENSOR_HBM0_TEMP_2:                           return "hbm0_temp_2";
    case SENSOR_HBM1_TEMP_2:                           return "hbm1_temp_2";

    case SENSOR_PMBUS_MP2975_3_VCCIN_VOLTAGE:          return "pmbus_mp2975_3_vccin_voltage";
    case SENSOR_PMBUS_MP2975_3_VCCIN_POWER:            return "pmbus_mp2975_3_vccin_power";
    case SENSOR_PMBUS_MP2975_3_VCCIN_CURRENT:          return "pmbus_mp2975_3_vccin_current";

    case SENSOR_PMBUS_MP2975_3_VCCFA_EHV_VOLTAGE:      return "pmbus_mp2975_3_vccfa_ehv_voltage";
    case SENSOR_PMBUS_MP2975_3_VCCFA_EHV_POWER:        return "pmbus_mp2975_3_vccfa_ehv_power";
    case SENSOR_PMBUS_MP2975_3_VCCFA_EHV_CURRENT:      return "pmbus_mp2975_3_vccfa_ehv_current";

    case SENSOR_SOC_DIE_TEMP_3:                        return "soc_die_temp_3";
    case SENSOR_HBM0_TEMP_3:                           return "hbm0_temp_3";
    case SENSOR_HBM1_TEMP_3:                           return "hbm1_temp_3";*/


    case SENSOR_COUNT:                                 return "Unknown";
    }

    return "Unknown";
}

const char *sensor_low_str(sensor_index_t sensor)
{
  switch (sensor)
  {
    case SENSOR_INLET_TEMP:                            return "0";
    case SENSOR_OUTLET_TEMP:                           return "0";

    case SENSOR_MARGIN_TO_TCONTROL_TEMP:               return "0";
    case SENSOR_MARGIN_TO_TPROCHOT_TEMP:               return "0";

    case SENSOR_PMBUS_MP2971_VOLTAGE:                  return "0";
    case SENSOR_PMBUS_MP2971_POWER:                    return "0";
    case SENSOR_PMBUS_MP2971_CURRENT:                  return "0";


    case SENSOR_PMBUS_MP2975_0_VCCIN_VOLTAGE:          return "0";
    case SENSOR_PMBUS_MP2975_0_VCCIN_POWER:            return "0";
    case SENSOR_PMBUS_MP2975_0_VCCIN_CURRENT:          return "0";


    case SENSOR_PMBUS_MP2975_0_VCCFA_EHV_VOLTAGE:      return "0";
    case SENSOR_PMBUS_MP2975_0_VCCFA_EHV_POWER:        return "0";
    case SENSOR_PMBUS_MP2975_0_VCCFA_EHV_CURRENT:      return "0";


    case SENSOR_SOC_DIE_TEMP_0:                        return "0";
    case SENSOR_HBM0_TEMP_0:                           return "0";
    case SENSOR_HBM1_TEMP_0:                           return "0";


    case SENSOR_PMBUS_MP2975_1_VCCIN_VOLTAGE:          return "0";
    case SENSOR_PMBUS_MP2975_1_VCCIN_POWER:            return "0";
    case SENSOR_PMBUS_MP2975_1_VCCIN_CURRENT:          return "0";


    case SENSOR_PMBUS_MP2975_1_VCCFA_EHV_VOLTAGE:      return "0";
    case SENSOR_PMBUS_MP2975_1_VCCFA_EHV_POWER:        return "0";
    case SENSOR_PMBUS_MP2975_1_VCCFA_EHV_CURRENT:      return "0";


    case SENSOR_SOC_DIE_TEMP_1:                        return "0";
    case SENSOR_HBM0_TEMP_1:                           return "0";
    case SENSOR_HBM1_TEMP_1:                           return "0";

    case SENSOR_COUNT:                                 return "Unknown";
  }
  return "Unknown";
}

const char *sensor_high_str(sensor_index_t sensor)
{
  switch (sensor)
  {
    case SENSOR_INLET_TEMP:                            return "65";
    case SENSOR_OUTLET_TEMP:                           return "75";

    case SENSOR_MARGIN_TO_TCONTROL_TEMP:               return "75";
    case SENSOR_MARGIN_TO_TPROCHOT_TEMP:               return "75";

    case SENSOR_PMBUS_MP2971_VOLTAGE:                  return "1050";
    case SENSOR_PMBUS_MP2971_POWER:                    return "1T: 12, 2T: 21, 4T: 44";
    case SENSOR_PMBUS_MP2971_CURRENT:                  return "1T: 11, 2T: 20, 4T: 42";


    case SENSOR_PMBUS_MP2975_0_VCCIN_VOLTAGE:          return "1890";
    case SENSOR_PMBUS_MP2975_0_VCCIN_POWER:            return "1T: 170, 2T: 331, 4T: 888";
    case SENSOR_PMBUS_MP2975_0_VCCIN_CURRENT:          return "1T: 90, 2T: 175, 4T: 470";


    case SENSOR_PMBUS_MP2975_0_VCCFA_EHV_VOLTAGE:      return "1890";
    case SENSOR_PMBUS_MP2975_0_VCCFA_EHV_POWER:        return "1T: 42, 2T: 81, 4T: 163";
    case SENSOR_PMBUS_MP2975_0_VCCFA_EHV_CURRENT:      return "1T: 22, 2T: 43, 4T: 86";

    case SENSOR_SOC_DIE_TEMP_0:                        return "65";
    case SENSOR_HBM0_TEMP_0:                           return "75";
    case SENSOR_HBM1_TEMP_0:                           return "75";


    case SENSOR_PMBUS_MP2975_1_VCCIN_VOLTAGE:          return "1890";
    case SENSOR_PMBUS_MP2975_1_VCCIN_POWER:            return "2T: 331, 4T: 888";
    case SENSOR_PMBUS_MP2975_1_VCCIN_CURRENT:          return "2T: 175, 4T: 470";


    case SENSOR_PMBUS_MP2975_1_VCCFA_EHV_VOLTAGE:      return "1890";
    case SENSOR_PMBUS_MP2975_1_VCCFA_EHV_POWER:        return "2T: 81, 4T: 163";
    case SENSOR_PMBUS_MP2975_1_VCCFA_EHV_CURRENT:      return "2T: 43, 4T: 86";


    case SENSOR_SOC_DIE_TEMP_1:                        return "65";
    case SENSOR_HBM0_TEMP_1:                           return "75";
    case SENSOR_HBM1_TEMP_1:                           return "75";

    case SENSOR_COUNT:                                 return "Unknown";
  }
  return "Unknown";
}

const char *sensor_unit_str(unit_t unit)
{
    switch(unit) {
    case UNIT_MV:
        return "mV";
    case UNIT_MA:
        return "mA";
    case UNIT_MW:
        return "mW";
    case UNIT_A:
        return "A";
    case UNIT_W:
        return "W";
    case UNIT_C:
        return "C";
    case UNIT_COUNT:
        return "N/A";
    }
    return "N/A";
}

std::vector<xpum_sensor_reading_t> read_sensor() {
    std::vector<xpum_sensor_reading_t> res;
    read_sensor_res sensor_read;
    // csv_buffer cbuf_header;
    // csv_buffer cbuf_data;
    int err = NRV_SUCCESS;
    int card_id = CARD_SELECT_ALL;
    nrv_list cards;
    err = get_card_list(&cards, card_id);
    if (err)
        goto exit;

    for (int i = 0; i < cards.count; i++) {
        for (int id = 0; id < SENSOR_COUNT; id++) {
            /* initialize sensor list */
            if (!cards.card[i].sensors_initialized) {
                cards.card[i].sensor_filtered[id] = false;
                if (get_sensor(&cards.card[i].ipmi_address, id, &sensor_read)) {
                    cards.card[i].sensor_filtered[id] = true;
                    continue;
                }
            } else {
                if (cards.card[i].sensor_filtered[id])
                    continue;

                if (get_sensor(&cards.card[i].ipmi_address, id, &sensor_read)) {
                    XPUM_LOG_ERROR("Sensor id(%i): read error\n", id);
                    continue;
                }
            }

            if (sensor_read.reading < 0) {
                XPUM_LOG_ERROR("Fail to read sensor {}", sensor_name_str((sensor_index_t)id));
                continue;
            }
            int value = sensor_read.reading;
            std::string sensorName = sensor_name_str((sensor_index_t)id);
            std::string sensorLow = sensor_low_str((sensor_index_t)id);
            std::string sensorHigh = sensor_high_str((sensor_index_t)id);
            std::string sensorUnit = sensor_unit_str((unit_t)sensor_read.unit);
            xpum_sensor_reading_t data;
            data.deviceIndex = i;
            data.value = value;

            sensorName.copy(data.sensorName, sensorName.length());
            data.sensorName[sensorName.length()] = '\0';

            sensorLow.copy(data.sensorLow, sensorLow.length());
            data.sensorLow[sensorLow.length()] = '\0';

            sensorHigh.copy(data.sensorHigh, sensorHigh.length());
            data.sensorHigh[sensorHigh.length()] = '\0';

            sensorUnit.copy(data.sensorUnit, sensorUnit.length());
            data.sensorUnit[sensorUnit.length()] = '\0';
            res.push_back(data);
        }
    }

exit:
    // free(cbuf_header.buf);
    // free(cbuf_data.buf);
    return res;
}

} // namespace xpum