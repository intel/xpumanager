#pragma once

#include <string>
#include <mutex>

class PciIds {
    public:
      static PciIds& instance();

      void get(){
          return;
      };

    private:
      PciIds();
      ~PciIds();

      PciIds& operator=(const PciIds &) = delete;
      PciIds(const PciIds &) = delete;

      bool init();

      bool bInitialized;
      std::mutex mutex;
};