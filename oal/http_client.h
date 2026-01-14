/*
 * Copyright (C) 2025 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _HTTP_CLIENT_H
#define _HTTP_CLIENT_H

#include <stdint.h>
#include <cstddef>
#include <string>

// HTTP client return codes
#define HTTP_SUCCESS 0
#define HTTP_FAILURE 1
#define HTTP_TIMEOUT 2
#define HTTP_AUTH_REQUIRED 3
#define HTTP_NOT_FOUND 4

// HTTP default timeout
#define HTTP_DEFAULT_TIMEOUT 30

// HTTP response structure
struct HttpResponse
{
	int statusCode;
	std::string responseData;

	HttpResponse() : statusCode(0) {}
};

class HttpClient
{
public:
	HttpClient();
	~HttpClient();

	// Main HTTP methods
	int get(const std::string &url, HttpResponse &response);
	int getWithAuth(const std::string &url, const std::string &username, const std::string &password,
					HttpResponse &response);

	// Utility methods
	void setTimeout(int timeoutSeconds);
	void setSslVerify(bool verify);

private:
	int timeoutSeconds;
	bool sslVerify;

	// Platform-specific implementation
	int performRequest(const std::string &url, const std::string &username, const std::string &password,
					   HttpResponse &response);
	void cleanupResponse(HttpResponse &response);
};

#endif // _HTTP_CLIENT_H
