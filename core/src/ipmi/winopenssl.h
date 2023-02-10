/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file winopenssl.h
 */

// https://docs.huihoo.com/doxygen/openssl/1.0.1c/crypto_2sha_2sha_8h_source.html

namespace xpum {
#define SHA_LBLOCK 16
#define SHA256_DIGEST_LENGTH 32

#define SHA_LONG unsigned int

typedef struct SHA256state_st {
    SHA_LONG h[8];
    SHA_LONG Nl, Nh;
    SHA_LONG data[SHA_LBLOCK];
    unsigned int num, md_len;
} SHA256_CTX;
} // namespace xpum
