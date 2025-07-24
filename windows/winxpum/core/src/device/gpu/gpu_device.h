/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file gpu_device.h
 */

#pragma once

#include "device/device.h"
#include "level_zero/zes_api.h"

namespace xpum {
	/*
	  GPUDevice class defines various interfaces for communication with GPU devices.
	*/

	class GPUDevice : public Device {
	public:
		GPUDevice();
		
		GPUDevice(const std::string& id, const zes_device_handle_t& zes_device, std::vector<DeviceCapability>& capabilities);

		GPUDevice(const std::string& id, const zes_device_handle_t& zes_device, const ze_driver_handle_t& driver, std::vector<DeviceCapability>& capabilities);

		void getPower(std::shared_ptr<MeasurementData>& data) noexcept override;

		void getActuralRequestFrequency(std::shared_ptr<MeasurementData>& data, MeasurementType type) noexcept override;

		void getTemperature(std::shared_ptr<MeasurementData>& data, MeasurementType type) noexcept override;

		void getMemoryUsedUtilization(std::shared_ptr<MeasurementData>& data, MeasurementType type) noexcept override;

		void getMemoryBandwidth(std::shared_ptr<MeasurementData>& data) noexcept override;

		void getMemoryReadWrite(std::shared_ptr<MeasurementData>& data, MeasurementType type) noexcept override;

		void getEngineUtilization(std::shared_ptr<MeasurementData>& data) noexcept override;

		void getGPUUtilization(std::shared_ptr<MeasurementData>& data) noexcept override;

		void getEngineGroupUtilization(std::shared_ptr<MeasurementData>& data, MeasurementType type) noexcept override;

		void getEnergy(std::shared_ptr<MeasurementData>& data) noexcept override;

		void getEuActiveStallIdle(std::shared_ptr<MeasurementData>& data, MeasurementType type) noexcept override;

		void getRasError(std::shared_ptr<MeasurementData>& data, MeasurementType type) noexcept override;

		void getFrequencyThrottle(std::shared_ptr<MeasurementData>& data) noexcept override;

		void getFrequencyThrottleReason(std::shared_ptr<MeasurementData>& data) noexcept override;

		void getPCIeReadThroughput(std::shared_ptr<MeasurementData>& data) noexcept override;

		void getPCIeWriteThroughput(std::shared_ptr<MeasurementData>& data) noexcept override;

		void getPCIeRead(std::shared_ptr<MeasurementData>& data) noexcept override;

		void getPCIeWrite(std::shared_ptr<MeasurementData>& data) noexcept override;

		void getFabricThroughput(std::shared_ptr<MeasurementData>& data) noexcept override;

		void getPerfMetrics(std::shared_ptr<MeasurementData>& data) noexcept override;

		void getDeviceSusPower(int32_t& susPower, bool& supported) noexcept override;

		bool setDevicePowerSustainedLimits(int32_t powerLimit) noexcept override;

		void getDevicePowerMaxLimit(int32_t& max_limit, bool& supported) noexcept override;

        void getSimpleEccState(uint8_t& current, uint8_t& pending) noexcept override;

        bool getEccState(MemoryEcc& ecc) noexcept override;

		void getDeviceFrequencyRange(int32_t tileId, double& min, double& max, std::string& clocks, bool& supported) noexcept override;

		bool setDeviceFrequencyRange(int32_t tileId, double min, double max) noexcept override;

        void getFreqAvailableClocks(int32_t tileId, std::vector<double>& clocksList) noexcept override;

		std::map<MeasurementType, std::shared_ptr<MeasurementData>> getRealtimeMetrics() noexcept override;

		virtual ~GPUDevice();
	
	private:
		zes_device_handle_t zes_device_handle = nullptr;

		ze_driver_handle_t ze_driver_handle = nullptr;
	};
} // end namespace xpum