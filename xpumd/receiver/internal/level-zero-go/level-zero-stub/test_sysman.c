/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "zes_stub.h"
#include "../level-zero/zes_api.h"

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include <time.h>

// ------------------------------------------------------------------
// Minimal test harness
// ------------------------------------------------------------------

static int g_pass = 0;
static int g_fail = 0;

#define ASSERT(label, cond)                                                                                            \
	if (cond) {                                                                                                        \
		printf("  PASS  %s\n", label);                                                                                 \
		g_pass++;                                                                                                      \
	} else {                                                                                                           \
		printf("  FAIL  %s  (line %d)\n", label, __LINE__);                                                            \
		g_fail++;                                                                                                      \
	}

#define ASSERT_ZE_OK(label, result) ASSERT(label, (result) == ZE_RESULT_SUCCESS)
#define ASSERT_ZE_RET(label, result, expected) ASSERT(label, (result) == (expected))

// ------------------------------------------------------------------
// Test data paths (relative to the level-zero-stub directory where
// the test binary is executed from).
// ------------------------------------------------------------------

#define YAML_TWO_ENGINES "testdata/two_engines.yaml"
#define YAML_ONE_ENGINE "testdata/one_device.yaml"
#define YAML_NO_DEVICES "testdata/no_devices.yaml"
#define YAML_NO_DRIVERS "testdata/no_drivers.yaml"
#define YAML_ALL_COMPONENTS "testdata/all_components.yaml"
#define YAML_INVALID_UUID "testdata/invalid_uuid.yaml"

// ------------------------------------------------------------------
// Test cases
// ------------------------------------------------------------------

static void test_driver_handle_persists(void)
{
	printf("test_driver_handle_persists\n");

	const char *path = YAML_TWO_ENGINES;

	sysman_state_reset();
	sysman_state_load(path);
	uint32_t count = 0;
	zesDriverGet(&count, NULL);
	ze_driver_handle_t *drvs = calloc(count, sizeof(*drvs));
	zesDriverGet(&count, drvs);
	ze_driver_handle_t drv = drvs[0];
	free(drvs);

	// Reload same config — driver[0] still exists.
	sysman_state_reset();
	sysman_state_load(path);
	uint32_t ext_count = 0;
	ASSERT_ZE_OK("driver handle valid after same-config reload",
				 zesDriverGetExtensionProperties(drv, &ext_count, NULL));
}

static void test_device_handle_persists(void)
{
	printf("test_device_handle_persists\n");

	const char *path = YAML_TWO_ENGINES;

	sysman_state_reset();
	sysman_state_load(path);
	uint32_t dc = 0;
	zesDriverGet(&dc, NULL);
	ze_driver_handle_t *drvs = calloc(dc, sizeof(*drvs));
	zesDriverGet(&dc, drvs);
	uint32_t devc = 0;
	zesDeviceGet(drvs[0], &devc, NULL);
	zes_device_handle_t *devs = calloc(devc, sizeof(*devs));
	zesDeviceGet(drvs[0], &devc, devs);
	zes_device_handle_t dev = devs[0];
	free(drvs);
	free(devs);

	// Reload same config — device[0] still exists.
	sysman_state_reset();
	sysman_state_load(path);
	zes_device_properties_t properties = {.stype = ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES};
	ASSERT_ZE_OK("device handle valid after same-config reload", zesDeviceGetProperties(dev, &properties));
}

static void test_engine_handle_persists(void)
{
	printf("test_engine_handle_persists\n");

	const char *path = YAML_TWO_ENGINES;

	sysman_state_reset();
	sysman_state_load(path);
	uint32_t dc = 0;
	zesDriverGet(&dc, NULL);
	ze_driver_handle_t *drvs = calloc(dc, sizeof(*drvs));
	zesDriverGet(&dc, drvs);
	uint32_t devc = 0;
	zesDeviceGet(drvs[0], &devc, NULL);
	zes_device_handle_t *devs = calloc(devc, sizeof(*devs));
	zesDeviceGet(drvs[0], &devc, devs);
	uint32_t ec = 0;
	zesDeviceEnumEngineGroups(devs[0], &ec, NULL);
	zes_engine_handle_t *engs = calloc(ec, sizeof(*engs));
	zesDeviceEnumEngineGroups(devs[0], &ec, engs);
	zes_engine_handle_t eng0 = engs[0];
	free(drvs);
	free(devs);
	free(engs);

	// Reload same config — engine[0] still exists.
	sysman_state_reset();
	sysman_state_load(path);
	zes_engine_properties_t eproperties = {.stype = ZES_STRUCTURE_TYPE_ENGINE_PROPERTIES};
	ASSERT_ZE_OK("engine[0] handle valid after same-config reload", zesEngineGetProperties(eng0, &eproperties));
}

static void test_engine_handle_invalidated_when_slot_removed(void)
{
	printf("test_engine_handle_invalidated_when_slot_removed\n");

	const char *path2 = YAML_TWO_ENGINES;
	const char *path1 = YAML_ONE_ENGINE;

	sysman_state_reset();
	sysman_state_load(path2);
	uint32_t dc = 0;
	zesDriverGet(&dc, NULL);
	ze_driver_handle_t *drvs = calloc(dc, sizeof(*drvs));
	zesDriverGet(&dc, drvs);
	uint32_t devc = 0;
	zesDeviceGet(drvs[0], &devc, NULL);
	zes_device_handle_t *devs = calloc(devc, sizeof(*devs));
	zesDeviceGet(drvs[0], &devc, devs);
	uint32_t ec = 0;
	zesDeviceEnumEngineGroups(devs[0], &ec, NULL);
	zes_engine_handle_t *engs = calloc(ec, sizeof(*engs));
	zesDeviceEnumEngineGroups(devs[0], &ec, engs);
	zes_engine_handle_t eng0 = engs[0];
	zes_engine_handle_t eng1 = engs[1]; // will be removed
	free(drvs);
	free(devs);
	free(engs);

	// Reload config with only one engine — engine[1] slot no longer exists.
	sysman_state_reset();
	sysman_state_load(path1);
	zes_engine_properties_t eproperties = {.stype = ZES_STRUCTURE_TYPE_ENGINE_PROPERTIES};

	ASSERT_ZE_OK("engine[0] handle still valid after shrink", zesEngineGetProperties(eng0, &eproperties));
	ASSERT_ZE_RET("engine[1] handle invalid after slot removed", zesEngineGetProperties(eng1, &eproperties),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
}

static void test_device_handle_invalidated_when_slot_removed(void)
{
	printf("test_device_handle_invalidated_when_slot_removed\n");

	const char *path_dev = YAML_TWO_ENGINES;
	const char *path_none = YAML_NO_DEVICES;

	sysman_state_reset();
	sysman_state_load(path_dev);
	uint32_t dc = 0;
	zesDriverGet(&dc, NULL);
	ze_driver_handle_t *drvs = calloc(dc, sizeof(*drvs));
	zesDriverGet(&dc, drvs);
	uint32_t devc = 0;
	zesDeviceGet(drvs[0], &devc, NULL);
	zes_device_handle_t *devs = calloc(devc, sizeof(*devs));
	zesDeviceGet(drvs[0], &devc, devs);
	zes_device_handle_t dev = devs[0];
	free(drvs);
	free(devs);

	// Reload config with no devices — device[0] slot gone.
	sysman_state_reset();
	sysman_state_load(path_none);
	zes_device_properties_t properties = {.stype = ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES};
	ASSERT_ZE_RET("device handle invalid after slot removed", zesDeviceGetProperties(dev, &properties),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
}

static void test_driver_handle_invalidated_when_slot_removed(void)
{
	printf("test_driver_handle_invalidated_when_slot_removed\n");

	const char *path_drv = YAML_TWO_ENGINES;
	const char *path_none = YAML_NO_DRIVERS;

	sysman_state_reset();
	sysman_state_load(path_drv);
	uint32_t dc = 0;
	zesDriverGet(&dc, NULL);
	ze_driver_handle_t *drvs = calloc(dc, sizeof(*drvs));
	zesDriverGet(&dc, drvs);
	ze_driver_handle_t drv = drvs[0];
	free(drvs);

	// Reload config with no drivers — driver[0] slot gone.
	sysman_state_reset();
	sysman_state_load(path_none);
	uint32_t ext_count = 0;
	ASSERT_ZE_RET("driver handle invalid after slot removed", zesDriverGetExtensionProperties(drv, &ext_count, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
}

static void test_wrong_handle_type_rejected(void)
{
	printf("test_wrong_handle_type_rejected\n");

	const char *path = YAML_TWO_ENGINES;
	sysman_state_reset();
	sysman_state_load(path);

	uint32_t dc = 0;
	zesDriverGet(&dc, NULL);
	ze_driver_handle_t *drvs = calloc(dc, sizeof(*drvs));
	zesDriverGet(&dc, drvs);
	uint32_t devc = 0;
	zesDeviceGet(drvs[0], &devc, NULL);
	zes_device_handle_t *devs = calloc(devc, sizeof(*devs));
	zesDeviceGet(drvs[0], &devc, devs);
	uint32_t ec = 0;
	zesDeviceEnumEngineGroups(devs[0], &ec, NULL);
	zes_engine_handle_t *engs = calloc(ec, sizeof(*engs));
	zesDeviceEnumEngineGroups(devs[0], &ec, engs);

	// Pass device handle where engine handle is expected.
	zes_engine_properties_t eproperties = {.stype = ZES_STRUCTURE_TYPE_ENGINE_PROPERTIES};
	ASSERT_ZE_RET("device handle rejected as engine handle",
				  zesEngineGetProperties((zes_engine_handle_t)devs[0], &eproperties),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);

	// Pass engine handle where device handle is expected.
	zes_device_properties_t dproperties = {.stype = ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES};
	ASSERT_ZE_RET("engine handle rejected as device handle",
				  zesDeviceGetProperties((zes_device_handle_t)engs[0], &dproperties),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);

	free(drvs);
	free(devs);
	free(engs);
}

static void test_state_load_without_config_clears_state(void)
{
	printf("test_state_load_without_config_clears_state\n");

	unsetenv("SYSMAN_STUB_CONFIG");

	sysman_state_reset();
	ASSERT("reload fixture before clearing", sysman_state_load(YAML_TWO_ENGINES) == 0);

	uint32_t count = 0;
	zesDriverGet(&count, NULL);
	ASSERT("fixture starts with one driver", count == 1);

	ASSERT("state_load without path succeeds", sysman_state_load(NULL) == 0);
	char *config_path = sysman_get_config_path();
	ASSERT("config path cleared", config_path && config_path[0] == '\0');
	free(config_path);

	count = 0;
	ASSERT_ZE_OK("driver enumeration succeeds on empty state", zesDriverGet(&count, NULL));
	ASSERT("driver count cleared when no config is set", count == 0);
}

// ------------------------------------------------------------------
// Auto-reload test (config file watcher)
// ------------------------------------------------------------------

// Copy src file to dst atomically (write to tmp, rename into place).
static int copy_file_atomic(const char *src, const char *dst)
{
	// Build a temp path in the same directory as dst.
	char tmp[PATH_MAX];
	char dst_copy[PATH_MAX];
	snprintf(dst_copy, sizeof(dst_copy), "%s", dst);
	snprintf(tmp, sizeof(tmp), "%s/.stub_tmp_XXXXXX", dirname(dst_copy));

	int fd_tmp = mkstemp(tmp);
	if (fd_tmp < 0) {
		perror("mkstemp");
		return -1;
	}

	FILE *fsrc = fopen(src, "r");
	if (!fsrc) {
		perror("fopen src");
		goto error_cleanup_2;
	}

	char buf[4096];
	size_t n;
	while ((n = fread(buf, 1, sizeof(buf), fsrc)) > 0) {
		size_t written = 0;
		while (written < n) {
			ssize_t rc = write(fd_tmp, buf + written, n - written);
			if (rc < 0) {
				perror("write");
				goto error_cleanup;
			}
			if (rc == 0) {
				fprintf(stderr, "write: short write\n");
				goto error_cleanup;
			}
			written += (size_t)rc;
		}
	}
	if (ferror(fsrc)) {
		perror("fread");
		goto error_cleanup;
	}
	fclose(fsrc);
	if (close(fd_tmp) != 0) {
		perror("close");
		unlink(tmp);
		return -1;
	}

	if (rename(tmp, dst) != 0) {
		perror("rename");
		unlink(tmp);
		return -1;
	}
	return 0;

error_cleanup:
	fclose(fsrc);
error_cleanup_2:
	close(fd_tmp);
	unlink(tmp);
	return -1;
}

static int write_text_file(const char *path, const char *text)
{
	FILE *f = fopen(path, "w");
	if (!f) {
		perror("fopen");
		return -1;
	}
	if (fputs(text, f) == EOF) {
		perror("fputs");
		fclose(f);
		return -1;
	}
	if (fclose(f) != 0) {
		perror("fclose");
		return -1;
	}
	return 0;
}

static void test_auto_reload(void)
{
	printf("test_auto_reload\n");

	// 1. Create a mutable temp file in /tmp (inotify watches its parent dir).
	char tmp_path[] = "/tmp/stub_watch_test_XXXXXX";
	int fd = mkstemp(tmp_path);
	if (fd < 0) {
		perror("mkstemp");
		ASSERT("auto-reload setup", false);
		return;
	}
	close(fd);

	// 2. Copy two_engines into temp file and load it.
	if (copy_file_atomic(YAML_TWO_ENGINES, tmp_path) != 0) {
		ASSERT("copy initial config", false);
		unlink(tmp_path);
		return;
	}
	sysman_state_reset();
	if (sysman_state_load(tmp_path) != 0) {
		ASSERT("load initial config", false);
		unlink(tmp_path);
		return;
	}

	// Verify initial state: 2 engines on device[0] of driver[0].
	{
		uint32_t dc = 0;
		zesDriverGet(&dc, NULL);
		ze_driver_handle_t drvs[8];
		zesDriverGet(&dc, drvs);
		uint32_t devc = 0;
		zesDeviceGet(drvs[0], &devc, NULL);
		zes_device_handle_t devs[8];
		zesDeviceGet(drvs[0], &devc, devs);
		uint32_t ec = 0;
		zesDeviceEnumEngineGroups(devs[0], &ec, NULL);
		ASSERT("initial engine count is 2", ec == 2);
	}

	// 3. Start watcher.
	if (sysman_watch_start() != 0) {
		ASSERT("watcher started", false);
		unlink(tmp_path);
		return;
	}

	// 4. Atomically replace tmp_path with test data content.
	if (copy_file_atomic(YAML_ONE_ENGINE, tmp_path) != 0) {
		ASSERT("copy updated config", false);
		sysman_watch_stop();
		unlink(tmp_path);
		return;
	}

	// 5. Poll up to 500 ms for state to reflect one engine.
	bool reloaded = false;
	for (int i = 0; i < 10 && !reloaded; i++) {
		struct timespec ts = {.tv_sec = 0, .tv_nsec = 50 * 1000 * 1000};
		nanosleep(&ts, NULL);
		uint32_t dc = 0;
		zesDriverGet(&dc, NULL);
		ze_driver_handle_t drvs[8];
		zesDriverGet(&dc, drvs);
		uint32_t devc = 0;
		zesDeviceGet(drvs[0], &devc, NULL);
		zes_device_handle_t devs[8];
		zesDeviceGet(drvs[0], &devc, devs);
		uint32_t ec = 0;
		zesDeviceEnumEngineGroups(devs[0], &ec, NULL);
		if (ec == 1)
			reloaded = true;
	}

	// 6. Stop watcher.
	sysman_watch_stop();

	// 7. Assert.
	ASSERT("config reloaded automatically on file change", reloaded);

	unlink(tmp_path);
}

static void test_array_null_vs_empty(void)
{
	printf("test_array_null_vs_empty\n");

	char tmp_path[] = "/tmp/stub_array_test_XXXXXX";
	int fd = mkstemp(tmp_path);
	if (fd < 0) {
		perror("mkstemp");
		ASSERT("array null-vs-empty setup", false);
		return;
	}
	close(fd);

	const char *engine_empty = "Drivers:\n"
							   "  - Devices:\n"
							   "      - Properties: {}\n"
							   "        EngineGroups: []\n";
	const char *engine_null = "Drivers:\n"
							  "  - Devices:\n"
							  "      - Properties: {}\n"
							  "        EngineGroups: null\n";
	const char *clocks_empty = "Drivers:\n"
							   "  - Devices:\n"
							   "      - Properties: {}\n"
							   "        FrequencyDomains:\n"
							   "          - Properties: {}\n"
							   "            AvailableClocks: []\n";
	const char *clocks_null = "Drivers:\n"
							  "  - Devices:\n"
							  "      - Properties: {}\n"
							  "        FrequencyDomains:\n"
							  "          - Properties: {}\n"
							  "            AvailableClocks: null\n";

	ASSERT("write engine-groups empty config", write_text_file(tmp_path, engine_empty) == 0);
	sysman_state_reset();
	ASSERT("reload engine-groups empty config", sysman_state_load(tmp_path) == 0);
	{
		uint32_t driver_count = 1;
		ze_driver_handle_t driver = NULL;
		zesDriverGet(&driver_count, &driver);
		uint32_t device_count = 1;
		zes_device_handle_t device = NULL;
		zesDeviceGet(driver, &device_count, &device);

		uint32_t count = 0;
		ze_result_t result = zesDeviceEnumEngineGroups(device, &count, NULL);
		ASSERT_ZE_OK("empty engine groups enumerate successfully", result);
		ASSERT("empty engine groups report zero entries", count == 0);
	}

	ASSERT("write engine-groups null config", write_text_file(tmp_path, engine_null) == 0);
	sysman_state_reset();
	ASSERT("reload engine-groups null config", sysman_state_load(tmp_path) == 0);
	{
		uint32_t driver_count = 1;
		ze_driver_handle_t driver = NULL;
		zesDriverGet(&driver_count, &driver);
		uint32_t device_count = 1;
		zes_device_handle_t device = NULL;
		zesDeviceGet(driver, &device_count, &device);

		uint32_t count = 0;
		ze_result_t result = zesDeviceEnumEngineGroups(device, &count, NULL);
		ASSERT_ZE_OK("null engine groups return success", result);
		ASSERT("null engine groups report zero entries", count == 0);
	}

	ASSERT("write available-clocks empty config", write_text_file(tmp_path, clocks_empty) == 0);
	sysman_state_reset();
	ASSERT("reload available-clocks empty config", sysman_state_load(tmp_path) == 0);
	{
		uint32_t driver_count = 1;
		ze_driver_handle_t driver = NULL;
		zesDriverGet(&driver_count, &driver);
		uint32_t device_count = 1;
		zes_device_handle_t device = NULL;
		zesDeviceGet(driver, &device_count, &device);
		uint32_t freq_count = 0;
		ze_result_t result = zesDeviceEnumFrequencyDomains(device, &freq_count, NULL);
		ASSERT_ZE_OK("frequency domains enumerate successfully", result);
		ASSERT("one frequency domain present", freq_count == 1);

		zes_freq_handle_t freq = NULL;
		freq_count = 1;
		zesDeviceEnumFrequencyDomains(device, &freq_count, &freq);
		uint32_t clock_count = 0;
		result = zesFrequencyGetAvailableClocks(freq, &clock_count, NULL);
		ASSERT_ZE_OK("empty available clocks succeed", result);
		ASSERT("empty available clocks report zero entries", clock_count == 0);
	}

	ASSERT("write available-clocks null config", write_text_file(tmp_path, clocks_null) == 0);
	sysman_state_reset();
	ASSERT("reload available-clocks null config", sysman_state_load(tmp_path) == 0);
	{
		uint32_t driver_count = 1;
		ze_driver_handle_t driver = NULL;
		zesDriverGet(&driver_count, &driver);
		uint32_t device_count = 1;
		zes_device_handle_t device = NULL;
		zesDeviceGet(driver, &device_count, &device);
		uint32_t freq_count = 1;
		zes_freq_handle_t freq = NULL;
		zesDeviceEnumFrequencyDomains(device, &freq_count, &freq);
		uint32_t clock_count = 0;
		ze_result_t result = zesFrequencyGetAvailableClocks(freq, &clock_count, NULL);
		ASSERT_ZE_OK("null available clocks return success", result);
		ASSERT("null available clocks report zero entries", clock_count == 0);
	}

	unlink(tmp_path);
}

// ------------------------------------------------------------------
// Error cases
// ------------------------------------------------------------------

#define NULL_HANDLE ((void *)0)

static void test_error_cases(void)
{
	printf("test_error_cases\n");

	sysman_state_reset();
	ASSERT("sysman_state_load", sysman_state_load(YAML_ALL_COMPONENTS) == 0);

	// Setup: get valid handles for use in NULL-pointer tests
	uint32_t drv_n = 1;
	ze_driver_handle_t drv = NULL;
	ASSERT_ZE_OK("zesDriverGet", zesDriverGet(&drv_n, &drv));

	uint32_t dev_n = 1;
	zes_device_handle_t dev = NULL;
	ASSERT_ZE_OK("zesDeviceGet", zesDeviceGet(drv, &dev_n, &dev));

	uint32_t eng_n = 1;
	zes_engine_handle_t eng0 = NULL;
	ASSERT_ZE_OK("zesDeviceEnumEngineGroups", zesDeviceEnumEngineGroups(dev, &eng_n, &eng0));

	ze_driver_handle_t bad_drv = NULL;
	zes_device_handle_t bad_dev = NULL;
	zes_engine_handle_t bad_eng = NULL;

	// zesDriverGet
	ASSERT_ZE_RET("zesDriverGet: NULL pCount", zesDriverGet(NULL, NULL), ZE_RESULT_ERROR_INVALID_NULL_POINTER);

	// zesDriverGetExtensionProperties
	ASSERT_ZE_RET("zesDriverGetExtensionProperties: NULL handle", zesDriverGetExtensionProperties(bad_drv, NULL, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesDriverGetExtensionProperties: NULL pCount with array",
				  zesDriverGetExtensionProperties(drv, NULL, (zes_driver_extension_properties_t *)(uintptr_t)1),
				  ZE_RESULT_ERROR_INVALID_NULL_POINTER);

	// zesDeviceGet
	ASSERT_ZE_RET("zesDeviceGet: NULL handle", zesDeviceGet(bad_drv, NULL, NULL), ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	{
		zes_device_handle_t d = NULL;
		ASSERT_ZE_RET("zesDeviceGet: NULL pCount with array", zesDeviceGet(drv, NULL, &d),
					  ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	}

	// zesDeviceGetProperties
	ASSERT_ZE_RET("zesDeviceGetProperties: NULL handle", zesDeviceGetProperties(bad_dev, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesDeviceGetProperties: NULL pProperties", zesDeviceGetProperties(dev, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_POINTER);

	// zesDeviceGetState
	ASSERT_ZE_RET("zesDeviceGetState: NULL handle", zesDeviceGetState(bad_dev, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesDeviceGetState: NULL pState", zesDeviceGetState(dev, NULL), ZE_RESULT_ERROR_INVALID_NULL_POINTER);

	// zesDeviceReset / zesDeviceResetExt
	ASSERT_ZE_RET("zesDeviceReset: NULL handle", zesDeviceReset(bad_dev, 0), ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesDeviceResetExt: NULL handle", zesDeviceResetExt(bad_dev, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);

	// zesDeviceProcessesGetState
	ASSERT_ZE_RET("zesDeviceProcessesGetState: NULL handle", zesDeviceProcessesGetState(bad_dev, NULL, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesDeviceProcessesGetState: NULL pCount", zesDeviceProcessesGetState(dev, NULL, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	{
		zes_process_state_t ps[1];
		ASSERT_ZE_RET("zesDeviceProcessesGetState: NULL pCount with array", zesDeviceProcessesGetState(dev, NULL, ps),
					  ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	}

	// zesDevicePciGetProperties/State/Bars/Stats
	ASSERT_ZE_RET("zesDevicePciGetProperties: NULL handle", zesDevicePciGetProperties(bad_dev, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesDevicePciGetProperties: NULL pProperties", zesDevicePciGetProperties(dev, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	ASSERT_ZE_RET("zesDevicePciGetState: NULL handle", zesDevicePciGetState(bad_dev, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesDevicePciGetState: NULL pState", zesDevicePciGetState(dev, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	{
		ASSERT_ZE_RET("zesDevicePciGetBars: NULL handle", zesDevicePciGetBars(bad_dev, NULL, NULL),
					  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
		ASSERT_ZE_RET("zesDevicePciGetBars: NULL pCount", zesDevicePciGetBars(dev, NULL, NULL),
					  ZE_RESULT_ERROR_INVALID_NULL_POINTER);
		zes_pci_bar_properties_t bars[1];
		ASSERT_ZE_RET("zesDevicePciGetBars: NULL pCount with array", zesDevicePciGetBars(dev, NULL, bars),
					  ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	}
	ASSERT_ZE_RET("zesDevicePciGetStats: NULL handle", zesDevicePciGetStats(bad_dev, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesDevicePciGetStats: NULL pStats", zesDevicePciGetStats(dev, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_POINTER);

	// zesDeviceEccAvailable/Configurable/GetEccState/SetEccState
	ASSERT_ZE_RET("zesDeviceEccAvailable: NULL handle", zesDeviceEccAvailable(bad_dev, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesDeviceEccAvailable: NULL pAvailable", zesDeviceEccAvailable(dev, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	ASSERT_ZE_RET("zesDeviceEccConfigurable: NULL handle", zesDeviceEccConfigurable(bad_dev, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesDeviceEccConfigurable: NULL pConfigurable", zesDeviceEccConfigurable(dev, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	ASSERT_ZE_RET("zesDeviceGetEccState: NULL handle", zesDeviceGetEccState(bad_dev, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesDeviceGetEccState: NULL pState", zesDeviceGetEccState(dev, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	ASSERT_ZE_RET("zesDeviceSetEccState: NULL handle", zesDeviceSetEccState(bad_dev, NULL, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesDeviceSetEccState: NULL pNewState", zesDeviceSetEccState(dev, NULL, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	{
		zes_device_ecc_desc_t desc = {0};
		ASSERT_ZE_RET("zesDeviceSetEccState: NULL pState", zesDeviceSetEccState(dev, &desc, NULL),
					  ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	}

	// Overclock device-level
	ASSERT_ZE_RET("zesDeviceSetOverclockWaiver: NULL handle", zesDeviceSetOverclockWaiver(bad_dev),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesDeviceGetOverclockDomains: NULL handle", zesDeviceGetOverclockDomains(bad_dev, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesDeviceGetOverclockDomains: NULL pOverclockDomains", zesDeviceGetOverclockDomains(dev, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	ASSERT_ZE_RET("zesDeviceGetOverclockControls: NULL handle", zesDeviceGetOverclockControls(bad_dev, 0, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesDeviceResetOverclockSettings: NULL handle", zesDeviceResetOverclockSettings(bad_dev, 0),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesDeviceReadOverclockState: NULL handle",
				  zesDeviceReadOverclockState(bad_dev, NULL, NULL, NULL, NULL, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	{
		ASSERT_ZE_RET("zesDeviceEnumOverclockDomains: NULL handle", zesDeviceEnumOverclockDomains(bad_dev, NULL, NULL),
					  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
		ASSERT_ZE_RET("zesDeviceEnumOverclockDomains: NULL pCount", zesDeviceEnumOverclockDomains(dev, NULL, NULL),
					  ZE_RESULT_ERROR_INVALID_NULL_POINTER);
		zes_overclock_handle_t oc[1];
		ASSERT_ZE_RET("zesDeviceEnumOverclockDomains: NULL pCount with array",
					  zesDeviceEnumOverclockDomains(dev, NULL, oc), ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	}

	// Overclock domain-level
	zes_overclock_handle_t bad_oc = NULL;
	ASSERT_ZE_RET("zesOverclockGetDomainProperties: NULL handle", zesOverclockGetDomainProperties(bad_oc, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesOverclockGetDomainVFProperties: NULL handle", zesOverclockGetDomainVFProperties(bad_oc, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesOverclockGetDomainControlProperties: NULL handle",
				  zesOverclockGetDomainControlProperties(bad_oc, 0, NULL), ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesOverclockGetControlCurrentValue: NULL handle",
				  zesOverclockGetControlCurrentValue(bad_oc, 0, NULL), ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesOverclockGetControlPendingValue: NULL handle",
				  zesOverclockGetControlPendingValue(bad_oc, 0, NULL), ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesOverclockSetControlUserValue: NULL handle", zesOverclockSetControlUserValue(bad_oc, 0, 0.0, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesOverclockGetControlState: NULL handle", zesOverclockGetControlState(bad_oc, 0, NULL, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesOverclockGetVFPointValues: NULL handle", zesOverclockGetVFPointValues(bad_oc, 0, 0, 0, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesOverclockSetVFPointValues: NULL handle", zesOverclockSetVFPointValues(bad_oc, 0, 0, 0),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);

	// Diagnostics
	{
		ASSERT_ZE_RET("zesDeviceEnumDiagnosticTestSuites: NULL handle",
					  zesDeviceEnumDiagnosticTestSuites(bad_dev, NULL, NULL), ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
		ASSERT_ZE_RET("zesDeviceEnumDiagnosticTestSuites: NULL pCount",
					  zesDeviceEnumDiagnosticTestSuites(dev, NULL, NULL), ZE_RESULT_ERROR_INVALID_NULL_POINTER);
		zes_diag_handle_t dh[1];
		ASSERT_ZE_RET("zesDeviceEnumDiagnosticTestSuites: NULL pCount with array",
					  zesDeviceEnumDiagnosticTestSuites(dev, NULL, dh), ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	}
	zes_diag_handle_t bad_diag = NULL;
	ASSERT_ZE_RET("zesDiagnosticsGetProperties: NULL handle", zesDiagnosticsGetProperties(bad_diag, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesDiagnosticsGetTests: NULL handle", zesDiagnosticsGetTests(bad_diag, NULL, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesDiagnosticsRunTests: NULL handle", zesDiagnosticsRunTests(bad_diag, 0, 0, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);

	// Engine groups
	{
		ASSERT_ZE_RET("zesDeviceEnumEngineGroups: NULL handle", zesDeviceEnumEngineGroups(bad_dev, NULL, NULL),
					  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
		ASSERT_ZE_RET("zesDeviceEnumEngineGroups: NULL pCount", zesDeviceEnumEngineGroups(dev, NULL, NULL),
					  ZE_RESULT_ERROR_INVALID_NULL_POINTER);
		zes_engine_handle_t eh[1];
		ASSERT_ZE_RET("zesDeviceEnumEngineGroups: NULL pCount with array", zesDeviceEnumEngineGroups(dev, NULL, eh),
					  ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	}
	ASSERT_ZE_RET("zesEngineGetProperties: NULL handle", zesEngineGetProperties(bad_eng, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesEngineGetProperties: NULL pProperties", zesEngineGetProperties(eng0, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	{
		ASSERT_ZE_RET("zesEngineGetActivity: NULL handle", zesEngineGetActivity(bad_eng, NULL),
					  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
		ASSERT_ZE_RET("zesEngineGetActivity: NULL pStats", zesEngineGetActivity(eng0, NULL),
					  ZE_RESULT_ERROR_INVALID_NULL_POINTER);
		zes_engine_stats_t st[1];
		ASSERT_ZE_RET("zesEngineGetActivity: NULL pCount with array", zesEngineGetActivityExt(eng0, NULL, st),
					  ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	}
	{
		ASSERT_ZE_RET("zesEngineGetActivityExt: NULL handle", zesEngineGetActivityExt(bad_eng, NULL, NULL),
					  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
		ASSERT_ZE_RET("zesEngineGetActivityExt: NULL pCount", zesEngineGetActivityExt(eng0, NULL, NULL),
					  ZE_RESULT_ERROR_INVALID_NULL_POINTER);
		zes_engine_stats_t st[1];
		ASSERT_ZE_RET("zesEngineGetActivityExt: NULL pCount with array", zesEngineGetActivityExt(eng0, NULL, st),
					  ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	}

	// Events
	ASSERT_ZE_RET("zesDeviceEventRegister: NULL handle", zesDeviceEventRegister(bad_dev, 0),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesDriverEventListen: NULL handle", zesDriverEventListen(bad_drv, 0, 0, NULL, NULL, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesDriverEventListenEx: NULL handle", zesDriverEventListenEx(bad_drv, 0, 0, NULL, NULL, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	{
		uint32_t n = 0;
		zes_event_type_flags_t ev = 0;
		ASSERT_ZE_RET("zesDriverEventListen: NULL phDevices", zesDriverEventListen(drv, 0, 0, NULL, &n, &ev),
					  ZE_RESULT_ERROR_INVALID_NULL_POINTER);
		ASSERT_ZE_RET("zesDriverEventListen: NULL pNumDeviceEvents",
					  zesDriverEventListen(drv, 0, 0, (zes_device_handle_t *)&dev, NULL, &ev),
					  ZE_RESULT_ERROR_INVALID_NULL_POINTER);
		ASSERT_ZE_RET("zesDriverEventListen: NULL pEvents",
					  zesDriverEventListen(drv, 0, 0, (zes_device_handle_t *)&dev, &n, NULL),
					  ZE_RESULT_ERROR_INVALID_NULL_POINTER);
		ASSERT_ZE_RET("zesDriverEventListenEx: NULL phDevices", zesDriverEventListenEx(drv, 0, 0, NULL, &n, &ev),
					  ZE_RESULT_ERROR_INVALID_NULL_POINTER);
		ASSERT_ZE_RET("zesDriverEventListenEx: NULL pNumDeviceEvents",
					  zesDriverEventListenEx(drv, 0, 0, (zes_device_handle_t *)&dev, NULL, &ev),
					  ZE_RESULT_ERROR_INVALID_NULL_POINTER);
		ASSERT_ZE_RET("zesDriverEventListenEx: NULL pEvents",
					  zesDriverEventListenEx(drv, 0, 0, (zes_device_handle_t *)&dev, &n, NULL),
					  ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	}

	// Fabric ports
	{
		ASSERT_ZE_RET("zesDeviceEnumFabricPorts: NULL handle", zesDeviceEnumFabricPorts(bad_dev, NULL, NULL),
					  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
		ASSERT_ZE_RET("zesDeviceEnumFabricPorts: NULL pCount", zesDeviceEnumFabricPorts(dev, NULL, NULL),
					  ZE_RESULT_ERROR_INVALID_NULL_POINTER);
		zes_fabric_port_handle_t fph[1];
		ASSERT_ZE_RET("zesDeviceEnumFabricPorts: NULL pCount with array", zesDeviceEnumFabricPorts(dev, NULL, fph),
					  ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	}
	zes_fabric_port_handle_t bad_fp = NULL;
	ASSERT_ZE_RET("zesFabricPortGetProperties: NULL handle", zesFabricPortGetProperties(bad_fp, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesFabricPortGetLinkType: NULL handle", zesFabricPortGetLinkType(bad_fp, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesFabricPortGetConfig: NULL handle", zesFabricPortGetConfig(bad_fp, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesFabricPortSetConfig: NULL handle", zesFabricPortSetConfig(bad_fp, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesFabricPortGetState: NULL handle", zesFabricPortGetState(bad_fp, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesFabricPortGetThroughput: NULL handle", zesFabricPortGetThroughput(bad_fp, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesFabricPortGetFabricErrorCounters: NULL handle", zesFabricPortGetFabricErrorCounters(bad_fp, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesFabricPortGetMultiPortThroughput: NULL handle",
				  zesFabricPortGetMultiPortThroughput(bad_dev, 0, NULL, NULL), ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	{
		zes_fabric_port_handle_t fp_arr[1] = {NULL};
		zes_fabric_port_throughput_t *tp_arr[1] = {NULL};
		ASSERT_ZE_RET("zesFabricPortGetMultiPortThroughput: NULL phPort",
					  zesFabricPortGetMultiPortThroughput(dev, 1, NULL, tp_arr), ZE_RESULT_ERROR_INVALID_NULL_POINTER);
		ASSERT_ZE_RET("zesFabricPortGetMultiPortThroughput: NULL pThroughput",
					  zesFabricPortGetMultiPortThroughput(dev, 1, fp_arr, NULL), ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	}

	// Fans
	{
		ASSERT_ZE_RET("zesDeviceEnumFans: NULL handle", zesDeviceEnumFans(bad_dev, NULL, NULL),
					  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
		ASSERT_ZE_RET("zesDeviceEnumFans: NULL pCount", zesDeviceEnumFans(dev, NULL, NULL),
					  ZE_RESULT_ERROR_INVALID_NULL_POINTER);
		zes_fan_handle_t fh[1];
		ASSERT_ZE_RET("zesDeviceEnumFans: NULL pCount with array", zesDeviceEnumFans(dev, NULL, fh),
					  ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	}
	zes_fan_handle_t bad_fan = NULL;
	ASSERT_ZE_RET("zesFanGetProperties: NULL handle", zesFanGetProperties(bad_fan, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesFanGetConfig: NULL handle", zesFanGetConfig(bad_fan, NULL), ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesFanSetDefaultMode: NULL handle", zesFanSetDefaultMode(bad_fan),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesFanSetFixedSpeedMode: NULL handle", zesFanSetFixedSpeedMode(bad_fan, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesFanSetSpeedTableMode: NULL handle", zesFanSetSpeedTableMode(bad_fan, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesFanGetState: NULL handle", zesFanGetState(bad_fan, 0, NULL), ZE_RESULT_ERROR_INVALID_NULL_HANDLE);

	// Firmware
	{
		ASSERT_ZE_RET("zesDeviceEnumFirmwares: NULL handle", zesDeviceEnumFirmwares(bad_dev, NULL, NULL),
					  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
		ASSERT_ZE_RET("zesDeviceEnumFirmwares: NULL pCount", zesDeviceEnumFirmwares(dev, NULL, NULL),
					  ZE_RESULT_ERROR_INVALID_NULL_POINTER);
		zes_firmware_handle_t fwh[1];
		ASSERT_ZE_RET("zesDeviceEnumFirmwares: NULL pCount with array", zesDeviceEnumFirmwares(dev, NULL, fwh),
					  ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	}
	zes_firmware_handle_t bad_fw = NULL;
	ASSERT_ZE_RET("zesFirmwareGetProperties: NULL handle", zesFirmwareGetProperties(bad_fw, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesFirmwareFlash: NULL handle", zesFirmwareFlash(bad_fw, NULL, 0),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesFirmwareGetFlashProgress: NULL handle", zesFirmwareGetFlashProgress(bad_fw, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesFirmwareGetConsoleLogs: NULL handle", zesFirmwareGetConsoleLogs(bad_fw, NULL, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	{
		// Get a real firmware handle to test NULL pSize with non-NULL log buffer.
		uint32_t fw_n = 1;
		zes_firmware_handle_t fw = NULL;
		zesDeviceEnumFirmwares(dev, &fw_n, &fw);
		ASSERT_ZE_RET("zesFirmwareGetConsoleLogs: NULL pSize with non-NULL log buf",
					  zesFirmwareGetConsoleLogs(fw, NULL, (char *)(uintptr_t)1), ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	}

	// Frequency domains
	{
		ASSERT_ZE_RET("zesDeviceEnumFrequencyDomains: NULL handle", zesDeviceEnumFrequencyDomains(bad_dev, NULL, NULL),
					  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
		ASSERT_ZE_RET("zesDeviceEnumFrequencyDomains: NULL pCount", zesDeviceEnumFrequencyDomains(dev, NULL, NULL),
					  ZE_RESULT_ERROR_INVALID_NULL_POINTER);
		zes_freq_handle_t fh[1];
		ASSERT_ZE_RET("zesDeviceEnumFrequencyDomains: NULL pCount with array",
					  zesDeviceEnumFrequencyDomains(dev, NULL, fh), ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	}
	zes_freq_handle_t bad_freq = NULL;
	ASSERT_ZE_RET("zesFrequencyGetProperties: NULL handle", zesFrequencyGetProperties(bad_freq, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesFrequencyGetAvailableClocks: NULL handle", zesFrequencyGetAvailableClocks(bad_freq, NULL, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesFrequencyGetRange: NULL handle", zesFrequencyGetRange(bad_freq, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesFrequencySetRange: NULL handle", zesFrequencySetRange(bad_freq, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesFrequencyGetState: NULL handle", zesFrequencyGetState(bad_freq, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesFrequencyGetThrottleTime: NULL handle", zesFrequencyGetThrottleTime(bad_freq, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	{
		uint32_t freq_n = 1;
		zes_freq_handle_t freq = NULL;
		zesDeviceEnumFrequencyDomains(dev, &freq_n, &freq);
		ASSERT_ZE_RET("zesFrequencyGetRange: NULL pLimits", zesFrequencyGetRange(freq, NULL),
					  ZE_RESULT_ERROR_INVALID_NULL_POINTER);
		ASSERT_ZE_RET("zesFrequencyGetAvailableClocks: NULL pCount", zesFrequencyGetAvailableClocks(freq, NULL, NULL),
					  ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	}

	// LEDs
	{
		ASSERT_ZE_RET("zesDeviceEnumLeds: NULL handle", zesDeviceEnumLeds(bad_dev, NULL, NULL),
					  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
		ASSERT_ZE_RET("zesDeviceEnumLeds: NULL pCount", zesDeviceEnumLeds(dev, NULL, NULL),
					  ZE_RESULT_ERROR_INVALID_NULL_POINTER);
		zes_led_handle_t lh[1];
		ASSERT_ZE_RET("zesDeviceEnumLeds: NULL pCount with array", zesDeviceEnumLeds(dev, NULL, lh),
					  ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	}
	zes_led_handle_t bad_led = NULL;
	ASSERT_ZE_RET("zesLedGetProperties: NULL handle", zesLedGetProperties(bad_led, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesLedGetState: NULL handle", zesLedGetState(bad_led, NULL), ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesLedSetState: NULL handle", zesLedSetState(bad_led, 0), ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesLedSetColor: NULL handle", zesLedSetColor(bad_led, NULL), ZE_RESULT_ERROR_INVALID_NULL_HANDLE);

	// Memory modules
	{
		ASSERT_ZE_RET("zesDeviceEnumMemoryModules: NULL handle", zesDeviceEnumMemoryModules(bad_dev, NULL, NULL),
					  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
		ASSERT_ZE_RET("zesDeviceEnumMemoryModules: NULL pCount", zesDeviceEnumMemoryModules(dev, NULL, NULL),
					  ZE_RESULT_ERROR_INVALID_NULL_POINTER);
		zes_mem_handle_t mh[1];
		ASSERT_ZE_RET("zesDeviceEnumMemoryModules: NULL pCount with array", zesDeviceEnumMemoryModules(dev, NULL, mh),
					  ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	}
	zes_mem_handle_t bad_mem = NULL;
	ASSERT_ZE_RET("zesMemoryGetProperties: NULL handle", zesMemoryGetProperties(bad_mem, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesMemoryGetState: NULL handle", zesMemoryGetState(bad_mem, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesMemoryGetBandwidth: NULL handle", zesMemoryGetBandwidth(bad_mem, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);

	// Performance factor domains
	{
		ASSERT_ZE_RET("zesDeviceEnumPerformanceFactorDomains: NULL handle",
					  zesDeviceEnumPerformanceFactorDomains(bad_dev, NULL, NULL), ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
		ASSERT_ZE_RET("zesDeviceEnumPerformanceFactorDomains: NULL pCount",
					  zesDeviceEnumPerformanceFactorDomains(dev, NULL, NULL), ZE_RESULT_ERROR_INVALID_NULL_POINTER);
		zes_perf_handle_t ph[1];
		ASSERT_ZE_RET("zesDeviceEnumPerformanceFactorDomains: NULL pCount with array",
					  zesDeviceEnumPerformanceFactorDomains(dev, NULL, ph), ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	}
	zes_perf_handle_t bad_perf = NULL;
	ASSERT_ZE_RET("zesPerformanceFactorGetProperties: NULL handle", zesPerformanceFactorGetProperties(bad_perf, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesPerformanceFactorGetConfig: NULL handle", zesPerformanceFactorGetConfig(bad_perf, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesPerformanceFactorSetConfig: NULL handle", zesPerformanceFactorSetConfig(bad_perf, 0.0),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);

	// Power domains
	{
		ASSERT_ZE_RET("zesDeviceEnumPowerDomains: NULL handle", zesDeviceEnumPowerDomains(bad_dev, NULL, NULL),
					  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
		ASSERT_ZE_RET("zesDeviceEnumPowerDomains: NULL pCount", zesDeviceEnumPowerDomains(dev, NULL, NULL),
					  ZE_RESULT_ERROR_INVALID_NULL_POINTER);
		zes_pwr_handle_t pwh[1];
		ASSERT_ZE_RET("zesDeviceEnumPowerDomains: NULL pCount with array", zesDeviceEnumPowerDomains(dev, NULL, pwh),
					  ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	}
	zes_pwr_handle_t bad_pwr = NULL;
	ASSERT_ZE_RET("zesPowerGetProperties: NULL handle", zesPowerGetProperties(bad_pwr, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesPowerGetEnergyCounter: NULL handle", zesPowerGetEnergyCounter(bad_pwr, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	{
		uint32_t n = 0;
		ASSERT_ZE_RET("zesPowerGetLimitsExt: NULL handle", zesPowerGetLimitsExt(bad_pwr, &n, NULL),
					  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
		ASSERT_ZE_RET("zesPowerGetLimitsExt: NULL pCount", zesPowerGetLimitsExt(bad_pwr, NULL, NULL),
					  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
		zes_power_limit_ext_desc_t pl[1];
		ASSERT_ZE_RET("zesPowerGetLimitsExt: NULL pCount with array", zesPowerGetLimitsExt(bad_pwr, NULL, pl),
					  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
		ASSERT_ZE_RET("zesPowerSetLimitsExt: NULL handle", zesPowerSetLimitsExt(bad_pwr, &n, NULL),
					  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	}
	ASSERT_ZE_RET("zesPowerGetEnergyThreshold: NULL handle", zesPowerGetEnergyThreshold(bad_pwr, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesPowerSetEnergyThreshold: NULL handle", zesPowerSetEnergyThreshold(bad_pwr, 0.0),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);

	// PSUs
	{
		ASSERT_ZE_RET("zesDeviceEnumPsus: NULL handle", zesDeviceEnumPsus(bad_dev, NULL, NULL),
					  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
		ASSERT_ZE_RET("zesDeviceEnumPsus: NULL pCount", zesDeviceEnumPsus(dev, NULL, NULL),
					  ZE_RESULT_ERROR_INVALID_NULL_POINTER);
		zes_psu_handle_t ph[1];
		ASSERT_ZE_RET("zesDeviceEnumPsus: NULL pCount with array", zesDeviceEnumPsus(dev, NULL, ph),
					  ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	}
	zes_psu_handle_t bad_psu = NULL;
	ASSERT_ZE_RET("zesPsuGetProperties: NULL handle", zesPsuGetProperties(bad_psu, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesPsuGetState: NULL handle", zesPsuGetState(bad_psu, NULL), ZE_RESULT_ERROR_INVALID_NULL_HANDLE);

	// RAS error sets
	{
		ASSERT_ZE_RET("zesDeviceEnumRasErrorSets: NULL handle", zesDeviceEnumRasErrorSets(bad_dev, NULL, NULL),
					  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
		ASSERT_ZE_RET("zesDeviceEnumRasErrorSets: NULL pCount", zesDeviceEnumRasErrorSets(dev, NULL, NULL),
					  ZE_RESULT_ERROR_INVALID_NULL_POINTER);
		zes_ras_handle_t rh[1];
		ASSERT_ZE_RET("zesDeviceEnumRasErrorSets: NULL pCount with array", zesDeviceEnumRasErrorSets(dev, NULL, rh),
					  ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	}
	zes_ras_handle_t bad_ras = NULL;
	ASSERT_ZE_RET("zesRasGetProperties: NULL handle", zesRasGetProperties(bad_ras, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesRasGetConfig: NULL handle", zesRasGetConfig(bad_ras, NULL), ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesRasSetConfig: NULL handle", zesRasSetConfig(bad_ras, NULL), ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesRasGetState: NULL handle", zesRasGetState(bad_ras, 0, NULL), ZE_RESULT_ERROR_INVALID_NULL_HANDLE);

	// Schedulers
	{
		ASSERT_ZE_RET("zesDeviceEnumSchedulers: NULL handle", zesDeviceEnumSchedulers(bad_dev, NULL, NULL),
					  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
		ASSERT_ZE_RET("zesDeviceEnumSchedulers: NULL pCount", zesDeviceEnumSchedulers(dev, NULL, NULL),
					  ZE_RESULT_ERROR_INVALID_NULL_POINTER);
		zes_sched_handle_t sh[1];
		ASSERT_ZE_RET("zesDeviceEnumSchedulers: NULL pCount with array", zesDeviceEnumSchedulers(dev, NULL, sh),
					  ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	}
	zes_sched_handle_t bad_sched = NULL;
	ASSERT_ZE_RET("zesSchedulerGetProperties: NULL handle", zesSchedulerGetProperties(bad_sched, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesSchedulerGetCurrentMode: NULL handle", zesSchedulerGetCurrentMode(bad_sched, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesSchedulerGetTimeoutModeProperties: NULL handle",
				  zesSchedulerGetTimeoutModeProperties(bad_sched, 0, NULL), ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesSchedulerGetTimesliceModeProperties: NULL handle",
				  zesSchedulerGetTimesliceModeProperties(bad_sched, 0, NULL), ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesSchedulerSetTimeoutMode: NULL handle", zesSchedulerSetTimeoutMode(bad_sched, NULL, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesSchedulerSetTimesliceMode: NULL handle", zesSchedulerSetTimesliceMode(bad_sched, NULL, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesSchedulerSetExclusiveMode: NULL handle", zesSchedulerSetExclusiveMode(bad_sched, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	{
		uint32_t sched_n = 1;
		zes_sched_handle_t sched = NULL;
		zesDeviceEnumSchedulers(dev, &sched_n, &sched);
		zes_sched_timeout_properties_t tp = {0};
		ze_bool_t nr = 0;
		ASSERT_ZE_RET("zesSchedulerSetTimeoutMode: NULL pNeedReload", zesSchedulerSetTimeoutMode(sched, &tp, NULL),
					  ZE_RESULT_ERROR_INVALID_NULL_POINTER);
		ASSERT_ZE_RET("zesSchedulerSetTimesliceMode: NULL pTimesliceProperties",
					  zesSchedulerSetTimesliceMode(sched, NULL, &nr), ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	}

	// Standby domains
	{
		ASSERT_ZE_RET("zesDeviceEnumStandbyDomains: NULL handle", zesDeviceEnumStandbyDomains(bad_dev, NULL, NULL),
					  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
		ASSERT_ZE_RET("zesDeviceEnumStandbyDomains: NULL pCount", zesDeviceEnumStandbyDomains(dev, NULL, NULL),
					  ZE_RESULT_ERROR_INVALID_NULL_POINTER);
		zes_standby_handle_t sh[1];
		ASSERT_ZE_RET("zesDeviceEnumStandbyDomains: NULL pCount with array", zesDeviceEnumStandbyDomains(dev, NULL, sh),
					  ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	}
	zes_standby_handle_t bad_sb = NULL;
	ASSERT_ZE_RET("zesStandbyGetProperties: NULL handle", zesStandbyGetProperties(bad_sb, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesStandbyGetMode: NULL handle", zesStandbyGetMode(bad_sb, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesStandbySetMode: NULL handle", zesStandbySetMode(bad_sb, 0), ZE_RESULT_ERROR_INVALID_NULL_HANDLE);

	// Temperature sensors
	{
		ASSERT_ZE_RET("zesDeviceEnumTemperatureSensors: NULL handle",
					  zesDeviceEnumTemperatureSensors(bad_dev, NULL, NULL), ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
		ASSERT_ZE_RET("zesDeviceEnumTemperatureSensors: NULL pCount", zesDeviceEnumTemperatureSensors(dev, NULL, NULL),
					  ZE_RESULT_ERROR_INVALID_NULL_POINTER);
		zes_temp_handle_t th[1];
		ASSERT_ZE_RET("zesDeviceEnumTemperatureSensors: NULL pCount with array",
					  zesDeviceEnumTemperatureSensors(dev, NULL, th), ZE_RESULT_ERROR_INVALID_NULL_POINTER);
	}
	zes_temp_handle_t bad_temp = NULL;
	ASSERT_ZE_RET("zesTemperatureGetProperties: NULL handle", zesTemperatureGetProperties(bad_temp, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesTemperatureGetConfig: NULL handle", zesTemperatureGetConfig(bad_temp, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesTemperatureSetConfig: NULL handle", zesTemperatureSetConfig(bad_temp, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);
	ASSERT_ZE_RET("zesTemperatureGetState: NULL handle", zesTemperatureGetState(bad_temp, NULL),
				  ZE_RESULT_ERROR_INVALID_NULL_HANDLE);

	// zesDeviceProcessesGetState: INVALID_SIZE when buffer is too small
	{
		uint32_t n = 0;
		zesDeviceProcessesGetState(dev, &n, NULL);
		uint32_t too_small = n - 1;
		zes_process_state_t ps[8];
		ASSERT_ZE_RET("zesDeviceProcessesGetState: buffer too small", zesDeviceProcessesGetState(dev, &too_small, ps),
					  ZE_RESULT_ERROR_INVALID_SIZE);
	}

	sysman_state_reset();
}

// ------------------------------------------------------------------
// UUID parsing
// ------------------------------------------------------------------

static void test_uuid(void)
{
	printf("test_uuid\n");

	sysman_state_reset();
	ASSERT("load one_device.yaml", sysman_state_load(YAML_ONE_ENGINE) == 0);

	uint32_t drv_n = 1;
	ze_driver_handle_t drv = NULL;
	zesDriverGet(&drv_n, &drv);

	uint32_t dev_n = 1;
	zes_device_handle_t dev = NULL;
	zesDeviceGet(drv, &dev_n, &dev);

	zes_device_properties_t props = {.stype = ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES};
	ASSERT_ZE_OK("zesDeviceGetProperties", zesDeviceGetProperties(dev, &props));

	// Core UUID: "12345678-abcd-ef01-2345-6789abcdef01"
	const uint8_t expected_core[16] = {0x12, 0x34, 0x56, 0x78, 0xab, 0xcd, 0xef, 0x01,
									   0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef, 0x01};
	ASSERT("UUID matches", memcmp(props.core.uuid.id, expected_core, 16) == 0);

	sysman_state_reset();
}

static void test_uuid_invalid_load(void)
{
	printf("test_uuid_invalid_load\n");

	sysman_state_reset();
	ASSERT("sysman_state_load fails on invalid UUID", sysman_state_load(YAML_INVALID_UUID) != 0);
}

// ------------------------------------------------------------------
// Main
// ------------------------------------------------------------------

int main(void)
{
	printf("=== stub handle persistence tests ===\n");

	test_driver_handle_persists();
	test_device_handle_persists();
	test_engine_handle_persists();
	test_engine_handle_invalidated_when_slot_removed();
	test_device_handle_invalidated_when_slot_removed();
	test_driver_handle_invalidated_when_slot_removed();
	test_wrong_handle_type_rejected();
	test_state_load_without_config_clears_state();
	test_auto_reload();
	test_array_null_vs_empty();
	test_error_cases();
	test_uuid();
	test_uuid_invalid_load();

	printf("\n%d passed, %d failed\n", g_pass, g_fail);
	return g_fail ? 1 : 0;
}
