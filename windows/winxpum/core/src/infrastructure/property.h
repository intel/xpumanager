/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file property.h
 */

#pragma once

#include <string>

#include "api/api_types.h"

namespace xpum {

    class Property {
    public:
        Property() {}

        Property(xpum_device_internal_property_name_t name, const std::string& value) : name(name), value(value) {
        }

        Property(xpum_device_internal_property_name_t name, int value) : name(name) {
            this->value = std::to_string(value);
        }

        Property(xpum_device_internal_property_name_t name, long value) : name(name) {
            this->value = std::to_string(value);
        }

        Property(xpum_device_internal_property_name_t name, bool value) : name(name) {
            int int_value = value ? 1 : 0;
            this->value = std::to_string(int_value);
        }

        Property(xpum_device_internal_property_name_t name, float value) : name(name) {
            this->value = std::to_string(value);
        }

        Property(xpum_device_internal_property_name_t name, double value) : name(name) {
            this->value = std::to_string(value);
        }

    public:
        xpum_device_internal_property_name_t getName() {
            return name;
        }

        std::string& getValue() {
            return value;
        }

        bool getValueBool() {
            return std::stoi(value) == 1 ? true : false;
        }

        int getValueInt() {
            return std::stoi(value);
        }

        long getValueLong() {
            return std::stol(value);
        }

        int getValueFloat() {
            return std::stof(value);
        }

        int getValueDouble() {
            return std::stod(value);
        }

        void setName(xpum_device_internal_property_name_t name) {
            this->name = name;
        }

        void setValue(const std::string& value) {
            this->value = value;
        }

        void setValue(int value) {
            this->value = std::to_string(value);
        }

        void setValue(float value) {
            this->value = std::to_string(value);
        }

        void setValue(double value) {
            this->value = std::to_string(value);
        }

        void setValue(bool value) {
            int int_value = value ? 1 : 0;
            this->value = std::to_string(int_value);
        }

    private:
        xpum_device_internal_property_name_t name;

        std::string value;
    };

} // end namespace xpum
