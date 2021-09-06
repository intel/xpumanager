#pragma once

#include <comlet_base.h>
#include <map>

template <class T>
class ComletBase {

  public:
    ComletBase(std::string command) : command(command) {}
    virtual ~ComletBase() {}

    std::string getCommand() { return command; }
    
    void baseProcess();

    virtual void setupCommand(std::map<std::string, std::string> &opt) = 0;
    virtual void runCommand(T const &opt) = 0;

  private:
    std::string command;
};