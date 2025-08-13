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
