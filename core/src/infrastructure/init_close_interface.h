#pragma once

#include <vector>

class InitCloseInterface {
 public:
  virtual ~InitCloseInterface() {};

  virtual void init() = 0;

  virtual void close() = 0;

};
