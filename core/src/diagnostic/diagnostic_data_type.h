#pragma once
#include "../include/xpum_structs.h"

struct ZeWorkGroups {
    uint32_t groupCountX;              // number of thread groups in X dimension
    uint32_t groupCountY;              // number of thread groups in Y dimension
    uint32_t groupCountZ;              // number of thread groups in Z dimension
    uint32_t group_size_x;
    uint32_t group_size_y;
    uint32_t group_size_z;
};
