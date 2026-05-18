/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "../http_client.h"
#include <windows.h>
#include <winhttp.h>
#include <string>
#include <vector>

#pragma comment(lib, "winhttp.lib")

/**
 * @brief Constructor for HttpClient class (Windows implementation)
 *
 * Initializes the HTTP client with default configuration settings using WinHTTP.
 * Sets up default timeout and SSL verification settings for Windows platform.
 *
 * @note Initializes default timeout and disables SSL verification by default
 * @note Uses WinHTTP library for HTTP communication on Windows
 */
HttpClient::HttpClient()
{
	timeoutSeconds = HTTP_DEFAULT_TIMEOUT;
	sslVerify = false; // Equivalent to curl -k
}

/**
 * @brief Destructor for HttpClient class (Windows implementation)
 *
 * Performs cleanup of WinHTTP resources and handles. Windows implementation
 * does not require global cleanup like libcurl.
 *
 * @note WinHTTP handles are cleaned up per-request rather than globally
 */
HttpClient::~HttpClient() {}

/**
 * @brief Perform HTTP GET request without authentication (Windows implementation)
 *
 * Executes an HTTP GET request to the specified URL using WinHTTP without
 * authentication credentials. Uses current timeout and SSL verification settings.
 *
 * @param url Target URL for the HTTP GET request
 * @param response Reference to HttpResponse structure to store the response
 *
 * @return Status of the HTTP request
 * @retval HTTP_SUCCESS Request completed successfully
 * @retval HTTP_FAILURE Request failed due to network or protocol error
 * @retval HTTP_TIMEOUT Request timed out
 *
 * @note Response data is stored directly in the HttpResponse structure
 * @note Windows implementation using WinHTTP API
 */
int HttpClient::get(const std::string &url, HttpResponse &response) { return performRequest(url, "", "", response); }

/**
 * @brief Perform HTTP GET request with basic authentication (Windows implementation)
 *
 * Executes an HTTP GET request to the specified URL using WinHTTP with basic
 * authentication using the provided username and password credentials.
 *
 * @param url Target URL for the HTTP GET request
 * @param username Username for basic authentication
 * @param password Password for basic authentication
 * @param response Reference to HttpResponse structure to store the response
 *
 * @return Status of the HTTP request
 * @retval HTTP_SUCCESS Request completed successfully
 * @retval HTTP_FAILURE Request failed due to network or protocol error
 * @retval HTTP_TIMEOUT Request timed out
 *
 * @note Response data is stored directly in the HttpResponse structure
 * @note Uses Windows WinHTTP basic authentication
 */
int HttpClient::getWithAuth(const std::string &url, const std::string &username, const std::string &password,
							HttpResponse &response)
{
	return performRequest(url, username, password, response);
}

/**
 * @brief Set HTTP request timeout value (Windows implementation)
 *
 * Configures the timeout duration for HTTP requests using WinHTTP. Requests
 * that exceed this timeout will be cancelled and return HTTP_TIMEOUT status.
 *
 * @param timeoutSec Timeout duration in seconds
 *
 * @note Timeout applies to all subsequent HTTP requests
 * @note Uses WinHTTP timeout configuration
 */
void HttpClient::setTimeout(int timeoutSec) { timeoutSeconds = timeoutSec; }

/**
 * @brief Configure SSL certificate verification (Windows implementation)
 *
 * Enables or disables SSL certificate verification for HTTPS requests using WinHTTP.
 * When disabled, equivalent to using insecure mode for self-signed certificates.
 *
 * @param verify true to enable SSL verification, false to disable
 *
 * @note Default is false (SSL verification disabled)
 * @note Disabling SSL verification reduces security but may be needed for self-signed certificates
 * @note Uses WinHTTP SSL security flags
 */
void HttpClient::setSslVerify(bool verify) { sslVerify = verify; }

/**
 * @brief Perform HTTP request with optional authentication (Windows implementation)
 *
 * Internal function that performs the actual HTTP request using WinHTTP API.
 * Handles URL parsing, connection establishment, authentication, SSL settings,
 * and response data collection.
 *
 * @param url Target URL for the HTTP request
 * @param username Username for basic authentication (can be empty string)
 * @param password Password for basic authentication (can be empty string)
 * @param response Reference to HttpResponse structure to store the response
 *
 * @return Status of the HTTP request
 * @retval HTTP_SUCCESS Request completed successfully
 * @retval HTTP_FAILURE Request failed due to network or protocol error
 * @retval HTTP_TIMEOUT Request timed out
 * @retval HTTP_NOT_FOUND HTTP error response received
 *
 * @note Response data is stored directly in the HttpResponse structure
 * @note Handles WinHTTP session, connection, and request lifecycle
 * @note Sets user agent to "AMC Redfish Client/1.0"
 * @note Properly cleans up WinHTTP handles on error paths
 */
int HttpClient::performRequest(const std::string &url, const std::string &username, const std::string &password,
							   HttpResponse &response)
{
	// Declare all variables at the beginning to avoid goto issues
	HINTERNET hSession = nullptr;
	HINTERNET hConnect = nullptr;
	HINTERNET hRequest = nullptr;
	int result = HTTP_FAILURE;
	DWORD bytesAvailable = 0;
	DWORD statusCode = 0;
	DWORD statusCodeSize = sizeof(statusCode);
	DWORD flags = 0;
	std::vector<char> buffer;
	std::wstring hostname;
	std::wstring urlPath;
	WCHAR wideUrl[512];

	// Initialize response
	response.statusCode = 0;
	response.responseData.clear();

	// Parse URL
	URL_COMPONENTS urlComp = {0};
	urlComp.dwStructSize = sizeof(urlComp);
	urlComp.dwSchemeLength = static_cast<DWORD>(-1);
	urlComp.dwHostNameLength = static_cast<DWORD>(-1);
	urlComp.dwUrlPathLength = static_cast<DWORD>(-1);

	MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1, wideUrl, sizeof(wideUrl) / sizeof(WCHAR));

	if (!WinHttpCrackUrl(wideUrl, 0, 0, &urlComp)) {
		return HTTP_FAILURE;
	}

	// Create session
	hSession = WinHttpOpen(L"AMC Redfish Client/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
						   WINHTTP_NO_PROXY_BYPASS, 0);
	if (!hSession) {
		return HTTP_FAILURE;
	}

	// Set timeouts
	WinHttpSetTimeouts(hSession, timeoutSeconds * 1000, timeoutSeconds * 1000, timeoutSeconds * 1000,
					   timeoutSeconds * 1000);

	// Extract hostname
	if (urlComp.lpszHostName && urlComp.dwHostNameLength > 0) {
		hostname.assign(urlComp.lpszHostName, urlComp.dwHostNameLength);
	} else {
		// Handle the error case, e.g., set hostname to an empty string or log an error
		hostname.clear();
	}

	// Create connection
	hConnect = WinHttpConnect(hSession, hostname.c_str(), urlComp.nPort, 0);
	if (!hConnect) {
		goto cleanup;
	}

	// Extract path
	if (urlComp.lpszUrlPath && urlComp.dwUrlPathLength > 0) {
		urlPath.assign(urlComp.lpszUrlPath, urlComp.dwUrlPathLength);
	} else {
		// Handle the error case, e.g., set urlPath to an empty string or log an error
		urlPath.clear();
	}

	// Create request
	flags = (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
	hRequest = WinHttpOpenRequest(hConnect, L"GET", urlPath.c_str(), nullptr, WINHTTP_NO_REFERER,
								  WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
	if (!hRequest) {
		goto cleanup;
	}

	// Disable SSL verification if requested (equivalent to curl -k)
	if (!sslVerify && (urlComp.nScheme == INTERNET_SCHEME_HTTPS)) {
		DWORD securityFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE |
							  SECURITY_FLAG_IGNORE_CERT_CN_INVALID | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;

		WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &securityFlags, sizeof(securityFlags));
	}

	// Add authentication if provided
	if (!username.empty() && !password.empty()) {
		WCHAR wideUser[256] = {}, widePass[256] = {};
		const int userLen =
			MultiByteToWideChar(CP_UTF8, 0, username.c_str(), -1, wideUser, sizeof(wideUser) / sizeof(WCHAR));
		const int passLen =
			MultiByteToWideChar(CP_UTF8, 0, password.c_str(), -1, widePass, sizeof(widePass) / sizeof(WCHAR));

		if (userLen > 0 && passLen > 0) {
			WinHttpSetCredentials(hRequest, WINHTTP_AUTH_TARGET_SERVER, WINHTTP_AUTH_SCHEME_BASIC, wideUser, widePass,
								  nullptr);
		}

		// Best-effort: scrub local stack copies only; WinHTTP-owned internal copies are not reachable
		SecureZeroMemory(wideUser, sizeof(wideUser));
		SecureZeroMemory(widePass, sizeof(widePass));
	}

	// Send request
	if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
		goto cleanup;
	}

	// Receive response
	if (!WinHttpReceiveResponse(hRequest, nullptr)) {
		goto cleanup;
	}

	// Get status code
	statusCodeSize = sizeof(statusCode);
	if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
							WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusCodeSize, nullptr)) {
		response.statusCode = statusCode;
	}

	// Read response data
	buffer.clear(); // Clear the buffer before use
	do {
		if (!WinHttpQueryDataAvailable(hRequest, &bytesAvailable)) {
			break;
		}

		if (bytesAvailable > 0) {
			std::vector<char> tempBuffer(bytesAvailable);
			DWORD bytesRead = 0;

			if (WinHttpReadData(hRequest, tempBuffer.data(), bytesAvailable, &bytesRead)) {
				buffer.insert(buffer.end(), tempBuffer.begin(), tempBuffer.begin() + bytesRead);
			}
		}
	} while (bytesAvailable > 0);

	// Copy response data
	if (!buffer.empty()) {
		response.responseData.assign(buffer.begin(), buffer.end());
	}

	result = HTTP_SUCCESS;

cleanup:
	if (hRequest)
		WinHttpCloseHandle(hRequest);
	if (hConnect)
		WinHttpCloseHandle(hConnect);
	if (hSession)
		WinHttpCloseHandle(hSession);

	return result;
}

/**
 * @brief Clean up HTTP response data (Windows implementation)
 *
 * Releases memory allocated for HTTP response data and resets the response structure.
 * Should be called after processing response data to prevent memory leaks.
 *
 * @param response Reference to HttpResponse structure to clean up
 *
 * @note Safe to call multiple times on the same response
 * @note Clears responseData string content
 * @note Windows implementation using std::string clear method
 */
void HttpClient::cleanupResponse(HttpResponse &response)
{
	response.responseData.clear();
	response.statusCode = 0;
}
