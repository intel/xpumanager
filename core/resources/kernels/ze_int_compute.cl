/*
 *
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#undef MAD_16
#undef MAD_64

#define MAD_4(x, y)     x = (y*x) + y;      y = (x*y) + x;      x = (y*x) + y;      y = (x*y) + x;
#define MAD_16(x, y)    MAD_4(x, y);        MAD_4(x, y);        MAD_4(x, y);        MAD_4(x, y);
#define MAD_64(x, y)    MAD_16(x, y);       MAD_16(x, y);       MAD_16(x, y);       MAD_16(x, y);

__kernel void compute_int_v1(__global int *input_value, __global int *output)
{
    int x = input_value[0];
    int y = (int)get_local_id(0);

    MAD_64(x, y);   MAD_64(x, y);
    MAD_64(x, y);   MAD_64(x, y);
    MAD_64(x, y);   MAD_64(x, y);
    MAD_64(x, y);   MAD_64(x, y);
    MAD_64(x, y);   MAD_64(x, y);
    MAD_64(x, y);   MAD_64(x, y);
    MAD_64(x, y);   MAD_64(x, y);
    MAD_64(x, y);   MAD_64(x, y);

    output[get_global_id(0)] = y;
}


__kernel void compute_int_v2(__global int *input_value, __global int *output)
{
    int2 x = (int2)(input_value[0], (input_value[0]+1));
    int2 y = (int2)get_local_id(0);

    MAD_64(x, y);   MAD_64(x, y);
    MAD_64(x, y);   MAD_64(x, y);
    MAD_64(x, y);   MAD_64(x, y);
    MAD_64(x, y);   MAD_64(x, y);

    output[get_global_id(0)] = (y.S0) + (y.S1);
}

__kernel void compute_int_v4(__global int *input_value, __global int *output)
{
    int4 x = (int4)(input_value[0], (input_value[0]+1), (input_value[0]+2), (input_value[0]+3));
    int4 y = (int4)get_local_id(0);

    MAD_64(x, y);
    MAD_64(x, y);
    MAD_64(x, y);
    MAD_64(x, y);

    output[get_global_id(0)] = (y.S0) + (y.S1) + (y.S2) + (y.S3);
}


__kernel void compute_int_v8(__global int *input_value, __global int *output)
{
    int8 x = (int8)(input_value[0], (input_value[0]+1), (input_value[0]+2), (input_value[0]+3), (input_value[0]+4), (input_value[0]+5), (input_value[0]+6), (input_value[0]+7));
    int8 y = (int8)get_local_id(0);

    MAD_64(x, y);
    MAD_64(x, y);

    output[get_global_id(0)] = (y.S0) + (y.S1) + (y.S2) + (y.S3) + (y.S4) + (y.S5) + (y.S6) + (y.S7);
}

__kernel void compute_int_v16(__global int *input_value, __global int *output)
{
    int16 x = (int16)(input_value[0], (input_value[0]+1), (input_value[0]+2), (input_value[0]+3), (input_value[0]+4), (input_value[0]+5), (input_value[0]+6), (input_value[0]+7),
                    (input_value[0]+8), (input_value[0]+9), (input_value[0]+10), (input_value[0]+11), (input_value[0]+12), (input_value[0]+13), (input_value[0]+14), (input_value[0]+15));
    int16 y = (int16)get_local_id(0);

    MAD_64(x, y);

    int2 t = (y.S01) + (y.S23) + (y.S45) + (y.S67) + (y.S89) + (y.SAB) + (y.SCD) + (y.SEF);
    output[get_global_id(0)] = t.S0 + t.S1;
}
