/*
 *  Copyright (C) 2021-2022 Intel Corporation
 *
 *  Intel Simplified Software License (Version August 2021)
 *
 *  Use and Redistribution.  You may use and redistribute the software (the “Software”), without modification, provided the following conditions are met:
 *
 *  * Redistributions must reproduce the above copyright notice and the
 *    following terms of use in the Software and in the documentation and/or other materials provided with the distribution.
 *  * Neither the name of Intel nor the names of its suppliers may be used to
 *    endorse or promote products derived from this Software without specific
 *    prior written permission.
 *  * No reverse engineering, decompilation, or disassembly of this Software
 *    is permitted.
 *
 *  No other licenses.  Except as provided in the preceding section, Intel grants no licenses or other rights by implication, estoppel or otherwise to, patent, copyright, trademark, trade name, service mark or other intellectual property licenses or rights of Intel.
 *
 *  Third party software.  The Software may contain Third Party Software. “Third Party Software” is open source software, third party software, or other Intel software that may be identified in the Software itself or in the files (if any) listed in the “third-party-software.txt” or similarly named text file included with the Software. Third Party Software, even if included with the distribution of the Software, may be governed by separate license terms, including without limitation, open source software license terms, third party software license terms, and other Intel software license terms. Those separate license terms solely govern your use of the Third Party Software, and nothing in this license limits any rights under, or grants rights that supersede, the terms of the applicable license terms.
 *
 *  DISCLAIMER.  THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT ARE DISCLAIMED. THIS SOFTWARE IS NOT INTENDED FOR USE IN SYSTEMS OR APPLICATIONS WHERE FAILURE OF THE SOFTWARE MAY CAUSE PERSONAL INJURY OR DEATH AND YOU AGREE THAT YOU ARE FULLY RESPONSIBLE FOR ANY CLAIMS, COSTS, DAMAGES, EXPENSES, AND ATTORNEYS’ FEES ARISING OUT OF ANY SUCH USE, EVEN IF ANY CLAIM ALLEGES THAT INTEL WAS NEGLIGENT REGARDING THE DESIGN OR MANUFACTURE OF THE SOFTWARE.
 *
 *  LIMITATION OF LIABILITY. IN NO EVENT WILL INTEL BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  No support.  Intel may make changes to the Software, at any time without notice, and is not obligated to support, update or provide training for the Software.
 *
 *  Termination. Your right to use the Software is terminated in the event of your breach of this license.
 *
 *  Feedback.  Should you provide Intel with comments, modifications, corrections, enhancements or other input (“Feedback”) related to the Software, Intel will be free to use, disclose, reproduce, license or otherwise distribute or exploit the Feedback in its sole discretion without any obligations or restrictions of any kind, including without limitation, intellectual property rights or licensing obligations.
 *
 *  Compliance with laws.  You agree to comply with all relevant laws and regulations governing your use, transfer, import or export (or prohibition thereof) of the Software.
 *
 *  Governing law.  All disputes will be governed by the laws of the United States of America and the State of Delaware without reference to conflict of law principles and subject to the exclusive jurisdiction of the state or federal courts sitting in the State of Delaware, and each party agrees that it submits to the personal jurisdiction and venue of those courts and waives any objections. The United Nations Convention on Contracts for the International Sale of Goods (1980) is specifically excluded and will not apply to the Software.
 *
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
