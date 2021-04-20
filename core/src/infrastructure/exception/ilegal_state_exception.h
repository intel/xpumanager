#include "base_exception.h"

class IlegalStateException : public BaseException {
 public:
  IlegalStateException(const std::string& msg): BaseException(msg) {

  }

  IlegalStateException(ErrorCode code, const std::string& msg): BaseException(code, msg) {

  }
};