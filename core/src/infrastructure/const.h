#pragma once

#include <functional>
#include <memory>

#include "base_exception.h"

typedef std::function<void(std::shared_ptr<void>, std::shared_ptr<BaseException>)> Callback_t;

typedef long long Timestamp_t;

template<typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}
