/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file data_logic_interface.h
 */

#pragma once

#include "data_logic_persistence_interface.h"
#include "data_logic_query_interface.h"
#include "infrastructure/init_close_interface.h"

namespace xpum {

class DataLogicInterface : public InitCloseInterface,
                           public DataLogicQueryInterface,
                           public DataLogicPersistenceInterface {
   public:
    virtual ~DataLogicInterface(){};
};

} // end namespace xpum
