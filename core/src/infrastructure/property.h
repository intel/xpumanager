#pragma once

#include <string>

#include "../include/xpum_structs.h"

class Property {
 public:
  Property() {}

  Property(const std::string& name, const std::string& value) : name(name), value(value){
  }

  Property(const std::string& name, int value) : name(name) {
    this->value = std::to_string(value);
  }

  Property(const std::string& name, long value) : name(name) {
    this->value = std::to_string(value);
  }

  Property(const std::string& name, bool value) : name(name) {
    int int_value = value ? 1 : 0;
    this->value = std::to_string(int_value);
  }

  Property(const std::string& name, float value) : name(name) {
    this->value = std::to_string(value);
  }

  Property(const std::string& name, double value) : name(name) {
    this->value = std::to_string(value);
  }

 public:
  std::string& getName() {
    return name;
  } 

  std::string& getValue()  {
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

  void setName(const std::string& name) {
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
  std::string name;

  std::string value;

};
