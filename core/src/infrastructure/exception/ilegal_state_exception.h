#include "base_exception.h"

namespace xpum {

class IlegalStateException : public BaseException {
   public:
    IlegalStateException(const std::string& msg) : BaseException(msg) {
    }

    IlegalStateException(ErrorCode code, const std::string& msg) : BaseException(code, msg) {
    }
};
} // end namespace xpum
