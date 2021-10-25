#pragma once

#include <vector>

namespace xpum {

class InitCloseInterface {
 public:
  virtual ~InitCloseInterface() {};

  virtual void init() = 0;

  virtual void close() = 0;

};

} // end namespace xpum
