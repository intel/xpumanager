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
#include <iostream>

#include "sensor_reading.h"
#include "xpum_structs.h"

namespace xpum {

extern unsigned char gNetfn;
extern unsigned char gCmd;
extern uint8_t gSensorIndex;

int get_sdr_count(ipmi_address_t *ipmi_address, int &count) {
    bsmc_req req;
    bsmc_res res;
    bsmc_hal->oem_req_init(&req, ipmi_address, 0x20);
    req.data[0] = 1;
    req.data_len = 1;
    gNetfn = 0x4;
    gCmd = 0x20;
    if (bsmc_hal->cmd(&req, &res))
        return NRV_IPMI_ERROR;
    count = res.data[0];
    return NRV_SUCCESS;
}

int cmd_get_sensor_reading(ipmi_address_t *ipmi_address, uint8_t sensor_number, ipmi_buf *buf) {
    bsmc_req req;
    bsmc_res res;
    bsmc_hal->oem_req_init(&req, ipmi_address, 0x2d);
    req.data[0] = sensor_number;
    req.data_len = 1;
    gNetfn = 0x4;
    gCmd = 0x2d;
    // std::cout << "Sensor Reading:" << std::endl;
    // std::cout << "request data: " << std::endl;
    // for (int i = 0; i < req.data_len; i++) {
    //     std::cout << "0x" << std::hex << (int)req.data[i] << " ";
    // }
    // std::cout << std::endl;
    if (bsmc_hal->cmd(&req, &res))
        return NRV_IPMI_ERROR;
    // std::cout << "completion code: ";
    // std::cout << std::dec << (int)res.completion_code << std::endl;
    if (res.completion_code)
        return NRV_IPMI_ERROR;
    memcpy(buf->data, res.data, res.data_len - 1);
    buf->data_len = res.data_len - 1;
    // std::cout << std::dec << "response data_len: " << res.data_len << std::endl;
    // std::cout << "response data: " << std::endl;
    // for (int i = 0; i < res.data_len; i++) {
    //     std::cout << "0x" << std::hex << (int)res.data[i] << " ";
    // }
    // std::cout << std::endl;
    return NRV_SUCCESS;
}

int get_sdr(ipmi_address_t *ipmi_address,
            uint16_t &record_id,
            ipmi_buf *buf) {
    bsmc_req req;
    bsmc_res res;
    bsmc_hal->oem_req_init(&req, ipmi_address, 0x21); // 0x21 is get device sdr
    gNetfn = 0x4;
    gCmd = 0x21;

    req.data[0] = 0x0;  // reservation id LS Byte
    req.data[1] = 0x0;  // reservation id MS Byte

    memcpy(req.data + 2, &record_id, 2);

    req.data[4] = 0x0;  // offset into record
    req.data[5] = 0x5; // bytes to read
    req.data_len = 6;

    // get header
    if (bsmc_hal->cmd(&req, &res))
        return NRV_IPMI_ERROR;

    buf->ccode = res.completion_code;
    if (res.completion_code) {
        return NRV_IPMI_ERROR;
    }
    memcpy(&record_id, res.data, 2);

    int record_length_following = res.data[6];
    uint8_t *p_buf = buf->data;
    memcpy(p_buf, res.data, res.data_len-1);
    p_buf += res.data_len-1;
    buf->data_len += res.data_len-1;

    int bytes_left = record_length_following;
    int offset = 0x5;

    while (bytes_left) {
        bsmc_res res_;
        int bytes_to_read = bytes_left < 0x1d ? bytes_left : 0x1d;
        req.data[4] = offset;        // offset into record
        req.data[5] = bytes_to_read; // bytes to read

        if (bsmc_hal->cmd(&req, &res_))
            return NRV_IPMI_ERROR;
        int tmp = res_.data_len - 3;
        bytes_left -= tmp;
        offset += tmp;
        memcpy(p_buf, res_.data+2, tmp);
        p_buf += tmp;
        buf->data_len += tmp;
    }

    // std::cout << "sdr data: " << std::endl;
    // for (int i = 0; i < buf->data_len; i++) {
    //     std::cout << "0x" << std::hex << (int)buf->data[i] << " ";
    // }
    // std::cout << std::endl;
    return NRV_SUCCESS;
}

int get_sdr_list(nrv_card& card){
    ipmi_address_t *ipmi_address = &card.ipmi_address;
    bsmc_req req;
    bsmc_hal->oem_req_init(&req, ipmi_address, 0x21); // 0x21 is get device sdr
    
    card.sdr_list.clear();
    int sdr_count = 0;
    get_sdr_count(ipmi_address,sdr_count);
    // std::cout << "sdr count: " << sdr_count << std::endl;

    uint16_t record_id = 0x0;
    int sensor_sdr_count = 0;
    while (sdr_count--) {
        ipmi_buf sdr_buf;
        memset(&sdr_buf, 0, sizeof(ipmi_buf));
        // std::cout << "SDR record " <<record_id << std::endl;
        int err = get_sdr(ipmi_address, record_id, &sdr_buf);
        if (err)
            continue;
        
        // check data is correct
        struct sdr_get_rs *header = (struct sdr_get_rs *)sdr_buf.data;
        if (header->length + 7 != sdr_buf.data_len)
            continue;

        card.sdr_list.push_back(sdr_buf);
        sensor_sdr_count++;
    }

    return NRV_SUCCESS;
}

void get_sensor_reading(nrv_card& card, std::vector<xpum_sensor_reading_t>& reading_list){
    int sdr_count = card.sdr_list.size();
    ipmi_address_t *ipmi_address = &card.ipmi_address;

    for (int i = 0; i < sdr_count; i++) {
        ipmi_buf sdr_buf = card.sdr_list[i];
        ipmi_buf reading_buf;
        memset(&reading_buf, 0, sizeof(ipmi_buf));

        struct sdr_get_rs *header = (struct sdr_get_rs *)sdr_buf.data;

        struct sdr_record_common_sensor * record = (struct sdr_record_common_sensor *)(sdr_buf.data+7);

        int err = cmd_get_sensor_reading(ipmi_address, record->keys.sensor_num, &reading_buf);
        if (err)
            continue;

        xpum_sensor_reading_t sensor_reading_data;

        memset(&sensor_reading_data, 0, sizeof(sensor_reading_data));

        sensor_reading_data.amcIndex = card.id;

        auto sr = ipmi_sdr_read_sensor_value(record, header->type, 3, &reading_buf);

        if(!sr)
            continue;

        if (!sr->s_has_analog_value) 
            continue;

        // sensor name
        snprintf(sensor_reading_data.sensorName, XPUM_MAX_STR_LENGTH, "%s", sr->s_id);
        

        // sensor value
        sensor_reading_data.value = sr->s_a_val;

        if (header->type == SDR_RECORD_TYPE_FULL_SENSOR) {
            // sensor high
            double sensorHigh = sdr_convert_sensor_reading(sr->full, sr->full->normal_max);

            // snprintf(sensor_reading_data.sensorHigh, XPUM_MAX_STR_LENGTH, "%.*f",
            //             (sensorHigh == (int)sensorHigh) ? 0 : 3, sensorHigh);
            sensor_reading_data.sensorHigh = sensorHigh;
            
            // sensor low
            double sensorLow = sdr_convert_sensor_reading(sr->full, sr->full->normal_min);

            // snprintf(sensor_reading_data.sensorLow, XPUM_MAX_STR_LENGTH, "%.*f",
            //             (sensorLow == (int)sensorLow) ? 0 : 3, sensorLow);
            sensor_reading_data.sensorLow = sensorLow;
            
        }

        // sensor unit
        snprintf(sensor_reading_data.sensorUnit, XPUM_MAX_STR_LENGTH,"%s",sr->s_a_units);

        reading_list.push_back(sensor_reading_data);
    }
}


std::vector<xpum_sensor_reading_t> read_sensor() {
    std::vector<xpum_sensor_reading_t> res;
    // read_sensor_res sensor_read;
    // csv_buffer cbuf_header;
    // csv_buffer cbuf_data;
    int err = NRV_SUCCESS;
    int card_id = CARD_SELECT_ALL;
    nrv_list cards{};
    err = get_card_list(&cards, card_id);
    if (err)
        goto exit;

    for (int i = 0; i < cards.count; i++) {
        get_sensor_reading(cards.card[i], res);
    }

exit:
    // free(cbuf_header.buf);
    // free(cbuf_data.buf);
    return res;
}

} // namespace xpum