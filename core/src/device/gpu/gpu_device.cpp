#include "logger.h"
#include "gpu_device_stub.h"
#include "gpu_device.h"
#include "device_property.h"

#include "api_types.h"
#include "unistd.h"
#include "stdio.h"
#include <fstream>

GPUDevice::GPUDevice() : commandExec( nullptr ) {
  Logger::instance().info("GPUDevice()");
}

GPUDevice::GPUDevice(const std::string& id,
                     const zes_device_handle_t& zes_device,
                     std::vector<DeviceCapability>& capabilities) : commandExec( nullptr ) {
  this->id = id;
  this->zes_device_handle = zes_device;                       
  for (DeviceCapability& cap : capabilities) {
    this->capabilities.push_back(cap);
  }
}

GPUDevice::~GPUDevice() {
  Logger::instance().info("~GPUDevice()");
}

void GPUDevice::getPower(Callback_t callback) noexcept {
  GPUDeviceStub::instance().getPower(zes_device_handle, 
    [callback](std::shared_ptr<void> ret, std::shared_ptr<BaseException> e) {
    callback(ret, e);
  });    
}

void GPUDevice::getActuralFrequency(Callback_t callback) noexcept {
  GPUDeviceStub::instance().getActuralFrequency(zes_device_handle, 
    [callback](std::shared_ptr<void> ret, std::shared_ptr<BaseException> e) {
    callback(ret, e);
  });    
}

void GPUDevice::getTemperature(Callback_t callback) noexcept {
  GPUDeviceStub::instance().getTemperature(zes_device_handle, 
    [callback](std::shared_ptr<void> ret, std::shared_ptr<BaseException> e) {
    callback(ret, e);
  });    
}

void GPUDevice::getMemory(Callback_t callback) noexcept {
  GPUDeviceStub::instance().getMemory(zes_device_handle, 
    [callback](std::shared_ptr<void> ret, std::shared_ptr<BaseException> e) {
    callback(ret, e);
  });    
}

void GPUDevice::getEngineUtilization(Callback_t callback) noexcept {
  GPUDeviceStub::instance().getEngineUtilization(zes_device_handle, 
    [callback](std::shared_ptr<void> ret, std::shared_ptr<BaseException> e) {
    callback(ret, e);
  });    
}

bool GPUDevice::runFirmwareFlash( const char* filePath, const std::string& toolPath ) noexcept {
    Property pcieAddrProp;
    bool res = getProperty( DeviceProperty::BDF_ADDRESS, pcieAddrProp );
    if ( !res ) {
        return false;
    }

    //remove first "0000"
    std::string address = pcieAddrProp.getValue();
    std::string::size_type begin = address.find( ":" );
    if ( begin == std::string::npos ) {
        return false;
    }

    std::string addrForTool = address.substr( begin + 1, address.length() );

    //change last "." to ":"
    begin = addrForTool.find( "." );
    if ( begin == std::string::npos ) {
        return false;
    }
    addrForTool[begin] = ':';

    std::string command = toolPath + " -Device " + addrForTool + " -F " + filePath;

    std::lock_guard<std::mutex> lck( mtx );
    if ( commandExec != nullptr ) {
        return false;
    }
    else {
        commandExec= popen( command.c_str(), "r" );
        return true;
    }
}

const std::string GPUDevice::logFilePath = "/tmp/gfx";

void GPUDevice::dumpFirmwareFlashLog() noexcept {
	std::ofstream logFile( logFilePath, std::ios_base::out | std::ios_base::trunc );
	if ( logFile.is_open() ) {
		logFile << log;
	}
	logFile.close();
}

xpum_firmware_flash_result_t GPUDevice::getFirmwareFlashResult() noexcept {
    xpum_firmware_flash_result_t rc { XPUM_DEVICE_FIRMWARE_FLASH_OK };

    if ( commandExec != nullptr ) {
        char buf[BUFFERSIZE];
        char* res = fgets( buf, BUFFERSIZE, commandExec );
        if ( res != nullptr ) {
            log += std::string{ res };
        }
        else {
            /*
            if ( log.find( "FPT Operation Successful" ) != string::npos ) {
                rc = Flash_result_t::OK;
            }
            else {
                rc = Flash_result_t::ERROR;
            }
            */
        	dumpFirmwareFlashLog();
            log.clear();
            if ( pclose( commandExec ) == 0 ) {
              rc = XPUM_DEVICE_FIRMWARE_FLASH_OK;
            }
            else {
              rc = XPUM_DEVICE_FIRMWARE_FLASH_ERROR;
            }
            commandExec = nullptr;
            return rc;
        }

        res = fgets( buf, BUFFERSIZE, commandExec );
        if ( res != nullptr ) {
            log += std::string{ res };
            return XPUM_DEVICE_FIRMWARE_FLASH_ONGOING;
        }
        else {
            /*
            if ( res.find( "FPT Operation Successful" ) != string::npos ) {
                rc = Flash_result_t::OK;
            }
            else {
                rc = Flash_result_t::ERROR;
            }
            */
        	dumpFirmwareFlashLog();
            log.clear();
            if ( pclose( commandExec ) == 0 ) {
              rc = XPUM_DEVICE_FIRMWARE_FLASH_OK;
            }
            else {
              rc = XPUM_DEVICE_FIRMWARE_FLASH_ERROR;
            }
            commandExec = nullptr;
            return rc;
        }
    }
    else {
        return XPUM_DEVICE_FIRMWARE_FLASH_OK;
    }
}
