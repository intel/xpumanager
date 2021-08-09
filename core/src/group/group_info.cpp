#include "logger.h"
#include "group_info.h"

GroupInfo::GroupInfo(char * name, xpum_group_id_t groupId)
{
    Logger::instance().info("GroupInfo");
    name = name;
    id = groupId;
}

GroupInfo::~GroupInfo()
{
    Logger::instance().info("~GroupInfo");
    deviceList.clear();
}