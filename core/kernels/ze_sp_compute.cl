/*
 *
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#undef MAD_4
#undef MAD_16
#undef MAD_64

#define MAD_4(x, y)     x = mad(y, x, y);   y = mad(x, y, x);   x = mad(y, x, y);   y = mad(x, y, x);
#define MAD_16(x, y)    MAD_4(x, y);        MAD_4(x, y);        MAD_4(x, y);        MAD_4(x, y);
#define MAD_64(x, y)    MAD_16(x, y);       MAD_16(x, y);       MAD_16(x, y);       MAD_16(x, y);

__kernel void compute_sp_v1(__global float *input_value, __global float *output)
{
    float x = input_value[0];
    float y = (float)get_local_id(0);

    for (int i = 0; i < 128; i++)
    {
        MAD_16(x, y);
    }

    output[get_global_id(0)] = y;
}

__kernel void compute_sp_v2(__global float *input_value, __global float *output)
{
    float2 x = (float2)(input_value[0], (input_value[0]+1));
    float2 y = (float2)get_local_id(0);

    for (int i = 0; i < 64; i++)
    {
        MAD_16(x, y);
    }

    output[get_global_id(0)] = (y.S0) + (y.S1);
}

__kernel void compute_sp_v4(__global float *input_value, __global float *output)
{
    float4 x = (float4)(input_value[0], (input_value[0]+1), (input_value[0]+2), (input_value[0]+3));
    float4 y = (float4)get_local_id(0);

    for(int i = 0; i < 32; i++)
    {
        MAD_16(x, y);
    }

    output[get_global_id(0)] = (y.S0) + (y.S1) + (y.S2) + (y.S3);
}


__kernel void compute_sp_v8(__global float *input_value, __global float *output)
{
    float8 x = (float8)(input_value[0], (input_value[0]+1), (input_value[0]+2), (input_value[0]+3), (input_value[0]+4), (input_value[0]+5), (input_value[0]+6), (input_value[0]+7));
    float8 y = (float8)get_local_id(0);

    for(int i = 0; i < 16; i++)
    {
        MAD_16(x, y);
    }


    output[get_global_id(0)] = (y.S0) + (y.S1) + (y.S2) + (y.S3) + (y.S4) + (y.S5) + (y.S6) + (y.S7);
}

__kernel void compute_sp_v16(__global float *input_value, __global float *output)
{
    float16 x = (float16)(input_value[0], (input_value[0]+1), (input_value[0]+2), (input_value[0]+3), (input_value[0]+4), (input_value[0]+5), (input_value[0]+6), (input_value[0]+7),
                    (input_value[0]+8), (input_value[0]+9), (input_value[0]+10), (input_value[0]+11), (input_value[0]+12), (input_value[0]+13), (input_value[0]+14), (input_value[0]+15));
    float16 y = (float16)get_local_id(0);

    for(int i = 0; i < 8; i++)
    {
        MAD_16(x, y);
    }

    float2 t = (y.S01) + (y.S23) + (y.S45) + (y.S67) + (y.S89) + (y.SAB) + (y.SCD) + (y.SEF);
    output[get_global_id(0)] = t.S0 + t.S1;
}
