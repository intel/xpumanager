/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file bsmc_ipmi_oem_cmd.h
 */

#pragma once

#include <stdint.h>
#include "..\ipmi\compiler_specific.h"
#include "pci_address.h"

#pragma warning(disable : 4201)
#pragma warning(disable : 4324)

#if defined(__cplusplus)
namespace xpum {
extern "C" {
#endif

/*
 * BSMC IPMI interface definition.
 * Please keep it packed and backward compatible.
*/

#define IPMI_UART_MGMT_READY 0x06

/* NetFn used for OEM commands */
#define IPMI_INTEL_OEM_NETFN 0x3e
#define IPMI_GET_DEVID_OEM_NETFN 0x6

/* OEM command identifiers */
#define IPMI_CARD_GET_INFO_CMD 0x00
#define IPMI_FW_GET_INFO_CMD 0x01
#define IPMI_FW_UPDATE_START_CMD 0x02
#define IPMI_FW_UPDATE_SYNC_CMD 0x03
#define IPMI_FW_UPDATE_SEND_DATA_CMD 0x04
#define IPMI_FW_REVERT_CMD 0x05
#define IPMI_AMC_RESET_CMD 0x06
#define IPMI_READ_SENSOR_CMD 0x07
#define IPMI_CSMC_BOOTLOADER_INFO_CMD 0x08
#define IPMI_CSMC_SERVICE_INFO_CMD 0x09
#define IPMI_CSMC_ICL_INIT_CMD 0x0a
#define IPMI_CSMC_ICL_STATUS_CMD 0x0b
#define IPMI_CSMC_ICL_DATA_CMD 0x0c
#define IPMI_DEBUG_CMD 0x0e
#define IPMI_EXT_SENSOR_INFO_CMD 0x0f
#define IPMI_CSMC_BOOTPCI_INFO_CMD 0x10
#define IPMI_CSMC_BUFFER_WRITE_CMD 0x11
#define IPMI_CSMC_BUFFER_READ_CMD 0x12
#define IPMI_CARD_SET_INFO_CMD 0x13
#define IPMI_TRANSFER_SIZE_DETECT 0x14
#define IPMI_ASIC_GET_INFO_CMD 0x15
#define IPMI_GET_MODULE_ID_CMD 0x16
#define IPMI_SET_EMI_FREQUENCY_ID_CMD 0x17
#define IPMI_SET_EMI_MITIGATION_STATE_CMD 0x18
#define IPMI_GET_EMI_MITIGATION_INFO_CMD 0x19
//#define IPMI_CSMC_BUFFER_STATUS_CMD	  0x20

#define NNP_PROJECT_CODENAME "NNP"
#define FRU_DATE_TIME_SIZE 3

typedef enum {
    BOARD_PRODUCT_LCR,
    BOARD_PRODUCT_SCR,
    BOARD_PRODUCT_SCR_PLUS,
    NUM_BOARD_PRODUCTS,
} board_product_t;

/*typedef enum {
	BOARD_TYPE_EBB = 0,
	BOARD_TYPE_400W_MEZZANINE,
	BOARD_TYPE_300W_PCIE_AIC,
	BOARD_TYPE_200W_MEZZANINE,
} board_sku_t;*/

typedef enum {
    CARD_INFO_MANUFACTURER_NAME,
    CARD_INFO_PRODUCT_NAME,
    CARD_INFO_SERIAL_NUMBER,
    CARD_INFO_PART_NUMBER,
} card_info_t;

typedef enum {
    BOARD_TYPE_EBB = 0,
    BOARD_TYPE_400W_MEZZANINE_V0_X = 1,
    BOARD_TYPE_300W_PCIE_AIC = 2,
    BOARD_TYPE_200W_MEZZANINE_V0_X = 3,
    BOARD_TYPE_400W_MEZZANINE_V1_0 = 5,
    BOARD_TYPE_200W_MEZZANINE_V1_0 = 7
} board_sku_t;

/*typedef enum {
    SKU_TYPE_COMPUTE_DENSE = 0,
    SKU_TYPE_PERFORMANCE   = 1,
    SKU_TYPE_VOLUME        = 2,
} sku_type_t;*/

/* Minimal size of card_get_info_res to support older firmwares */
#define CARD_GET_INFO_RES_MIN_SIZE 5

#define CARD_SET_INFO_MAX_DATA_SIZE 32

/* IPMI_GET_CARD_INFO_CMD */
#if !(_MSC_VER < 1910)
#pragma pack(push,1)
#endif
#if __linux__
typedef struct __attribute__((packed))
#else
typedef struct
#endif
{
    uint8_t completion_code;
    uint8_t project_codename[3];
    uint8_t peer_count;
    uint8_t protocol;
    pci_address_t pci_address;
    uint8_t board_product;
    uint8_t board_revision;
    uint8_t board_sku;
    uint32_t bar0_address;
} card_get_info_res;

#if __linux__
typedef struct __attribute__((packed))
#else
typedef struct
#endif
{
    uint8_t type;
    uint8_t data[CARD_SET_INFO_MAX_DATA_SIZE];
} card_set_info_req;

/* Firmware update using constant packet length equals 256 bytes */
#define VERSION_PROTOCOL_0 0
/* Send requested offset/size to BSMC, changes introduce in 1.3.0 */
#define VERSION_PROTOCOL_1 1

/* IPMI_FW_GET_VERSION_CMD */
#if __linux__
typedef struct __attribute__((packed))
#else
typedef struct
#endif
{
    uint8_t completion_code;
    uint32_t major;
    uint32_t minor;
    uint32_t patch;
    uint32_t build;
    uint8_t partition;
} fw_get_info_res;

typedef enum {
    FW_UPDATE_TYPE_INVALID = 0,
    FW_UPDATE_TYPE_BSMC,
    FW_UPDATE_TYPE_CSMC_FULL
} fw_update_type_t;

#if __linux__
typedef struct __attribute__((packed))
#else
typedef struct
#endif
{
    uint8_t fw_update_type;
} fw_update_start_req;

#if __linux__
typedef struct __attribute__((packed))
#else
typedef struct
#endif
{
    uint8_t completion_code;
} fw_update_start_res;

/* IPMI_FW_UPDATE_SYNC_CMD */
#if __linux__
typedef struct __attribute__((packed))
#else
typedef struct
#endif
{
    uint8_t completion_code;
    uint8_t status;
    uint32_t offset;
    uint32_t size;
} fw_update_sync_res;

/* IPMI_READ_SENSOR_CMD */
#define SENSOR_MAX_NAME 24

typedef enum {
    UNIT_MV,
    UNIT_MA,
    UNIT_MW,
    UNIT_W,
    UNIT_A,
    UNIT_C,
    UNIT_COUNT
} unit_t;

#if __linux__
typedef struct __attribute__((packed))
#else
typedef struct
#endif
{
    uint8_t sensor_index;
} read_sensor_req;

#if __linux__
typedef struct __attribute__((packed))
#else
typedef struct
#endif
{
    uint8_t completion_code;
    int reading;
    uint8_t unit;
} read_sensor_res;

/* IPMI_FW_UPDATE_SEND_DATA_CMD */
typedef enum {
    IPMI_FW_UPDATE_READ = 0,
    IPMI_FW_UPDATE_WAIT,
    IPMI_FW_UPDATE_COMPLETE,
    IPMI_FW_UPDATE_FAIL,
    IPMI_FW_UPDATE_SIGNATURE_FAIL,
    IPMI_FW_UPDATE_IMAGE_TO_LARGE_FAIL,
    IPMI_FW_UPDATE_NO_IMAGE_SIZE_FAIL,
    IPMI_FW_UPDATE_PACKET_TO_LARGE_FAIL,
    IPMI_FW_UPDATE_TO_MANY_RETRIES_FAIL,
    IPMI_FW_UPDATE_WRITE_TO_FLASH_FAIL,
    IPMI_FW_UPDATE_GET_FILE_SIZE,
} ipmi_fw_update_status_t;

typedef enum {
    IPMI_ICL_STATUS_IN_PROGRESS,
    IPMI_ICL_STATUS_SEND,
    IPMI_ICL_STATUS_RECV,
    IPMI_ICL_STATUS_COMPLETE,
    IPMI_ICL_STATUS_FAIL
} ipmi_icl_status_t;

/* IPMI_ICL_INIT_CMD */
#if __linux__
typedef struct __attribute__((packed))
#else
typedef struct
#endif
{
    uint16_t data_len;
} icl_init_req;

/* IPMI_ICL_STATUS_CMD */
#if __linux__
typedef struct __attribute__((packed))
#else
typedef struct
#endif
{
    uint8_t completion_code;
    uint8_t status;
} icl_status_res;

#define ICL_DATA_RES_DATA_SIZE 24

/* IPMI_ICL_READ_CMD */
#if __linux__
typedef struct __attribute__((packed))
#else
typedef struct
#endif
{
    uint8_t completion_code;
    uint8_t status;
    uint8_t data[ICL_DATA_RES_DATA_SIZE];
} icl_data_res;

#if __linux__
typedef struct __attribute__((packed))
#else
typedef struct
#endif
{
    uint8_t completion_code;
    uint8_t data[ICL_DATA_RES_DATA_SIZE];
} icl_read_res;

/* IPMI_EXT_SENSOR_INFO_CMD */
typedef struct
{
    uint8_t sensor_id;
    uint8_t request_type;
} ext_sensor_info_req;

typedef struct
{
    uint8_t completion_code;
    uint16_t data;
} ext_sensor_info_res;

typedef enum {
    EXT_SENSOR_ID_CORE_TEMP = 0,
    EXT_SENSOR_ID_TOTAL_POWER,
} ext_sensor_id_t;

typedef enum {
    EXT_SENSOR_TYPE_VALUE = 0,
    EXT_SENSOR_TYPE_WARN_THRESHOLD,
    EXT_SENSOR_TYPE_CRIT_THRESHOLD,
} ext_sensor_request_type_t;

/* IPMI_TRANSFER_SIZE_DETECT */

#if __linux__
typedef struct __attribute__((packed))
#else
typedef struct
#endif
{
    uint8_t completion_code;
    uint16_t received_bytes;
} transfer_size_detect_res;

/*
 * POST codes
 */
/* typedef enum {
	 //Bootloader status (range 0x0 - 0xf)
	BOARD_STATUS_LOADER_INIT                      = 0x0,
	BOARD_STATUS_LOADER_INIT_BOOT_CONFIG          = 0x1,
	BOARD_STATUS_LOADER_INIT_COMPLETED            = 0x2,
	BOARD_STATUS_LOADER_PROGRAM_PRIMARY_STARTED   = 0x3,
	BOARD_STATUS_LOADER_PROGRAM_SECONDARY_STARTED = 0x4,

	//Program status (range 0x10 - 0x9f)
	BOARD_STATUS_INIT_PINOUT                    = 0x10,
	BOARD_STATUS_INIT_CONSOLE                   = 0x11,
	BOARD_STATUS_INIT_EEPROM                    = 0x12,
	BOARD_STATUS_INIT_PERSISTENT_LOG            = 0x13,
	BOARD_STATUS_INIT_SHA                       = 0x14,
	BOARD_STATUS_INIT_ADC                       = 0x15,
	BOARD_STATUS_INIT_I2C                       = 0x16,
	BOARD_STATUS_INIT_SSI                       = 0x17,
	BOARD_STATUS_INIT_FRU                       = 0x18,
	BOARD_STATUS_INIT_IPMB                      = 0x19,
	BOARD_STATUS_INIT_FW_UPDATE                 = 0x1a,
	BOARD_STATUS_INIT_GPIO_INTR                 = 0x1b,
	BOARD_STATUS_INIT_PWR_CTRL                  = 0x1c,
	BOARD_STATUS_INIT_SENSORS                   = 0x1d,
	BOARD_STATUS_INIT_WATCHDOG                  = 0x1e,
	BOARD_STATUS_INIT_CSMC_WATCHDOG             = 0x1f,
	BOARD_STATUS_INIT_COMPLETED                 = 0x20,
	BOARD_STATUS_WAIT_PWR_UP                    = 0x21,
	BOARD_STATUS_WAIT_HOST_PWR_OK               = 0x22,
	BOARD_STATUS_PWR_UP_SEQ_STARTED             = 0x23,
	BOARD_STATUS_PWR_UP_SEQ_COMPLETED           = 0x24,
	BOARD_STATUS_PWR_DOWN_SEQ_STARTED           = 0x25,
	BOARD_STATUS_PWR_DOWN_SEQ_COMPLETED         = 0x26,
	BOARD_STATUS_VID_ACTIVE_COMPLETED           = 0x27,
	BOARD_STATUS_VID_BOOT_COMPLETED             = 0x28,

	// Errors (range 0xa0 - 0xef) - letter in front means error
	BOARD_STATUS_FIRST_ERROR                    = 0xa0,

	BOARD_STATUS_HOST_PWR_FAIL                  = BOARD_STATUS_FIRST_ERROR,
	BOARD_STATUS_P5V0_VR_FAIL                   = 0xa1,
	BOARD_STATUS_CORE_VR_FAIL                   = 0xa2,
	BOARD_STATUS_P2V5_VR_FAIL                   = 0xa3,
	BOARD_STATUS_P1V8_VR_FAIL                   = 0xa4,
	BOARD_STATUS_P1V5_VR_FAIL                   = 0xa5,
	BOARD_STATUS_P1V2_VR_FAIL                   = 0xa6,
	BOARD_STATUS_P1V0_VR_FAIL                   = 0xa7,
	BOARD_STATUS_P0V9_VR_FAIL                   = 0xa8,
	BOARD_STATUS_SPI_PLL_FAIL                   = 0xa9,
	BOARD_STATUS_VR_PWR_GOOD_DROPPED            = 0xaa,
	BOARD_STATUS_AUX_PWR_CONN_MISSING           = 0xab,
	BOARD_STATUS_AUX_PWR_CONN_DROPPED           = 0xac,
	BOARD_STATUS_P1V8_BIAS_VR_FAIL              = 0xad,
	BOARD_STATUS_P1V8_VDDH_VR_FAIL              = 0xae,
	BOARD_STATUS_P1V2_VDDQ1_VR_FAIL             = 0xaf,
	BOARD_STATUS_P1V2_VDDQ2_VR_FAIL             = 0xb0,
	BOARD_STATUS_PERST_DEASSERT_FAIL            = 0xb1,
	BOARD_STATUS_VID_ACTIVE_FAILED              = 0xb2,
	BOARD_STATUS_VID_BOOT_FAILED                = 0xb3,
	BOARD_STATUS_PWR_DOWN_ERROR                 = 0xb4,
	BOARD_STATUS_HOST_I2C_HANG                  = 0xb5,
	BOARD_STATUS_BSMC_FW_UPDATE_FAIL            = 0xb6,
	BOARD_STATUS_CORE_VR_WARN_TMP               = 0xb7,
	BOARD_STATUS_CSMC_WATCHDOG_TIMEOUT          = 0xb8,
	BOARD_STATUS_SENSOR_THRESHOLD_EXCEEDED      = 0xb9,
	BOARD_STATUS_SENSOR_NOT_AVAILABLE           = 0xba,
	BOARD_STATUS_P12V0_VR_FAIL                  = 0xbb,
	BOARD_STATUS_ASIC_PRSNT_LOST                = 0xbc,
	BOARD_STATUS_SENSOR_CONFIG_ERROR            = 0xbd,
	BOARD_STATUS_VR_CONFIG_ERROR                = 0xbe,
	BOARD_STATUS_SPI_RESET_FAIL                 = 0xbf,
	BOARD_STATUS_WARN_TEMP_ERROR                = 0xc0,
	BOARD_STATUS_CRIT_TEMP_ERROR                = 0xc1,
	BOARD_STATUS_SENSOR_TOTAL_POWER_EXCEEDED    = 0xc2,
	BOARD_STATUS_VID_ACTIVE_REQ_FAILED          = 0xc3,
	BOARD_STATUS_VID_ACTIVE_VALUE_FAILED        = 0xc4,
	BOARD_STATUS_VID_ACTIVE_ACK_FAILED          = 0xc5,
	BOARD_STATUS_IR38163_UPDATE_NO_MORE_ATTEMPS = 0xc6,
	BOARD_STATUS_IR38163_UPDATE_TIME_OUT        = 0xc7,

	// Fatal Errors (range 0xf0 - 0xff)
	BOARD_STATUS_FIRST_FATAL_ERROR              = 0xf0,

	BOARD_STATUS_NO_VALID_PROGRAM               = BOARD_STATUS_FIRST_FATAL_ERROR,
	BOARD_STATUS_WATCHDOG_TIMEOUT               = 0xf1,
	BOARD_STATUS_STACK_OVERFLOW                 = 0xf9,
	BOARD_STATUS_ASSERT                         = 0xfa,
	BOARD_STATUS_BUS_FAULT                      = 0xfb,
	BOARD_STATUS_BUS_FAULT_VECTOR               = 0xfc,
	BOARD_STATUS_USAGE_FAULT                    = 0xfd,
	BOARD_STATUS_MEMORY_MANAGE_FAULT            = 0xfe,
	BOARD_STATUS_HARD_FAULT                     = 0xff,
} board_status_t;*/

/*
 * POST codes
 */
typedef enum {
    /* Bootloader status (range 0x0 - 0xf) */
    BOARD_STATUS_LOADER_INIT = 0x0,
    BOARD_STATUS_LOADER_INIT_BOOT_CONFIG = 0x1,
    BOARD_STATUS_LOADER_INIT_COMPLETED = 0x2,
    BOARD_STATUS_LOADER_PROGRAM_PRIMARY_STARTED = 0x3,
    BOARD_STATUS_LOADER_PROGRAM_SECONDARY_STARTED = 0x4,

    /* Program status (range 0x10 - 0x9f) */
    BOARD_STATUS_INIT_PINOUT = 0x10,
    BOARD_STATUS_INIT_CONSOLE = 0x11,
    BOARD_STATUS_INIT_EEPROM = 0x12,
    BOARD_STATUS_INIT_PERSISTENT_LOG = 0x13,
    BOARD_STATUS_INIT_SHA = 0x14,
    BOARD_STATUS_INIT_ADC = 0x15,
    BOARD_STATUS_INIT_I2C = 0x16,
    BOARD_STATUS_INIT_SSI = 0x17,
    BOARD_STATUS_INIT_FRU = 0x18,
    BOARD_STATUS_INIT_IPMB = 0x19,
    BOARD_STATUS_INIT_FW_UPDATE = 0x1a,
    BOARD_STATUS_INIT_GPIO_INTR = 0x1b,
    BOARD_STATUS_INIT_PWR_CTRL = 0x1c,
    BOARD_STATUS_INIT_SENSORS = 0x1d,
    BOARD_STATUS_INIT_WATCHDOG = 0x1e,
    //BOARD_STATUS_INIT_CSMC_TASK                 = 0x1f,
    //BOARD_STATUS_INIT_CSMC_WATCHDOG             = 0x20,
    BOARD_STATUS_INIT_COMPLETED = 0x21,
    BOARD_STATUS_WAIT_PWR_UP = 0x22,
    BOARD_STATUS_WAIT_HOST_PWR_OK = 0x23,
    BOARD_STATUS_PWR_UP_SEQ_STARTED = 0x24,
    BOARD_STATUS_PWR_UP_SEQ_COMPLETED = 0x25,
    BOARD_STATUS_PWR_DOWN_SEQ_STARTED = 0x26,
    BOARD_STATUS_PWR_DOWN_SEQ_COMPLETED = 0x27,
    //BOARD_STATUS_VID_ACTIVE_COMPLETED           = 0x28,
    //BOARD_STATUS_VID_BOOT_COMPLETED             = 0x29,

    /* Errors (range 0xa0 - 0xef) - letter in front means error */
    BOARD_STATUS_FIRST_ERROR = 0xa0,
    BOARD_STATUS_HOST_PWR_FAIL = BOARD_STATUS_FIRST_ERROR,
    BOARD_STATUS_P5V0_VR_FAIL = 0xa1,
    BOARD_STATUS_CORE_VR_FAIL = 0xa2,
    BOARD_STATUS_P2V5_VR_FAIL = 0xa3,
    BOARD_STATUS_P1V8_VR_FAIL = 0xa4, /* Only on LCR */
    BOARD_STATUS_P1V5_VR_FAIL = 0xa5, /* Only on LCR */
    BOARD_STATUS_P1V2_VR_FAIL = 0xa6, /* Only on LCR */
    BOARD_STATUS_P1V0_VR_FAIL = 0xa7, /* Only on LCR */
    BOARD_STATUS_P0V9_VR_FAIL = 0xa8,
    BOARD_STATUS_REF_CLOCK_FAIL = 0xa9,
    BOARD_STATUS_VR_PWR_GOOD_DROPPED = 0xaa,
    BOARD_STATUS_AUX_PWR_CONN_MISSING = 0xab, /* not used */
    BOARD_STATUS_AUX_PWR_CONN_DROPPED = 0xac, /* Only on LCR */
    BOARD_STATUS_P1V8_BIAS_VR_FAIL = 0xad,
    BOARD_STATUS_P1V8_VDDH_VR_FAIL = 0xae,
    BOARD_STATUS_P1V2_VDDQ1_VR_FAIL = 0xaf,
    BOARD_STATUS_P1V2_VDDQ2_VR_FAIL = 0xb0,
    BOARD_STATUS_PERST_DEASSERT_FAIL = 0xb1, /* not used */
    BOARD_STATUS_VID_ACTIVE_FAILED = 0xb2,
    BOARD_STATUS_VID_BOOT_FAILED = 0xb3, /* Only on LCR */
    BOARD_STATUS_PWR_DOWN_ERROR = 0xb4,
    BOARD_STATUS_HOST_I2C_HANG = 0xb5,      /* not used */
    BOARD_STATUS_AMC_FW_UPDATE_FAIL = 0xb6, /* not used */
    BOARD_STATUS_CORE_VR_WARN_TMP = 0xb7,   /* not used */
    //BOARD_STATUS_CSMC_WATCHDOG_TIMEOUT          = 0xb8,
    BOARD_STATUS_SENSOR_THRESHOLD_EXCEEDED = 0xb9, /* post code and leds only */
    BOARD_STATUS_SENSOR_NOT_AVAILABLE = 0xba,      /* post code and leds only */
    BOARD_STATUS_P12V0_VR_FAIL = 0xbb,
    BOARD_STATUS_ASIC_PRSNT_LOST = 0xbc,
    BOARD_STATUS_SENSOR_CONFIG_ERROR = 0xbd,
    BOARD_STATUS_CORE_VR_CONFIG_ERROR = 0xbe,
    //BOARD_STATUS_SPI_RESET_FAIL                 = 0xbf,
    //BOARD_STATUS_WARN_TEMP_ERROR                = 0xc0,
    //BOARD_STATUS_CRIT_TEMP_ERROR                = 0xc1,
    //BOARD_STATUS_SENSOR_TOTAL_POWER_EXCEEDED    = 0xc2,
    BOARD_STATUS_ATS_NUMBER_OF_TILES_INVAL = 0xc3,
    BOARD_STATUS_ATS_TDP_INVAL = 0xc4,
    BOARD_STATUS_P3V3_AUX_INRAIL_FAIL = 0xc5,
    BOARD_STATUS_ATS_INPUT_RAILS_FAIL = 0xc6,
    BOARD_STATUS_VPP_VR_FAIL = 0xc7,
    BOARD_STATUS_VCCFA_EHV_VR_FAIL = 0xc8,
    BOARD_STATUS_VCCINFAON_VR_FAIL = 0xc9,
    BOARD_STATUS_VCCIN_VR_FAIL = 0xca,
    BOARD_STATUS_HOST_PERST_TIMEOUT = 0xcb,
    BOARD_STATUS_INIT_AMC_WATCHDOG = 0xcc,
    BOARD_STATUS_AMC_WATCHDOG_TIMEOUT = 0xcd,
    BOARD_STATUS_VRHOT_DETECTED = 0xce,
    BOARD_STATUS_MEMHOT_DETECTED = 0xcf,
    BOARD_STATUS_ATS_CARD_TYPE_INVAL = 0xd0,
    BOARD_STATUS_ATS_PLATFORM_TYPE_INVAL = 0xd1,
    BOARD_STATUS_ATS_FAB_ID_INVAL = 0xd2,
    BOARD_STATUS_VPP_VR_I2C_FAIL = 0xd3,
    BOARD_STATUS_VCCFA_EHV_VR_I2C_FAIL = 0xd4,
    BOARD_STATUS_VCCINFAON_VR_I2C_FAIL = 0xd5,
    BOARD_STATUS_VCCIN_VR_I2C_FAIL = 0xd6,
    BOARD_STATUS_PERST_N_I2C_FAIL = 0xd7,
    BOARD_STATUS_EXPD_OUT_CFG_I2C_FAIL = 0xd8,

    /* Fatal Errors (range 0xf0 - 0xff) */
    BOARD_STATUS_NO_VALID_PROGRAM = 0xf0,
    BOARD_STATUS_WATCHDOG_TIMEOUT = 0xf1,

    BOARD_STATUS_STACK_OVERFLOW = 0xf9,
    BOARD_STATUS_ASSERT = 0xfa,
    BOARD_STATUS_BUS_FAULT = 0xfb,
    BOARD_STATUS_BUS_FAULT_VECTOR = 0xfc,
    BOARD_STATUS_USAGE_FAULT = 0xfd,
    BOARD_STATUS_MEMORY_MANAGE_FAULT = 0xfe,
    BOARD_STATUS_HARD_FAULT = 0xff,
} board_status_t;

/*
 * Persistent log
 */
#define PLOG_LAST_ENTRY_INDEX 0xffff

typedef enum {
    PLOG_ENTRY_EMPTY = 0,
    PLOG_ENTRY_LOG_HEAD,
    PLOG_ENTRY_RESET,
    PLOG_ENTRY_SENSOR_THRESHOLD_EXCEEDED,
    PLOG_ENTRY_SENSOR_NOT_AVAILABLE,
    PLOG_ENTRY_BOARD,
    //PLOG_ENTRY_CSMC,
    PLOG_ENTRY_FW_UPDATED,
    //PLOG_ENTRY_VERSION_MISMATCH,
} plog_entry_type_t;

typedef enum {
    PLOG_ERROR = 0,
    PLOG_WARNING,
    PLOG_INFO,
    PLOG_DEBUG
} plog_level_t;

#if __linux__
typedef struct __attribute__((packed))
#else
typedef struct
#endif
{
    uint32_t partition : 1;
    uint32_t major : 8;
    uint32_t minor : 8;
    uint32_t patch : 8;
    uint32_t build : 23;
} plog_program_version_t;

/*typedef enum {
    PLOG_CSMC_INVALID               = 0x0,
    ASIC_INIT_ERROR_HBM             = 0x1,
    ASIC_INIT_ERROR_TPC             = 0x2,
    ASIC_INIT_ERROR_OTHER           = 0x3,
    ASIC_HBM_TEMPERATURE_CRITICAL   = 0x4,
    ASIC_HBM_TEMPERATURE_WARNING    = 0x5,
    ASIC_HBM_PARITY_ERROR           = 0x6,
} plog_csmc_type_t;*/

#if __linux__
typedef struct __attribute__((packed))
#else
typedef struct
#endif
{
    uint8_t type;
    union {
        uint32_t hbm_number;
        uint32_t tpc_number;
    };
    union {
        uint8_t hbm_temperature;
        struct {
            uint8_t err_ps0 : 1;
            uint8_t err_ps1 : 1;
            uint8_t instance : 6;
        } ca_parity;
    };
} plog_csmc_data_t;

#if __linux__
typedef union __attribute__((packed))
#else
typedef union
#endif
{
    uint8_t raw_data[7];

    uint8_t ir38163_update_status;
    uint8_t spi_reset_status;
#if __linux__
    struct __attribute__((packed))
#else
    struct
#endif
    {
        plog_program_version_t version;
        uint8_t reset_source;
    } startup;

#if __linux__
    struct __attribute__((packed))
#else
    struct
#endif
    {
        int32_t value;
        uint8_t id;
        uint16_t _unused; /* TODO: find usage for these 2 bytes */
    } sensor;

#if __linux__
    struct __attribute__((packed))
#else
    struct
#endif
    {
        uint32_t program_counter;
        uint16_t power_events;
        uint8_t board_status;
    } board_error;

#if __linux__
    struct __attribute__((packed))
#else
    struct
#endif
    {
        plog_program_version_t version;
        uint8_t update_type;
    } update;

#if __linux__
    struct __attribute__((packed))
#else
    struct
#endif
    {
        int32_t value;
    } total_power_exceeded;

    plog_csmc_data_t csmc;

} plog_entry_data_t;

#if __linux__
typedef struct __attribute__((packed))
#else
typedef struct
#endif
{
    uint8_t type : 6; /* has to be first */
    uint8_t level : 2;
    uint32_t timestamp; /* unix time (seconds since 1/1/1970) */
} plog_entry_header_t;

#if __linux__
typedef struct __attribute__((packed))
#else
typedef struct
#endif
{
    plog_entry_header_t header;
    plog_entry_data_t data;
} plog_entry_t;

/* IPMI_DEBUG_CMD */
#define DEBUG_INFO_GET_RESET_COUNT 1
#define DEBUG_INFO_PLOG_GET_ENTRY 2
#define DEBUG_INFO_PLOG_ERASE 3

typedef enum {
    DEBUG_GET_RESET_COUNT = 1,
    DEBUG_GET_PLOG_ENTRY = 2,
    DEBUG_PLOG_ERASE = 3,
    DEBUG_SENSOR_GET_LIMITS = 4,
    DEBUG_SENSOR_SET_LOWER_LIMIT = 5,
    DEBUG_SENSOR_SET_UPPER_LIMIT = 6,
    DEBUG_SET_POWER_BREAK = 7,
    DEBUG_SET_POWER_REDUCTION = 8,
    DEBUG_SET_HIGH_POWER = 9,
} debug_request_type_t;

#if __linux__
typedef struct __attribute__((packed))
#else
typedef struct
#endif
{
    uint8_t request_type;
    union {
#if __linux__
        struct __attribute__((packed))
#else
        struct
#endif
        {
            uint16_t entry_nr;
        } entry;

#if __linux__
        struct __attribute__((packed))
#else
        struct
#endif
        {
            uint8_t sensor_id;
        } sensor_get_limits;

#if __linux__
        struct __attribute__((packed))
#else
        struct
#endif
        {
            uint8_t sensor_id;
            int32_t value;
        } sensor_set_lower_limit;
#if __linux__
        struct __attribute__((packed))
#else
        struct
#endif
        {
            uint8_t sensor_id;
            int32_t value;
        } sensor_set_upper_limit;

#if __linux__
        struct __attribute__((packed))
#else
        struct
#endif
        {
            uint8_t value;
        } set_power_break;
#if __linux__
        struct __attribute__((packed))
#else
        struct
#endif
        {
            uint8_t level;
        } set_power_reduction;

#if __linux__
        struct __attribute__((packed))
#else
        struct
#endif
        {
            uint8_t value;
        } set_high_power;
    };
} debug_req;

#if __linux__
typedef struct __attribute__((packed))
#else
typedef struct
#endif
{
    uint8_t completion_code;
    union {
#if __linux__
        struct __attribute__((packed))
#else
        struct
#endif
        {
            uint32_t amc;
            //uint32_t csmc;
        } reset_count;

#if __linux__
        struct __attribute__((packed))
#else
        struct
#endif
        {
            plog_entry_t data;
            uint16_t prev_entry;
        } entry;
#if __linux__
        struct __attribute__((packed))
#else
        struct
#endif
        {
            int32_t lower_limit;
            int32_t upper_limit;
        } sensor_get_limits;
    };
} debug_res;

typedef enum {
    RESET_SOURCE_POR = (1 << 0),
    RESET_SOURCE_SW = (1 << 1),
    RESET_SOURCE_WDT = (1 << 2),
    RESET_SOURCE_EXT = (1 << 3),
    RESET_SOURCE_BOR = (1 << 4),
    RESET_SOURCE_LOCKUP = (1 << 5),
    RESET_SOURCE_HIBERNATE = (1 << 6),
    RESET_SOURCE_HSSR = (1 << 7),
} board_reset_source_t;

/*typedef enum {
	SENSOR_ADC_BIAS_1V8_VOLTAGE,
	SENSOR_ADC_BIAS_5V0_VOLTAGE,

	SENSOR_ADC_VDD_CORE_VOLTAGE,
	SENSOR_VR_VDD_CORE_VOLTAGE,
	SENSOR_VR_VDD_CORE_POWER,
	SENSOR_VR_VDD_CORE_CURRENT,
	SENSOR_VR_VDD_CORE_TEMP,

	SENSOR_ADC_VPP_HBM_VOLTAGE,

	SENSOR_ADC_VDDQ1_HBM_VOLTAGE,
	SENSOR_VR_VDDQ1_HBM_VOLTAGE,
	SENSOR_VR_VDDQ1_HBM_POWER,
	SENSOR_VR_VDDQ1_HBM_CURRENT,
	SENSOR_VR_VDDQ_CONTROLLER_TEMP,

	SENSOR_ADC_VDDQ2_HBM_VOLTAGE,
	SENSOR_VR_VDDQ2_HBM_VOLTAGE,
	SENSOR_VR_VDDQ2_HBM_POWER,
	SENSOR_VR_VDDQ2_HBM_CURRENT,
	SENSOR_VR_VDDQ_PWRSTAGE_TEMP,

	SENSOR_ADC_AVDD_PLL_VOLTAGE,
	SENSOR_VR_AVDD_ANALOG_VOLTAGE,
	SENSOR_VR_AVDD_ANALOG_POWER,
	SENSOR_VR_AVDD_ANALOG_CURRENT,

	SENSOR_ADC_VDDH_IO_VOLTAGE,
	SENSOR_ADC_VDDA_PCIE_VOLTAGE,
	SENSOR_ADC_VDDHA_ICL_VOLTAGE,
	SENSOR_VR_VDDA_ICL_VOLTAGE,
	SENSOR_VR_VDDA_ICL_POWER,
	SENSOR_VR_VDDA_ICL_CURRENT,

	SENSOR_INLET_TEMP,
	SENSOR_OUTLET_TEMP,

	SENSOR_ADC_CONN_12V0_VOLTAGE,
	SENSOR_PMON_CONN_12V0_VOLTAGE,
	SENSOR_PMON_CONN_12V0_POWER,
	SENSOR_PMON_CONN_12V0_CURRENT,

	SENSOR_PMON_AUX1_12V0_VOLTAGE,
	SENSOR_PMON_AUX1_12V0_POWER,
	SENSOR_PMON_AUX1_12V0_CURRENT,

	SENSOR_PMON_AUX2_12V0_VOLTAGE,
	SENSOR_PMON_AUX2_12V0_POWER,
	SENSOR_PMON_AUX2_12V0_CURRENT,

	SENSOR_ADC_PCIE_12V0_VOLTAGE,
	SENSOR_PMON_PCIE_12V0_VOLTAGE,
	SENSOR_PMON_PCIE_12V0_POWER,
	SENSOR_PMON_PCIE_12V0_CURRENT,

	SENSOR_ADC_PCIE_3V3_VOLTAGE,
	SENSOR_PMON_PCIE_3V3_VOLTAGE,
	SENSOR_PMON_PCIE_3V3_POWER,
	SENSOR_PMON_PCIE_3V3_CURRENT,

	SENSOR_HBM0_TEMP,
	SENSOR_HBM1_TEMP,
	SENSOR_HBM2_TEMP,
	SENSOR_HBM3_TEMP,

	SENSOR_ASIC_TPC00_TEMP,
	SENSOR_ASIC_TPC01_TEMP,
	SENSOR_ASIC_TPC02_TEMP,
	SENSOR_ASIC_TPC03_TEMP,
	SENSOR_ASIC_TPC10_TEMP,
	SENSOR_ASIC_TPC11_TEMP,
	SENSOR_ASIC_TPC12_TEMP,
	SENSOR_ASIC_TPC13_TEMP,
	SENSOR_ASIC_TPC20_TEMP,
	SENSOR_ASIC_TPC21_TEMP,
	SENSOR_ASIC_TPC22_TEMP,

	SENSOR_ASIC_TPC23_TEMP,
	SENSOR_ASIC_TPC32_TEMP,
	SENSOR_ASIC_TPC33_TEMP,
	SENSOR_ASIC_TPC42_TEMP,
	SENSOR_ASIC_TPC43_TEMP,
	SENSOR_ASIC_HBM0_PHY_TEMP,
	SENSOR_ASIC_HBM1_PHY_TEMP,
	SENSOR_ASIC_HBM2_PHY_TEMP,
	SENSOR_ASIC_HBM3_PHY_TEMP,
	SENSOR_ASIC_ICG_SOUTH_TEMP,
	SENSOR_ASIC_ICG_NORTH_TEMP,

	SENSOR_ADC_MEZZ_54V0_VOLTAGE,
	SENSOR_ADC_NBM_12V0_VOLTAGE,

	SENSOR_PMON_MEZZ_54V0_VOLTAGE,
	SENSOR_PMON_MEZZ_54V0_POWER,
	SENSOR_PMON_MEZZ_54V0_CURRENT,

	SENSOR_COUNT
} sensor_id_t;*/
typedef enum {
    /*SENSOR_ADC_BIAS_1V8_VOLTAGE,
    SENSOR_ADC_BIAS_5V0_VOLTAGE,

    SENSOR_ADC_VDD_CORE_VOLTAGE,
    SENSOR_VR_VDD_CORE_VOLTAGE,
    SENSOR_VR_VDD_CORE_POWER,
    SENSOR_VR_VDD_CORE_CURRENT,
    SENSOR_VR_VDD_CORE_TEMP,

    SENSOR_ADC_VPP_HBM_VOLTAGE,

    SENSOR_ADC_VDDQ1_HBM_VOLTAGE,
    SENSOR_VR_VDDQ1_HBM_VOLTAGE,
    SENSOR_VR_VDDQ1_HBM_POWER,
    SENSOR_VR_VDDQ1_HBM_CURRENT,
    SENSOR_VR_VDDQ_CONTROLLER_TEMP,

    SENSOR_ADC_VDDQ2_HBM_VOLTAGE,
    SENSOR_VR_VDDQ2_HBM_VOLTAGE,
    SENSOR_VR_VDDQ2_HBM_POWER,
    SENSOR_VR_VDDQ2_HBM_CURRENT,
    SENSOR_VR_VDDQ_PWRSTAGE_TEMP,

    SENSOR_ADC_AVDD_PLL_VOLTAGE,
    SENSOR_VR_AVDD_ANALOG_VOLTAGE,
    SENSOR_VR_AVDD_ANALOG_POWER,
    SENSOR_VR_AVDD_ANALOG_CURRENT,

    SENSOR_ADC_VDDH_IO_VOLTAGE,
    SENSOR_ADC_VDDA_PCIE_VOLTAGE,
    SENSOR_ADC_VDDHA_ICL_VOLTAGE,
    SENSOR_VR_VDDA_ICL_VOLTAGE,
    SENSOR_VR_VDDA_ICL_POWER,
    SENSOR_VR_VDDA_ICL_CURRENT,*/

    SENSOR_INLET_TEMP,
    SENSOR_OUTLET_TEMP,

    SENSOR_MARGIN_TO_TCONTROL_TEMP,
    SENSOR_MARGIN_TO_TPROCHOT_TEMP,

    /*SENSOR_ADC_CONN_12V0_VOLTAGE,
    SENSOR_PMON_CONN_12V0_VOLTAGE,
    SENSOR_PMON_CONN_12V0_POWER,
    SENSOR_PMON_CONN_12V0_CURRENT,

    SENSOR_PMON_AUX1_12V0_VOLTAGE,
    SENSOR_PMON_AUX1_12V0_POWER,
    SENSOR_PMON_AUX1_12V0_CURRENT,

    SENSOR_PMON_AUX2_12V0_VOLTAGE,
    SENSOR_PMON_AUX2_12V0_POWER,
    SENSOR_PMON_AUX2_12V0_CURRENT,

    SENSOR_ADC_PCIE_12V0_VOLTAGE,
    SENSOR_PMON_PCIE_12V0_VOLTAGE,
    SENSOR_PMON_PCIE_12V0_POWER,
    SENSOR_PMON_PCIE_12V0_CURRENT,

    SENSOR_ADC_PCIE_3V3_VOLTAGE,
    SENSOR_PMON_PCIE_3V3_VOLTAGE,
    SENSOR_PMON_PCIE_3V3_POWER,
    SENSOR_PMON_PCIE_3V3_CURRENT,

    SENSOR_HBM0_TEMP,
    SENSOR_HBM1_TEMP,
    SENSOR_HBM2_TEMP,
    SENSOR_HBM3_TEMP,

    SENSOR_ASIC_TPC00_TEMP,
    SENSOR_ASIC_TPC01_TEMP,
    SENSOR_ASIC_TPC02_TEMP,
    SENSOR_ASIC_TPC03_TEMP,
    SENSOR_ASIC_TPC10_TEMP,
    SENSOR_ASIC_TPC11_TEMP,
    SENSOR_ASIC_TPC12_TEMP,
    SENSOR_ASIC_TPC13_TEMP,
    SENSOR_ASIC_TPC20_TEMP,
    SENSOR_ASIC_TPC21_TEMP,
    SENSOR_ASIC_TPC22_TEMP,

    SENSOR_ASIC_TPC23_TEMP,
    SENSOR_ASIC_TPC32_TEMP,
    SENSOR_ASIC_TPC33_TEMP,
    SENSOR_ASIC_TPC42_TEMP,
    SENSOR_ASIC_TPC43_TEMP,
    SENSOR_ASIC_HBM0_PHY_TEMP,
    SENSOR_ASIC_HBM1_PHY_TEMP,
    SENSOR_ASIC_HBM2_PHY_TEMP,
    SENSOR_ASIC_HBM3_PHY_TEMP,
    SENSOR_ASIC_ICG_SOUTH_TEMP,
    SENSOR_ASIC_ICG_NORTH_TEMP,

    SENSOR_ADC_MEZZ_54V0_VOLTAGE,
    SENSOR_ADC_NBM_12V0_VOLTAGE,

    SENSOR_PMON_MEZZ_54V0_VOLTAGE,
    SENSOR_PMON_MEZZ_54V0_POWER,
    SENSOR_PMON_MEZZ_54V0_CURRENT,

    SENSOR_PMON_TOTAL_POWER,*/

    SENSOR_PMBUS_MP2971_VOLTAGE,
    SENSOR_PMBUS_MP2971_POWER,
    SENSOR_PMBUS_MP2971_CURRENT,

    SENSOR_PMBUS_MP2975_0_VCCIN_VOLTAGE,
    SENSOR_PMBUS_MP2975_0_VCCIN_POWER,
    SENSOR_PMBUS_MP2975_0_VCCIN_CURRENT,

    SENSOR_PMBUS_MP2975_0_VCCFA_EHV_VOLTAGE,
    SENSOR_PMBUS_MP2975_0_VCCFA_EHV_POWER,
    SENSOR_PMBUS_MP2975_0_VCCFA_EHV_CURRENT,

    SENSOR_SOC_DIE_TEMP_0,
    SENSOR_HBM0_TEMP_0,
    SENSOR_HBM1_TEMP_0,

    SENSOR_PMBUS_MP2975_1_VCCIN_VOLTAGE,
    SENSOR_PMBUS_MP2975_1_VCCIN_POWER,
    SENSOR_PMBUS_MP2975_1_VCCIN_CURRENT,

    SENSOR_PMBUS_MP2975_1_VCCFA_EHV_VOLTAGE,
    SENSOR_PMBUS_MP2975_1_VCCFA_EHV_POWER,
    SENSOR_PMBUS_MP2975_1_VCCFA_EHV_CURRENT,

    SENSOR_SOC_DIE_TEMP_1,
    SENSOR_HBM0_TEMP_1,
    SENSOR_HBM1_TEMP_1,

    /*    SENSOR_PMBUS_MP2975_2_VCCIN_VOLTAGE,
    SENSOR_PMBUS_MP2975_2_VCCIN_POWER,
    SENSOR_PMBUS_MP2975_2_VCCIN_CURRENT,

    SENSOR_PMBUS_MP2975_2_VCCFA_EHV_VOLTAGE,
    SENSOR_PMBUS_MP2975_2_VCCFA_EHV_POWER,
    SENSOR_PMBUS_MP2975_2_VCCFA_EHV_CURRENT,

    SENSOR_SOC_DIE_TEMP_2,
    SENSOR_HBM0_TEMP_2,
    SENSOR_HBM1_TEMP_2,

    SENSOR_PMBUS_MP2975_3_VCCIN_VOLTAGE,
    SENSOR_PMBUS_MP2975_3_VCCIN_POWER,
    SENSOR_PMBUS_MP2975_3_VCCIN_CURRENT,

    SENSOR_PMBUS_MP2975_3_VCCFA_EHV_VOLTAGE,
    SENSOR_PMBUS_MP2975_3_VCCFA_EHV_POWER,
    SENSOR_PMBUS_MP2975_3_VCCFA_EHV_CURRENT,

    SENSOR_SOC_DIE_TEMP_3,
    SENSOR_HBM0_TEMP_3,
    SENSOR_HBM1_TEMP_3,*/

    SENSOR_COUNT
} sensor_index_t;

#pragma pack(pop)
#if defined(__cplusplus)
}
}
#endif
