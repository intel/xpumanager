#pragma once

#include "comlet_base.h"
#include <string>

#define CHAR_PTR(str) ((std::string)(str)).c_str()
#define MAKE_COMLET_PTR(comlet_type) (std::static_pointer_cast<ComletBase>(std::make_shared<comlet_type>()))

void testComlet(std::shared_ptr<ComletBase> comlet, const char *expected, std::initializer_list<std::string> parameters);