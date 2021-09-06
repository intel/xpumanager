#include <comlet_version.h>

#include <iostream>

void ComletVersionOption::debug() {
  std::cout << __FILE__ << ":" << __FUNCTION__ << ":" << __LINE__ << std::endl;
}

void ComletVersion::setupCommand(std::map<std::string, std::string> &opt) {
  std::cout << __FILE__ << ":" << __FUNCTION__ << ":" << __LINE__ << std::endl;
}

void ComletVersion::runCommand(ComletVersionOption const &opt) {
  std::cout << __FILE__ << ":" << __FUNCTION__ << ":" << __LINE__ << std::endl;
}