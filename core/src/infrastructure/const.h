#pragma once

#include <functional>
#include <memory>

#include "infrastructure/exception/base_exception.h"

namespace xpum {

typedef std::function<void(std::shared_ptr<void>, std::shared_ptr<BaseException>)> Callback_t;

typedef long long Timestamp_t;

// template <typename T, typename... Args>
// std::unique_ptr<T> make_unique(Args&&... args) {
//     return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
// }

} // end namespace xpum
