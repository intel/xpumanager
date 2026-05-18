/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include <http_client.h>
#include <debug.h>
#include <curl/curl.h>
#include <string>
#include <string.h>
#include <vector>

/**
 * @brief Callback function to write HTTP response data
 *
 * CURL callback function that accumulates response data into a vector buffer.
 * Called by libcurl as data is received from the HTTP response.
 *
 * @param contents Pointer to received data chunk
 * @param size Size of each data element
 * @param nmemb Number of data elements
 * @param buffer Pointer to vector buffer to store the data
 *
 * @return Size of data processed (size * nmemb)
 *
 * @note This is a static callback function used internally by CURL
 */
static size_t writeCallback(void *contents, size_t size, size_t nmemb, std::vector<char> *buffer)
{
	size_t totalSize = size * nmemb;
	buffer->insert(buffer->end(), (char *)contents, (char *)contents + totalSize);
	return totalSize;
}

/**
 * @brief Constructor for HttpClient class
 *
 * Initializes the HTTP client with default configuration settings.
 * Sets up libcurl global state and default timeout/SSL verification settings.
 *
 * @note Initializes default timeout and disables SSL verification by default
 * @note Calls curl_global_init() for libcurl initialization
 */
HttpClient::HttpClient()
{
	timeoutSeconds = HTTP_DEFAULT_TIMEOUT;
	sslVerify = false; // Equivalent to curl -k
	curl_global_init(CURL_GLOBAL_DEFAULT);
}

/**
 * @brief Destructor for HttpClient class
 *
 * Performs cleanup of libcurl global state and resources.
 * Should be called when the HTTP client is no longer needed.
 *
 * @note Calls curl_global_cleanup() to release libcurl resources
 */
HttpClient::~HttpClient() { curl_global_cleanup(); }

/**
 * @brief Perform HTTP GET request without authentication
 *
 * Executes an HTTP GET request to the specified URL without authentication credentials.
 * Uses the current timeout and SSL verification settings.
 *
 * @param url Target URL for the HTTP GET request
 * @param response Reference to HttpResponse structure to store the response
 *
 * @return Status of the HTTP request
 * @retval HTTP_SUCCESS Request completed successfully
 * @retval HTTP_FAILURE Request failed due to network or protocol error
 * @retval HTTP_TIMEOUT Request timed out
 *
 * @note Response data must be freed using cleanup_response()
 */
int HttpClient::get(const std::string &url, HttpResponse &response) { return performRequest(url, "", "", response); }

/**
 * @brief Perform HTTP GET request with basic authentication
 *
 * Executes an HTTP GET request to the specified URL using basic authentication
 * with the provided username and password credentials.
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
 * @note Response data must be freed using cleanup_response()
 * @note Uses HTTP basic authentication (base64 encoded credentials)
 */
int HttpClient::getWithAuth(const std::string &url, const std::string &username, const std::string &password,
							HttpResponse &response)
{
	return performRequest(url, username, password, response);
}

/**
 * @brief Set HTTP request timeout value
 *
 * Configures the timeout duration for HTTP requests. Requests that exceed
 * this timeout will be cancelled and return HTTP_TIMEOUT status.
 *
 * @param timeout_sec Timeout duration in seconds
 *
 * @note Timeout applies to all subsequent HTTP requests
 */
void HttpClient::setTimeout(int timeoutSec) { timeoutSeconds = timeoutSec; }

/**
 * @brief Configure SSL certificate verification
 *
 * Enables or disables SSL certificate verification for HTTPS requests.
 * When disabled, equivalent to using 'curl -k' (insecure mode).
 *
 * @param verify true to enable SSL verification, false to disable
 *
 * @note Default is false (SSL verification disabled)
 * @note Disabling SSL verification reduces security but may be needed for self-signed certificates
 */
void HttpClient::setSslVerify(bool verify) { sslVerify = verify; }

/**
 * @brief Perform HTTP request with optional authentication
 *
 * Internal function that performs the actual HTTP request using libcurl.
 * Handles authentication, SSL settings, timeouts, and response data collection.
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
 * @note Response data is dynamically allocated and must be freed
 * @note Follows HTTP redirects automatically
 * @note Sets user agent to "AMC Redfish Client/1.0"
 */
int HttpClient::performRequest(const std::string &url, const std::string &username, const std::string &password,
							   HttpResponse &response)
{
	CURL *curl;
	CURLcode res;
	std::vector<char> buffer;
	std::string authString;

	// Initialize response
	response.statusCode = 0;
	response.responseData.clear();

	curl = curl_easy_init();
	if (!curl) {
		ERR("ERROR: Failed to initialize CURL\n");
		return HTTP_FAILURE;
	}

	// Set URL
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

	// Set timeout
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeoutSeconds);

	// Disable SSL verification if requested (equivalent to curl -k)
	if (!sslVerify) {
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
	}

	// Set authentication if provided
	if (!username.empty() && !password.empty()) {
		authString = username + ":" + password;
		curl_easy_setopt(curl, CURLOPT_USERPWD, authString.c_str());
		curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
	}

	// Set callback function to capture response
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);

	// Follow redirects
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

	// Set user agent
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "AMC Redfish Client/1.0");

	// Perform the request
	res = curl_easy_perform(curl);

	// Best-effort: scrub local auth string; libcurl-owned internal copies are not reachable
	if (!authString.empty()) {
		explicit_bzero(authString.data(), authString.size());
		authString.clear();
	}

	if (res == CURLE_OK) {
		// Get response code
		long responseCode;
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
		response.statusCode = (int)responseCode;

		// Copy response data
		if (!buffer.empty()) {
			response.responseData.assign(buffer.begin(), buffer.end());
		}

		curl_easy_cleanup(curl);
		return HTTP_SUCCESS;
	} else {
		curl_easy_cleanup(curl);

		// Print detailed error information for debugging
		ERR("CURL Error: {} (code: {})\n", curl_easy_strerror(res), res);

		// Map curl errors to our error codes
		switch (res) {
		case CURLE_UNSUPPORTED_PROTOCOL:
			ERR("ERROR: Unsupported protocol. Check if HTTPS/SSL support is available.\n");
			return HTTP_FAILURE;
		case CURLE_OPERATION_TIMEDOUT:
			return HTTP_TIMEOUT;
		case CURLE_HTTP_RETURNED_ERROR:
			return HTTP_NOT_FOUND;
		default:
			return HTTP_FAILURE;
		}
	}
}

/**
 * @brief Clean up HTTP response data
 *
 * Releases memory allocated for HTTP response data and resets the response structure.
 * Should be called after processing response data to prevent memory leaks.
 *
 * @param response Reference to HttpResponse structure to clean up
 *
 * @note Clears responseData string content
 */
void HttpClient::cleanupResponse(HttpResponse &response)
{
	response.responseData.clear();
	response.statusCode = 0;
}
