/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file file_utils.cpp
 */

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tool.h"
#include "winopenssl.h"

namespace xpum {
/*
 * On success return pointer to buffer with file content and read_size.
 * To read whole file content set read_size to 0.
 * To read part of file set read_size to require size.
 */
uint8_t *read_file(const char *path, size_t *read_size) {
    FILE *fd;
    uint8_t *buffer = NULL;
    size_t count;
    size_t buffer_size;

    assert(path);
    assert(read_size);

    fd = fopen(path, "rb");
    if (!fd) {
        XPUM_LOG_ERROR("Unable to open {}. errno: {}({})\n",
                       path, errno, strerror(errno));
        return NULL;
    }

    buffer_size = *read_size;
    if (buffer_size == 0) {
        if (fseek(fd, 0, SEEK_END)) {
            XPUM_LOG_ERROR("Unable to set position file indicator\n");
            goto error;
        }

        buffer_size = ftell(fd);
        if (buffer_size == 0) {
            XPUM_LOG_ERROR("File {} does not have any content\n", path);
            goto error;
        }

        if (fseek(fd, 0, SEEK_SET)) {
            XPUM_LOG_ERROR("Unable to set position file indicator\n");
            goto error;
        }
    }

    buffer = (uint8_t *)malloc(buffer_size);
    if (!buffer) {
        goto error;
    }

    count = fread(buffer, 1, buffer_size, fd);
    if (count != buffer_size) {
        XPUM_LOG_ERROR("Reading file %s failed\n", path);
        goto error;
    }

    fclose(fd);
    *read_size = buffer_size;
    return buffer;
error:
    free(buffer);
    fclose(fd);
    return NULL;
}

bool write_file(const char *path, const uint8_t *buffer, size_t buffer_size) {
    FILE *fd;
    size_t count;

    assert(path);
    assert(buffer);

    fd = fopen(path, "wb");
    if (!fd) {
        XPUM_LOG_ERROR("Unable to open {}. errno: {}({})\n",
                       path, errno, strerror(errno));
        return false;
    }

    count = fwrite(buffer, 1, buffer_size, fd);
    if (count != buffer_size) {
        XPUM_LOG_ERROR("Writing to file {} failed\n", path);
        fclose(fd);
        return false;
    }

    fclose(fd);
    return true;
}

bool compare_with_file(const char *path, const uint8_t *buffer, size_t buffer_size) {
    uint8_t *file_buffer;
    size_t file_size = buffer_size;

    assert(path);
    assert(buffer);

    file_buffer = read_file(path, &file_size);
    if (!file_buffer)
        return false;

    if (file_size != buffer_size) {
        free(file_buffer);
        return false;
    }

    if (memcmp(file_buffer, buffer, file_size)) {
        free(file_buffer);
        return false;
    }

    free(file_buffer);
    return true;
}
} // namespace xpum
