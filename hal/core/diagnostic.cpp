/*
 * Copyright © 2025 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 */

#include "diagnostic.h"
#include <vector>

/**
 * @brief Destructor for the diagnostic class
 *
 * This destructor performs cleanup operations for the diagnostic management
 * object, releasing allocated memory for diagnostic test suite handles and
 * ensuring proper resource deallocation when the diagnostic object is destroyed.
 */
diagnostic::~diagnostic()
{
	if (testSuites) {
		delete[] testSuites;
		testSuites = nullptr;
	}
}

/**
 * @brief Enumerates available diagnostic test suites for a device
 *
 * This function discovers and catalogs all diagnostic test suites available on
 * the specified device. Diagnostic test suites provide comprehensive hardware
 * validation and health checking capabilities for GPU components.
 *
 * @param device Handle to the Level Zero Sysman device
 * @return ze_result_t ZE_RESULT_SUCCESS on successful enumeration, error code otherwise
 */
ze_result_t diagnostic::enumDiag(zes_device_handle_t device)
{
	// Enumerate diagnostic test suites
	ze_result_t result = zesDeviceEnumDiagnosticTestSuites(device, &testSuiteCount, nullptr);
	if (result != ZE_RESULT_SUCCESS || testSuiteCount == 0) {
		ERR("Failed to enumerate diagnostic test suites or no test suites found: 0x%X (%s)\n", result,
			l0_error_to_string(result));
		return result;
	}

	testSuites = new zes_diag_handle_t[testSuiteCount];
	result = zesDeviceEnumDiagnosticTestSuites(device, &testSuiteCount, testSuites);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get diagnostic test suite handles: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("  - Device has %d diagnostic test suites:\n", testSuiteCount);
	return result;
}

/**
 * @brief Gets properties for a specific diagnostic test suite
 *
 * This function retrieves detailed properties and characteristics for a
 * specific diagnostic test suite, including suite name, type, test availability,
 * and subdevice association information for comprehensive diagnostic analysis.
 *
 * @param testSuite Handle to the specific diagnostic test suite
 * @return ze_result_t ZE_RESULT_SUCCESS on successful property retrieval, error code otherwise
 */
ze_result_t diagnostic::getProperties(zes_diag_handle_t testSuite)
{
	ze_result_t result = ZE_RESULT_SUCCESS;
	zes_diag_properties_t diagProperties = {};
	result = zesDiagnosticsGetProperties(testSuite, &diagProperties);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get diagnostic test suite properties: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}
	DBG("    - Test Suite Name: %s\n", diagProperties.name);
	DBG("    - Type : %d\n", diagProperties.stype);
	DBG("    - have Tests : %d\n", diagProperties.haveTests);
	DBG("    - onSubDevice : %d\n", diagProperties.onSubdevice);
	DBG("    - subdeviceId : %d\n", diagProperties.subdeviceId);
	return result;
}

/**
 * @brief Gets available tests within a diagnostic test suite
 *
 * This function retrieves all individual diagnostic tests available within
 * a specific test suite, providing detailed information about test indices
 * and names for comprehensive hardware validation capabilities.
 *
 * @param testSuite Handle to the specific diagnostic test suite
 * @return int Number of test suites available, or 0 on error
 */
int diagnostic::getTests(zes_diag_handle_t testSuite)
{
	uint32_t testCount = 0;
	// Get tests within the diagnostic test suite
	ze_result_t result = zesDiagnosticsGetTests(testSuite, &testCount, nullptr);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get diagnostic tests count: 0x%X (%s)\n", result, l0_error_to_string(result));
		return 0;
	}

	std::vector<zes_diag_test_t> tests(testCount);
	result = zesDiagnosticsGetTests(testSuite, &testCount, tests.data());
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get diagnostic tests: 0x%X (%s)\n", result, l0_error_to_string(result));
		return 0;
	}

	DBG("    - Test Suite has %d tests:\n", testCount);

	for (const auto &test : tests) {
		DBG("      - Test Index: %d\n", test.index);
		DBG("      - Test Name: %s\n", test.name);
	}
	return testSuiteCount;
}

/**
 * @brief Executes diagnostic tests within a test suite
 *
 * This function runs the specified number of diagnostic tests within a test suite,
 * performing comprehensive hardware validation and health checking to ensure
 * device functionality and identify potential issues.
 *
 * @param testSuite Handle to the specific diagnostic test suite
 * @param testCount Number of tests to execute within the suite
 * @return ze_result_t ZE_RESULT_SUCCESS on successful test execution, error code otherwise
 */
ze_result_t diagnostic::runTests(zes_diag_handle_t testSuite, uint32_t testCount)
{
	// Run diagnostic tests
	ze_result_t result = zesDiagnosticsRunTests(testSuite, 0, testCount, nullptr);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to run diagnostic tests: 0x%X (%s)\n", result, l0_error_to_string(result));
	} else {
		DBG("    - Diagnostic tests run successfully.\n");
	}

	return result;
}

/**
 * @brief Performs comprehensive diagnostic runtime operations
 *
 * This function executes a complete diagnostic cycle including test suite
 * enumeration, property retrieval, test discovery, and test execution for
 * thorough hardware validation and device health assessment.
 *
 * @param device Handle to the device for diagnostic operations
 * @return ze_result_t ZE_RESULT_SUCCESS on successful execution, error code otherwise
 */
ze_result_t diagnostic::zesRun(zes_device_handle_t device)
{
	ze_result_t result = enumDiag(device);
	if (result != ZE_RESULT_SUCCESS)
		return result;

	// Run diagnostic tests for each test suite
	for (uint32_t i = 0; i < testSuiteCount; i++) {
		zes_diag_handle_t testSuite = testSuites[i];
		getProperties(testSuite);
		uint32_t testCount = getTests(testSuite);
		runTests(testSuite, testCount);
	}
	return result;
}