#include "base_exception.h"

class IlegalParameterException : public BaseException {
 public:
  IlegalParameterException(const std::string& msg): BaseException(msg) {

  }

  IlegalParameterException(ErrorCode code, const std::string& msg): BaseException(code, msg) {

  }  
};