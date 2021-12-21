#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <climits>

#include "comlet_base.h"

namespace xpum::cli {
    struct FlashFirmwareOptions {
        int deviceId;
        std::string firmwareType;
        std::string firmwarePath;

        /*
        FlashFirmwareOptions( unsigned int id, const std::string& type, const std::string& path )
            : deviceId( id ), firmwarePath( path ) {
            if ( type == "GSC" ) {
                firmwareType = 0;
            }
            else {
                firmwareType = 0;
            }
        }
        */
    };

    class ComletFirmware : public ComletBase {
        public:
            ComletFirmware();
            virtual ~ComletFirmware();

            virtual void setupOptions() override;
            virtual std::unique_ptr<nlohmann::json> run() override;
            virtual void getTableResult(std::ostream &out) override;
        
        private:
            std::unique_ptr<FlashFirmwareOptions> opts;
    };
}