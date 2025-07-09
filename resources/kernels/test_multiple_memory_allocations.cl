/*
 *
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

kernel void test_device_memory1(__global char *src, __global char *dst) {
    int  tid = get_global_id(0);
    dst[tid] = src[tid];
}
kernel void test_device_memory2(__global char *src, __global char *dst) {
    int  tid = get_global_id(0);
    dst[tid] = src[tid];
}
kernel void test_device_memory3(__global char *src, __global char *dst) {
    int  tid = get_global_id(0);
    dst[tid] = src[tid];
}
kernel void test_device_memory4(__global char *src, __global char *dst) {
    int  tid = get_global_id(0);
    dst[tid] = src[tid];
}
kernel void test_device_memory5(__global char *src, __global char *dst) {
    int  tid = get_global_id(0);
    dst[tid] = src[tid];
}
kernel void test_device_memory6(__global char *src, __global char *dst) {
    int  tid = get_global_id(0);
    dst[tid] = src[tid];
}
kernel void test_device_memory7(__global char *src, __global char *dst) {
    int  tid = get_global_id(0);
    dst[tid] = src[tid];
}
kernel void test_device_memory8(__global char *src, __global char *dst) {
    int  tid = get_global_id(0);
    dst[tid] = src[tid];
}
kernel void test_device_memory9(__global char *src, __global char *dst) {
    int  tid = get_global_id(0);
    dst[tid] = src[tid];
}
kernel void test_device_memory10(__global char *src, __global char *dst) {
    int  tid = get_global_id(0);
    dst[tid] = src[tid];
}