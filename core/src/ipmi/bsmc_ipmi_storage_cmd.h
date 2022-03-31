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
 *  @file bsmc_ipmi_storage_cmd.h
 */

#pragma once

#include <stdint.h>

#include "compiler_specific.h"

namespace xpum {

/* NetFn used for storage commands */
#define IPMI_STORAGE_NETFN 0x0a

/******************************************************************************
 * 				FRU section
 *****************************************************************************/
/* FRU command identifiers */
#define IPMI_FRU_GET_INFO 0x10
#define IPMI_FRU_READ_DATA 0x11
#define IPMI_FRU_WRITE_DATA 0x12

/* Structures for communication using FRU commands */
/* IPMI_FRU_GET_INFO request */

#if !(_MSC_VER < 1910)
#pragma pack(1)
#endif
typedef struct
{
    uint8_t device_id;
} fru_get_area_info_req_t;

/* IPMI_FRU_GET_INFO response */
typedef struct
{
    uint8_t completion_code;
    uint8_t fru_size_lsb;
    uint8_t fru_size_msb;
    uint8_t access_type;
} fru_get_area_info_resp_t;

/* IPMI_FRU_READ_DATA request */
typedef struct
{
    uint8_t device_id;
    uint8_t offset_lsb;
    uint8_t offset_msb;
    uint8_t read_count;
} fru_read_data_req_t;

/* IPMI_FRU_READ_DATA response */
typedef struct
{
    uint8_t completion_code;
    uint8_t bytes_read;
    uint8_t read_data[1];
} fru_read_data_resp_t;

/* IPMI_FRU_WRITE_DATA response */
typedef struct
{
    uint8_t completion_code;
    uint8_t bytes_written;
} fru_write_data_resp_t;

/* IPMI FRU Area header */
typedef struct
{
    uint8_t product_area_format_version; // Product area format version
    uint8_t product_area_length;         // Product area length
    uint8_t language_code;               // Language code
} fru_product_area_header_t;

/* FRU Product Area element positions */
#define FRU_PRODUCT_PART_NUMBER_POS 2
#define FRU_PRODUCT_SERIAL_NUMBER_POS 12

/* Additional member in Board Area Header */
#define FRU_BOARD_MFG_DATE_TIME_LENGTH 8
/* IPMI FRU Area header */
typedef struct
{
    uint8_t area_format_version;                           // Board area format version
    uint8_t area_length;                                   // Board area length
    uint8_t language_code;                                 // Language code
    uint8_t mfg_date_time[FRU_BOARD_MFG_DATE_TIME_LENGTH]; // Mfg Date/Time stamp - minutes from 1996
} fru_board_area_header_t;

/* FRU Board Area element positions */
#define FRU_BOARD_MANUFACTURER_POS 0
#define FRU_BOARD_PRODUCT_NAME_POS 1
#define FRU_BOARD_SERIAL_NUMBER_POS 2
#define FRU_BOARD_PART_NUMBER_POS 3

#define FRU_BOARD_CARD_TYPE_POS 51
#define FRU_BOARD_TILE_INFO_POS 52
#define FRU_BOARD_PLATFORM_TYPE_POS 53
#define FRU_BOARD_FAB_ID_POS 54

#define FRU_BOARD_PRODUCT_NUMBER_POS 55
#define FRU_BOARD_HARDWARE_REVISION_POS 71
#define FRU_BOARD_ODM_POS 72
#define FRU_BOARD_CARD_TDP_POS 73

#define FRU_BOARD_UUID_POS 77

#define FRU_BOARD_CRC_POS 110
#define FRU_BOARD_AMC_SLAVE_ADDR_POS 111
#define FRU_BOARD_FRU_FILE_ID_POS 112
#define FRU_BOARD_SRIS_ENABLE_POS 113
#define FRU_BOARD_GPIO_EXPANDER_POS 114
#define FRU_BOARD_REWORK_TRACKER_POS 115

/*FRU Board Elements Length*/
#define FRU_BOARD_PRODUCT_NUMBER_LEN 15
#define FRU_BOARD_ODM_LEN 0x01
#define FRU_BOARD_UUID_LEN 32
#define FRU_BOARD_REWORK_TRACKER_LEN 0x04

/* IPMI FRU Common Header with offset 0 in FRU */
typedef struct
{
    uint8_t common_header_format_version;      // Common header format version
    uint8_t internal_use_area_offset;          // Internal use area offset
    uint8_t chassis_info_area_offset;          // Chassis area info offset
    uint8_t board_area_starting_offset;        // Board area starting offset
    uint8_t product_info_area_starting_offset; // Product info area starting offset
    uint8_t multirecord_area_starting_offset;  // Multirecord area starting offset
    uint8_t padding;                           // Pad character (always 0x00)
    uint8_t zero_checksum;                     // Zero checksum value (to make the whole structure checksum to 0x00)
} fru_header_t;

/* FRU offset and size of area needs to be multiply by 8 to get it value */
#define FRU_GET_AREA_OFFSET(offset) (offset * 8)
#define FRU_GET_AREA_SIZE(size) (size * 8)

/* FRU can store in one entry 63 bytes (!0xc0 == 63) and 1 byte for zero sign */
#define FRU_MAX_STRING_SIZE 64

/* FRU supported format */
#define HEADER_FORMAT_VERSION 0x01 /* Supported header format */

/* FRU entry Type/Length mask */
#define TYPE_TYPE_MASK 0xc0                  /* Type mask */
#define TYPE_LENGTH_MASK ((uint8_t) ~(0xc0)) /* Length mask */
#define FRU_END_FIELD 0xc1                   /* Last record in area */

/* FRU entry type */
#define TYPE_BINARY_UNSPECIFIED 0x00 /* Binary or unspecified */
#define TYPE_BCD_PLUS 0x40           /* BCD Plus */
#define TYPE_6_BIT_ASCII 0x80        /* 6 Bit ASCII packed */
#define TYPE_8_BIT_ASCII 0xc0        /* 8 Bit ASCII */

/* Supported language */
#define LANGUAGE_ENGLISH 0x00 /* Language field */

/******************************************************************************
 * 				SEL section
 *****************************************************************************/
/* SEL command identifiers */
#define IPMI_SEL_GET_INFO 0x40
#define IPMI_SEL_GET_ENTRY 0x43
#define IPMI_SEL_CLEAR 0x47
#define IPMI_SEL_GET_TIME 0x48
#define IPMI_SEL_SET_TIME 0x49

/* IPMI_SEL_GET_INFO response */
typedef struct ALIGNAS(16) {
    uint8_t completion_code;
    uint8_t sel_version;
    uint8_t lsb_entries;
    uint8_t msb_entries;
    uint8_t lsb_free_space;
    uint8_t msb_free_space;
    uint8_t b0_last_addition_timestamp;
    uint8_t b1_last_addition_timestamp;
    uint8_t b2_last_addition_timestamp;
    uint8_t b3_last_addition_timestamp;
    uint8_t b0_last_deletion_timestamp;
    uint8_t b1_last_deletion_timestamp;
    uint8_t b2_last_deletion_timestamp;
    uint8_t b3_last_deletion_timestamp;
    union ALIGNAS(16) {
        uint8_t all;
        struct ALIGNAS(16) {
            uint8_t bCmdSupportGetSELAlloc : 1;
            uint8_t bCmdSupportReserveSEL : 1;
            uint8_t bCmdSupportPartialAdd : 1;
            uint8_t bCmdSupportDeleteSEL : 1;
            uint8_t bReserved000 : 3;
            uint8_t bSELOverflow : 1;
        } bits;
    } operation_support;
} sel_get_info_resp_t;

/* IPMI_SEL_GET_ENTRY request */
typedef struct ALIGNAS(16) {
    uint8_t lsb_reservation_id;
    uint8_t msb_reservation_id;
    uint8_t lsb_record_id;
    uint8_t msb_record_id;
    uint8_t record_offset;
    uint8_t bytes_to_read;
} sel_get_entry_req_t;

/* Used in SSELGetEntryReq to read entire record (16 bytes) */
#define SEL_READ_ENTIRE_RECORD 0xff
#define SEL_RECORD_SIZE 16

/* IPMI_SEL_GET_ENTRY response */
typedef struct ALIGNAS(16) {
    uint8_t completion_code;
    uint8_t lsb_next_record_id;
    uint8_t msb_next_record_id;
    uint8_t record_data[SEL_RECORD_SIZE];
} sel_get_entry_resp_t;

/* Completion code for get info and get entry commands */
#define SEL_ERASE_IS_IN_PROGRESS_COMPCODE 0x81

/* SEL record structure */
typedef struct ALIGNAS(16) {
    uint8_t b0_record_id;
    uint8_t b1_record_id;
    uint8_t record_type;
    uint8_t b0_timestamp;
    uint8_t b1_timestamp;
    uint8_t b2_timestamp;
    uint8_t b3_timestamp;
    uint8_t b0_generator_id;
    uint8_t b1_generator_id;
    uint8_t event_message_format;
    uint8_t sensor_type;
    uint8_t sensor_number;
    uint8_t event_dir_type;
    uint8_t event_data1;
    uint8_t event_data2;
    uint8_t event_data3;
} sel_event_record_t;

/* InitiateErase member of SSELClearReq can be set to one of following values */
#define SEL_CLEAR_INITIATE_ERASE 0xaa
#define SEL_CLEAR_GET_STATUS 0x0

/* IPMI_SEL_CLEAR request */
typedef struct ALIGNAS(16) {
    uint8_t lsb_reservation_id;
    uint8_t msb_reservation_id;
    uint8_t c;
    uint8_t l;
    uint8_t r;
    uint8_t command;
} sel_clear_req_t;

/* EraseProgress member of SSELClearResp set lsb to singal status of erase */
#define SEL_CLEAR_ERASURE_IN_PROGRESS 0
#define SEL_CLEAR_ERASE_COMPLETED 1

/* IPMI_SEL_CLEAR response */
#if __linux__
typedef struct __attribute__((packed))
#else
typedef struct
#endif
{
    uint8_t completion_code;
    uint8_t erase_progress;
} sel_clear_resp_t;

/* IPMI_SEL_GET_TIME response */
#ifdef __linux__
typedef struct __attribute__((packed))
#else
typedef struct
#endif
{
    uint8_t completion_code;
    uint8_t b0_time;
    uint8_t b1_time;
    uint8_t b2_time;
    uint8_t b3_time;
} sel_get_time_resp_t;

/* IPMI_SEL_SET_TIME request */
#ifdef __linux__
typedef struct __attribute__((packed))
#else
typedef struct
#endif
{
    uint8_t b0_time;
    uint8_t b1_time;
    uint8_t b2_time;
    uint8_t b3_time;
} sel_set_time_req_t;

#ifdef __linux__
typedef struct __attribute__((packed))
#else
typedef struct
#endif
{
    uint8_t completion_code;
} sel_set_time_resp_t;

/* IPMI defined special SEL record Ids */
#define FIRST_SEL_RECORD 0     /* First record in the SEL */
#define LAST_SEL_RECORD 0xFFFF /* Last SEL entry ID */

/* IPMI defined SEL record types (lowest number of each range) */
#define SEL_RECORDTYPE_SYSEVENT 0x02 /* System Event */
#define SEL_RECORDTYPE_OEMTS 0xC0    /* OEM timestamped */
#define SEL_RECORDTYPE_OEMNTS 0xE0   /* OEM not timestamped */
#define SEL_RECORDTYPE_OEM_SENSOR (SEL_RECORDTYPE_OEMTS | 0x1)
#define SEL_RECORDTYPE_OEM_POWER (SEL_RECORDTYPE_OEMTS | 0x2)
#define SEL_RECORDTYPE_OEM_RESET (SEL_RECORDTYPE_OEMTS | 0x3)
#define SEL_RECORDTYPE_OEM_IR38163 (SEL_RECORDTYPE_OEMTS | 0x4)
#define SEL_RECORDTYPE_OEM_WATCHDOG (SEL_RECORDTYPE_OEMTS | 0x5)

#define SEL_RECORD_HAS_TIMESTAMP(t) ((t) < SEL_RECORDTYPE_OEMNTS)
} // namespace xpum
