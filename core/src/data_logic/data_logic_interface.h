
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
