#include "base_exception.h"

namespace xpum {

class LevelZeroInitializationException : public BaseException {
   public:
    LevelZeroInitializationException(const std::string& msg) : BaseException(msg) {
    }

    LevelZeroInitializationException(ErrorCode code, const std::string& msg) : BaseException(code, msg) {
    }
};
} // end namespace xpum
