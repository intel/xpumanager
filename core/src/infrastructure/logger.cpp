#include <iostream>
#include <ctime>
#include <ratio>
#include <chrono>

#include "utility.h"
#include "logger.h"

#include "spdlog/spdlog.h"

Logger& Logger::instance() {
  static Logger instance;
  return instance;
}

void Logger::error(const std::string& msg){
  spdlog::error(msg);
}

void Logger::warn(const std::string& msg){
  spdlog::warn(msg);
}

void Logger::info(const std::string& msg){
  spdlog::info(msg);
}

void Logger::debug(const std::string& msg){
  spdlog::debug(msg);
}

