#pragma once

#include <string>

class Logger {
 public:
  static Logger& instance();

 public:
  void error(const std::string& msg);

  void warn(const std::string& msg);

  void info(const std::string& msg);

  void debug(const std::string& msg);
  
 private:
  std::string getCurrentSystemTime();

  Logger() = default;

  Logger& operator=(const Logger &) = delete;

  Logger(const Logger &other) = delete;
  
};
