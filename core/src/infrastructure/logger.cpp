#include <iostream>
#include <ctime>
#include <ratio>
#include <chrono>

#include "utility.h"
#include "logger.h"

Logger& Logger::instance() {
  static Logger instance;
  return instance;
}

void Logger::error(const std::string& msg){
  std::cout << Utility::getCurrentTimeString() << " " << msg << std::endl;
}

void Logger::warn(const std::string& msg){
  std::cout << Utility::getCurrentTimeString() << " " << msg << std::endl;
}

void Logger::info(const std::string& msg){
  std::cout << Utility::getCurrentTimeString() << " " << msg << std::endl;
}

void Logger::debug(const std::string& msg){
  std::cout << Utility::getCurrentTimeString() << " " << msg << std::endl;
}

