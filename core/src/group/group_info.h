#pragma once
#include <string>
#include <vector>

#include "xpum_structs.h"

class GroupInfo
{
    public:
        GroupInfo(char * name, xpum_group_id_t groupId);
        ~GroupInfo();

    private:
        xpum_group_id_t id;
        std::string name;
        std::vector<xpum_device_id_t> deviceList;
};