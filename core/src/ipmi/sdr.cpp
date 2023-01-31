/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file sdr.cpp
 */

#include "sdr.h"
#include <cstdio>

static int sdr_sensor_has_analog_reading(struct sensor_reading *sr);

#define UNIT_TYPE_MAX 92 /* This is the ID of "grams" */
#define UNIT_TYPE_LONGEST_NAME 19 /* This is the length of "color temp deg K" */
static const char *unit_desc[] = {
	"unspecified",
	"degrees C",
	"degrees F",
	"degrees K",
	"Volts",
	"Amps",
	"Watts",
	"Joules",
	"Coulombs",
	"VA",
	"Nits",
	"lumen",
	"lux",
	"Candela",
	"kPa",
	"PSI",
	"Newton",
	"CFM",
	"RPM",
	"Hz",
	"microsecond",
	"millisecond",
	"second",
	"minute",
	"hour",
	"day",
	"week",
	"mil",
	"inches",
	"feet",
	"cu in",
	"cu feet",
	"mm",
	"cm",
	"m",
	"cu cm",
	"cu m",
	"liters",
	"fluid ounce",
	"radians",
	"steradians",
	"revolutions",
	"cycles",
	"gravities",
	"ounce",
	"pound",
	"ft-lb",
	"oz-in",
	"gauss",
	"gilberts",
	"henry",
	"millihenry",
	"farad",
	"microfarad",
	"ohms",
	"siemens",
	"mole",
	"becquerel",
	"PPM",
	"reserved",
	"Decibels",
	"DbA",
	"DbC",
	"gray",
	"sievert",
	"color temp deg K",
	"bit",
	"kilobit",
	"megabit",
	"gigabit",
	"byte",
	"kilobyte",
	"megabyte",
	"gigabyte",
	"word",
	"dword",
	"qword",
	"line",
	"hit",
	"miss",
	"retry",
	"reset",
	"overflow",
	"underrun",
	"collision",
	"packets",
	"messages",
	"characters",
	"error",
	"correctable error",
	"uncorrectable error",
	"fatal error",
	"grams"
};

const char *
ipmi_sdr_get_unit_string(bool pct, uint8_t relation,
                         uint8_t base, uint8_t modifier)
{
	/*
	 * Twice as long as the longest possible unit name, plus
	 * two characters for '%' and relation (either '*' or '/'),
	 * plus the terminating null-byte.
	 */
	static char unitstr[2 * UNIT_TYPE_LONGEST_NAME + 2 + 1];

	/*
	 * By default, if units are supposed to be percent, we will pre-pend
	 * the percent string  to the textual representation of the units.
	 */
	const char *pctstr = pct ? "% " : "";
	const char *basestr;
	const char *modstr;

	if (base <= UNIT_TYPE_MAX) {
		basestr = unit_desc[base];
	}
	else {
		basestr = "invalid";
	}

	if (modifier <= UNIT_TYPE_MAX) {
		modstr = unit_desc[modifier];
	}
	else {
		modstr = "invalid";
	}

	switch (relation) {
	case SDR_UNIT_MOD_MUL:
		snprintf(unitstr, sizeof (unitstr), "%s%s*%s",
			 pctstr, basestr, modstr);
		break;
	case SDR_UNIT_MOD_DIV:
		snprintf(unitstr, sizeof (unitstr), "%s%s/%s",
			 pctstr, basestr, modstr);
		break;
	case SDR_UNIT_MOD_NONE:
	default:
		/*
		 * Display the text "percent" only when the Base unit is
		 * "unspecified" and the caller specified to print percent.
		 */
		if (base == 0 && pct) {
			snprintf(unitstr, sizeof(unitstr), "percent");
		} else {
			snprintf(unitstr, sizeof (unitstr), "%s%s",
			         pctstr, basestr);
		}
		break;
	}
	return unitstr;
}

double sdr_convert_sensor_reading(struct sdr_record_full_sensor *sensor, uint8_t val)
{
    int m, b, k1, k2;
    double result;

    m = __TO_M(sensor->mtol);
    b = __TO_B(sensor->bacc);
    k1 = __TO_B_EXP(sensor->bacc);
    k2 = __TO_R_EXP(sensor->bacc);

    switch (sensor->cmn.unit.analog)
    {
    case 0:
        result = (double)(((m * val) +
                           (b * pow(10, k1))) *
                          pow(10, k2));
        break;
    case 1:
        if (val & 0x80)
            val++;
        /* fall through */
    case 2:
        result = (double)(((m * (int8_t)val) +
                           (b * pow(10, k1))) *
                          pow(10, k2));
        break;
    default:
        /* Oops! This isn't an analog sensor. */
        return 0.0;
    }

    switch (sensor->linearization & 0x7f)
    {
    case SDR_SENSOR_L_LN:
        result = log(result);
        break;
    case SDR_SENSOR_L_LOG10:
        result = log10(result);
        break;
    case SDR_SENSOR_L_LOG2:
        result = (double)(log(result) / log(2.0));
        break;
    case SDR_SENSOR_L_E:
        result = exp(result);
        break;
    case SDR_SENSOR_L_EXP10:
        result = pow(10.0, result);
        break;
    case SDR_SENSOR_L_EXP2:
        result = pow(2.0, result);
        break;
    case SDR_SENSOR_L_1_X:
        result = pow(result, -1.0); /*1/x w/o exception */
        break;
    case SDR_SENSOR_L_SQR:
        result = pow(result, 2.0);
        break;
    case SDR_SENSOR_L_CUBE:
        result = pow(result, 3.0);
        break;
    case SDR_SENSOR_L_SQRT:
        result = sqrt(result);
        break;
    case SDR_SENSOR_L_CUBERT:
        result = cbrt(result);
        break;
    case SDR_SENSOR_L_LINEAR:
    default:
        break;
    }
    return result;
}

struct sensor_reading *
ipmi_sdr_read_sensor_value(struct sdr_record_common_sensor *sensor,
                           uint8_t sdr_record_type, 
                           int precision,
                           struct ipmi_buf *sensor_reading_buf)
{
    static struct sensor_reading sr;

    if (!sensor)
        return NULL;

    /* Initialize to reading valid value of zero */
    memset(&sr, 0, sizeof(sr));

    switch (sdr_record_type)
    {
        unsigned int idlen;
    case (SDR_RECORD_TYPE_FULL_SENSOR):
        sr.full = (struct sdr_record_full_sensor *)sensor;
        idlen = sr.full->id_code & 0x1f;
        idlen = idlen < sizeof(sr.s_id) ? idlen : sizeof(sr.s_id) - 1;
        memcpy(sr.s_id, sr.full->id_string, idlen);
        break;
    case SDR_RECORD_TYPE_COMPACT_SENSOR:
        sr.compact = (struct sdr_record_compact_sensor *)sensor;
        idlen = sr.compact->id_code & 0x1f;
        idlen = idlen < sizeof(sr.s_id) ? idlen : sizeof(sr.s_id) - 1;
        memcpy(sr.s_id, sr.compact->id_string, idlen);
        break;
    default:
        return NULL;
    }

    sr.s_a_val = 0.0;     /* init analog value to a floating point 0 */
    sr.s_a_str[0] = '\0'; /* no converted analog value string */
    sr.s_a_units = "";    /* no converted analog units units */

    if (!sensor_reading_buf)
    {
        return &sr;
    }

    if (sensor_reading_buf->ccode)
    {
        return &sr;
    }

    if (sensor_reading_buf->data_len < 2)
    {
        /*
         * We must be returned both a value (data[0]), and the validity
         * of the value (data[1]), in order to correctly interpret
         * the reading.    If we don't have both of these we can't have
         * a valid sensor reading.
         */
        return &sr;
    }

    if (IS_READING_UNAVAILABLE(sensor_reading_buf->data[1]))
        sr.s_reading_unavailable = 1;

    if (IS_SCANNING_DISABLED(sensor_reading_buf->data[1]))
    {
        sr.s_scanning_disabled = 1;
        return &sr;
    }
    if (!sr.s_reading_unavailable)
    {
        sr.s_reading_valid = 1;
        sr.s_reading = sensor_reading_buf->data[0];
    }
    if (sensor_reading_buf->data_len > 2)
        sr.s_data2 = sensor_reading_buf->data[2];
    if (sensor_reading_buf->data_len > 3)
        sr.s_data3 = sensor_reading_buf->data[3];
    if (sdr_sensor_has_analog_reading(&sr)) {
		sr.s_has_analog_value = 1;
		if (sr.s_reading_valid) {
			sr.s_a_val = sdr_convert_sensor_reading(sr.full, sr.s_reading);
		}
		/* determine units string with possible modifiers */
		sr.s_a_units = ipmi_sdr_get_unit_string(sr.full->cmn.unit.pct,
					   sr.full->cmn.unit.modifier,
					   sr.full->cmn.unit.type.base,
					   sr.full->cmn.unit.type.modifier);
		snprintf(sr.s_a_str, sizeof(sr.s_a_str), "%.*f",
			(sr.s_a_val == (int) sr.s_a_val) ? 0 :
			precision, sr.s_a_val);
	}        
    return &sr;
}

static int sdr_sensor_has_analog_reading(struct sensor_reading *sr)
{
    /* Compact sensors can't return analog values so we false */
    if (!sr->full)
    {
        return 0;
    }
    if (UNITS_ARE_DISCRETE(&sr->full->cmn))
    {
        return 0; /* Sensor specified as not having Analog Units */
    }
    /*
    if (!IS_THRESHOLD_SENSOR(&sr->full->cmn))
    {
        return 0;
    }
    */
    /*
     * If sensor has linearization, then we should be able to update the
     * reading factors and if we cannot fail the conversion.
     */
    if (sr->full->linearization >= SDR_SENSOR_L_NONLINEAR &&
        sr->full->linearization <= 0x7F)
    {
        sr->s_reading_valid = 0;
        return 0;
    }

    return 1;
}

const char *
ipmi_sdr_get_thresh_status(struct sensor_reading *sr, const char *invalidstr)
{
    uint8_t stat;
    if (!sr->s_reading_valid)
    {
        return invalidstr;
    }
    stat = sr->s_data2;
    if (stat & SDR_SENSOR_STAT_LO_NR)
    {
        return "Lower Non-Recoverable";
    }
    else if (stat & SDR_SENSOR_STAT_HI_NR)
    {
        return "Upper Non-Recoverable";
    }
    else if (stat & SDR_SENSOR_STAT_LO_CR)
    {
        return "Lower Critical";
    }
    else if (stat & SDR_SENSOR_STAT_HI_CR)
    {
        return "Upper Critical";
    }
    else if (stat & SDR_SENSOR_STAT_LO_NC)
    {
        return "Lower Non-Critical";
    }
    else if (stat & SDR_SENSOR_STAT_HI_NC)
    {
        return "Upper Non-Critical";
    }
    return "ok";
}

void dump_sensor_fc_thredshold(
    const char *thresh_status,
    struct sensor_reading *sr)
{
    printf("%-16s ", sr->s_id);
    if (sr->s_reading_valid)
    {
        if (sr->s_has_analog_value)
            printf("| %-10.3f | %-10s | %-6s",
                   sr->s_a_val, sr->s_a_units, thresh_status);
        else
            printf("| 0x%-8x | %-10s | %-6s",
                   sr->s_reading, sr->s_a_units, thresh_status);
    }
    else
    {
        printf("| %-10s | %-10s | %-6s",
               "na", sr->s_a_units, "na");
    }
    printf("| %-10s| %-10s| %-10s| %-10s| %-10s| %-10s",
           "na", "na", "na", "na", "na", "na");

    printf("\n");
}

void dump_sensor_fc_discrete(struct sensor_reading *sr)
{
    printf("%-16s ", sr->s_id);
    if (sr->s_reading_valid)
    {
        if (sr->s_has_analog_value)
        {
            /* don't show discrete component */
            printf("| %-10s | %-10s | %-6s",
                   sr->s_a_str, sr->s_a_units, "ok");
        }
        else
        {
            printf("| 0x%-8x | %-10s | 0x%02x%02x",
                   sr->s_reading, "discrete",
                   sr->s_data2, sr->s_data3);
        }
    }
    else
    {
        printf("| %-10s | %-10s | %-6s",
               "na", "discrete", "na");
    }
    printf("| %-10s| %-10s| %-10s| %-10s| %-10s| %-10s",
           "na", "na", "na", "na", "na", "na");

    printf("\n");
}
