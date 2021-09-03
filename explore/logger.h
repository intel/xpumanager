#pragma once


class Logger {
  public:
   static Logger & instance() {
     static Logger log;
     return log;
   }
   
   void info(std::string A) {

   }
   void error(std::string A) {
     
   }
};