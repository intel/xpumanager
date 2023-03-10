/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file sdr.h
 */
#pragma once

#include <cstdint>
#include <cmath>
#include <cstring>
#pragma warning(disable:4146)

//#define ATTRIBUTE_PACKING __attribute__ ((packed))
#define ATTRIBUTE_PACKING  
#pragma pack (push, 1)
struct sdr_get_rs
{
    uint16_t next;   /* next record id */
    uint16_t id;     /* record ID */
    uint8_t version; /* SDR version (51h) */
#define SDR_RECORD_TYPE_FULL_SENSOR 0x01
#define SDR_RECORD_TYPE_COMPACT_SENSOR 0x02
#define SDR_RECORD_TYPE_EVENTONLY_SENSOR 0x03
#define SDR_RECORD_TYPE_ENTITY_ASSOC 0x08
#define SDR_RECORD_TYPE_DEVICE_ENTITY_ASSOC 0x09
#define SDR_RECORD_TYPE_GENERIC_DEVICE_LOCATOR 0x10
#define SDR_RECORD_TYPE_FRU_DEVICE_LOCATOR 0x11
#define SDR_RECORD_TYPE_MC_DEVICE_LOCATOR 0x12
#define SDR_RECORD_TYPE_MC_CONFIRMATION 0x13
#define SDR_RECORD_TYPE_BMC_MSG_CHANNEL_INFO 0x14
#define SDR_RECORD_TYPE_OEM 0xc0
    uint8_t type;   /* record type */
    uint8_t length; /* remaining record bytes */
} ATTRIBUTE_PACKING;

struct entity_id {
	uint8_t	id;			/* physical entity id */
	uint8_t	instance    : 7;	/* instance number */
	uint8_t	logical     : 1;	/* physical/logical */
} ATTRIBUTE_PACKING;

struct sdr_record_mask {
	union {
		struct {
			uint16_t assert_event;	/* assertion event mask */
			uint16_t deassert_event;	/* de-assertion event mask */
			uint16_t read;	/* discrete reading mask */
		} ATTRIBUTE_PACKING discrete;
		struct {
			uint16_t assert_lnc_low:1;
			uint16_t assert_lnc_high:1;
			uint16_t assert_lcr_low:1;
			uint16_t assert_lcr_high:1;
			uint16_t assert_lnr_low:1;
			uint16_t assert_lnr_high:1;
			uint16_t assert_unc_low:1;
			uint16_t assert_unc_high:1;
			uint16_t assert_ucr_low:1;
			uint16_t assert_ucr_high:1;
			uint16_t assert_unr_low:1;
			uint16_t assert_unr_high:1;
			uint16_t status_lnc:1;
			uint16_t status_lcr:1;
			uint16_t status_lnr:1;
			uint16_t reserved:1;
			uint16_t deassert_lnc_low:1;
			uint16_t deassert_lnc_high:1;
			uint16_t deassert_lcr_low:1;
			uint16_t deassert_lcr_high:1;
			uint16_t deassert_lnr_low:1;
			uint16_t deassert_lnr_high:1;
			uint16_t deassert_unc_low:1;
			uint16_t deassert_unc_high:1;
			uint16_t deassert_ucr_low:1;
			uint16_t deassert_ucr_high:1;
			uint16_t deassert_unr_low:1;
			uint16_t deassert_unr_high:1;
			uint16_t status_unc:1;
			uint16_t status_ucr:1;
			uint16_t status_unr:1;
			uint16_t reserved_2:1;
			union {
				struct {
					uint16_t readable:8;
					uint16_t lnc:1;
					uint16_t lcr:1;
					uint16_t lnr:1;
					uint16_t unc:1;
					uint16_t ucr:1;
					uint16_t unr:1;
					uint16_t reserved:2;
				} ATTRIBUTE_PACKING set;
				struct {
					uint16_t lnc:1;
					uint16_t lcr:1;
					uint16_t lnr:1;
					uint16_t unc:1;
					uint16_t ucr:1;
					uint16_t unr:1;
					uint16_t reserved:2;
					uint16_t settable:8;
				} ATTRIBUTE_PACKING read;
			} ATTRIBUTE_PACKING;
		} ATTRIBUTE_PACKING threshold;
	} ATTRIBUTE_PACKING type;
} ATTRIBUTE_PACKING;

struct sdr_record_common_sensor {
	struct {
		uint8_t owner_id;
		uint8_t lun:2;	/* sensor owner lun */
		uint8_t ___reserved:2;
		uint8_t channel:4;	/* channel number */
		uint8_t sensor_num;	/* unique sensor number */
	} ATTRIBUTE_PACKING keys;

	struct entity_id entity;

	struct {
		struct {
			uint8_t sensor_scan:1;
			uint8_t event_gen:1;
			uint8_t type:1;
			uint8_t hysteresis:1;
			uint8_t thresholds:1;
			uint8_t events:1;
			uint8_t scanning:1;
			uint8_t ___reserved:1;
		} ATTRIBUTE_PACKING init;
		struct {
			uint8_t event_msg:2;
			uint8_t threshold:2;
			uint8_t hysteresis:2;
			uint8_t rearm:1;
			uint8_t ignore:1;
		} ATTRIBUTE_PACKING capabilities;
		uint8_t type;
	} ATTRIBUTE_PACKING sensor;

	uint8_t event_type;	/* event/reading type code */

	struct sdr_record_mask mask;

/* IPMI 2.0, Table 43-1, byte 21[7:6] Analog (numeric) Data Format */
#define SDR_UNIT_FMT_UNSIGNED 0 /* unsigned */
#define SDR_UNIT_FMT_1S_COMPL 1 /* 1's complement (signed) */
#define SDR_UNIT_FMT_2S_COMPL 2 /* 2's complement (signed) */
#define SDR_UNIT_FMT_NA 3 /* does not return analog (numeric) reading */
/* IPMI 2.0, Table 43-1, byte 21[5:3] Rate */
#define SDR_UNIT_RATE_NONE 0 /* none */
#define SDR_UNIT_RATE_MICROSEC 1 /* per us */
#define SDR_UNIT_RATE_MILLISEC 2 /* per ms */
#define SDR_UNIT_RATE_SEC 3 /* per s */
#define SDR_UNIT_RATE_MIN 4 /* per min */
#define SDR_UNIT_RATE_HR 5 /* per hour */
#define SDR_UNIT_RATE_DAY 6 /* per day */
#define SDR_UNIT_RATE_RSVD 7 /* reserved */
/* IPMI 2.0, Table 43-1, byte 21[2:1] Modifier Unit */
#define SDR_UNIT_MOD_NONE 0 /* none */
#define SDR_UNIT_MOD_DIV 1 /* Basic Unit / Modifier Unit */
#define SDR_UNIT_MOD_MUL 2 /* Basic Unit * Mofifier Unit */
#define SDR_UNIT_MOD_RSVD 3 /* Reserved */
/* IPMI 2.0, Table 43-1, byte 21[0] Percentage */
#define SDR_UNIT_PCT_NO 0
#define SDR_UNIT_PCT_YES 1

	struct {
		uint8_t pct:1;
		uint8_t modifier:2;
		uint8_t rate:3;
		uint8_t analog:2;
		struct {
			uint8_t base; /* Base unit type code per IPMI 2.0 Table 43-15 */
			uint8_t modifier; /* Modifier unit type code per Table 43-15 */
		} ATTRIBUTE_PACKING type;
	} ATTRIBUTE_PACKING unit;
} ATTRIBUTE_PACKING;

struct sdr_record_full_sensor {
	struct sdr_record_common_sensor cmn;

#define SDR_SENSOR_L_LINEAR     0x00
#define SDR_SENSOR_L_LN         0x01
#define SDR_SENSOR_L_LOG10      0x02
#define SDR_SENSOR_L_LOG2       0x03
#define SDR_SENSOR_L_E          0x04
#define SDR_SENSOR_L_EXP10      0x05
#define SDR_SENSOR_L_EXP2       0x06
#define SDR_SENSOR_L_1_X        0x07
#define SDR_SENSOR_L_SQR        0x08
#define SDR_SENSOR_L_CUBE       0x09
#define SDR_SENSOR_L_SQRT       0x0a
#define SDR_SENSOR_L_CUBERT     0x0b
#define SDR_SENSOR_L_NONLINEAR  0x70

	uint8_t linearization;	/* 70h=non linear, 71h-7Fh=non linear, OEM */
	uint16_t mtol;		/* M, tolerance */
	uint32_t bacc;		/* accuracy, B, Bexp, Rexp */

	struct {
		uint8_t nominal_read:1;	/* nominal reading field specified */
		uint8_t normal_max:1;	/* normal max field specified */
		uint8_t normal_min:1;	/* normal min field specified */
		uint8_t ___reserved:5;
	} ATTRIBUTE_PACKING analog_flag;

	uint8_t nominal_read;	/* nominal reading, raw value */
	uint8_t normal_max;	/* normal maximum, raw value */
	uint8_t normal_min;	/* normal minimum, raw value */
	uint8_t sensor_max;	/* sensor maximum, raw value */
	uint8_t sensor_min;	/* sensor minimum, raw value */

	struct {
		struct {
			uint8_t non_recover;
			uint8_t critical;
			uint8_t non_critical;
		} ATTRIBUTE_PACKING upper;
		struct {
			uint8_t non_recover;
			uint8_t critical;
			uint8_t non_critical;
		} ATTRIBUTE_PACKING lower;
		struct {
			uint8_t positive;
			uint8_t negative;
		} ATTRIBUTE_PACKING hysteresis;
	} ATTRIBUTE_PACKING threshold;
	uint8_t ___reserved[2];
	uint8_t oem;		/* reserved for OEM use */
	uint8_t id_code;	/* sensor ID string type/length code */
	uint8_t id_string[16];	/* sensor ID string bytes, only if id_code != 0 */
} ATTRIBUTE_PACKING;

struct sdr_record_compact_sensor {
	struct sdr_record_common_sensor cmn;
	struct {
		uint8_t count:4;
		uint8_t mod_type:2;
		uint8_t ___reserved:2;
		uint8_t mod_offset:7;
		uint8_t entity_inst:1;
	} ATTRIBUTE_PACKING share;

	struct {
		struct {
			uint8_t positive;
			uint8_t negative;
		} ATTRIBUTE_PACKING hysteresis;
	} ATTRIBUTE_PACKING threshold;

	uint8_t ___reserved[3];
	uint8_t oem;		/* reserved for OEM use */
	uint8_t id_code;	/* sensor ID string type/length code */
	uint8_t id_string[16];	/* sensor ID string bytes, only if id_code != 0 */
} ATTRIBUTE_PACKING;

#define tos32(val, bits)    ((val & ((1<<((bits)-1)))) ? (-((val) & (1<<((bits)-1))) | (val)) : (val))
# define BSWAP_16(x) ((((x) & 0xff00) >> 8) | (((x) & 0x00ff) << 8))
# define BSWAP_32(x) ((((x) & 0xff000000) >> 24) | (((x) & 0x00ff0000) >> 8) |\
                     (((x) & 0x0000ff00) << 8) | (((x) & 0x000000ff) << 24))
# define __TO_M(mtol)       (int16_t)(tos32((((BSWAP_16(mtol) & 0xff00) >> 8) | ((BSWAP_16(mtol) & 0xc0) << 2)), 10))
# define __TO_B(bacc)       (int32_t)(tos32((((BSWAP_32(bacc) & 0xff000000) >> 24) | \
                            ((BSWAP_32(bacc) & 0xc00000) >> 14)), 10))
# define __TO_R_EXP(bacc)   (int32_t)(tos32(((BSWAP_32(bacc) & 0xf0) >> 4), 4))
# define __TO_B_EXP(bacc)   (int32_t)(tos32((BSWAP_32(bacc) & 0xf), 4))

struct sensor_reading {
	char		s_id[17];		/* name of the sensor */
	struct sdr_record_full_sensor    *full;
	struct sdr_record_compact_sensor *compact;
	uint8_t		s_reading_valid;	/* read value valididity */
	uint8_t		s_scanning_disabled;	/* read of value disabled */
	uint8_t		s_reading_unavailable;	/* read value unavailable */
	uint8_t		s_reading;		/* value which was read */
	uint8_t		s_data2;		/* data2 value read */
	uint8_t		s_data3;		/* data3 value read */
	uint8_t		s_has_analog_value;	/* sensor has analog value */
	double		s_a_val;		/* read value converted to analog */
	char		s_a_str[16];		/* analog value as a string */
	const char	*s_a_units;		/* analog value units string */
};

#define IPMI_BUF_SIZE 1024

struct ipmi_buf {
	uint8_t ccode;
	uint8_t data[IPMI_BUF_SIZE];
	int data_len;
};

double sdr_convert_sensor_reading(struct sdr_record_full_sensor *sensor, uint8_t val);

bool validate_sdr_record(char buf[], int count);

#define READING_UNAVAILABLE	0x20
#define SCANNING_DISABLED	0x40

#define IS_READING_UNAVAILABLE(val)	((val) & READING_UNAVAILABLE)
#define IS_SCANNING_DISABLED(val)	(!((val) & SCANNING_DISABLED))

struct sensor_reading *
ipmi_sdr_read_sensor_value(struct sdr_record_common_sensor *sensor,
                           uint8_t sdr_record_type, 
                           int precision,
                           struct ipmi_buf *sensor_reading_buf);

#define IS_THRESHOLD_SENSOR(s)	((s)->event_type == 1)
#define UNITS_ARE_DISCRETE(s)	((s)->unit.analog == 3)


#define SDR_SENSOR_STAT_LO_NC	(1<<0)
#define SDR_SENSOR_STAT_LO_CR	(1<<1)
#define SDR_SENSOR_STAT_LO_NR	(1<<2)
#define SDR_SENSOR_STAT_HI_NC	(1<<3)
#define SDR_SENSOR_STAT_HI_CR	(1<<4)
#define SDR_SENSOR_STAT_HI_NR	(1<<5)

const char *ipmi_sdr_get_thresh_status(struct sensor_reading *sr, const char *invalidstr);

void dump_sensor_fc_thredshold(const char *thresh_status, struct sensor_reading *sr);

void dump_sensor_fc_discrete(struct sensor_reading *sr);

#pragma pack(pop)