#include "handle_lock.h"

#include <memory>
#include <mutex>
#include <unordered_map>

namespace xpum {
std::mutex HandleLock::handle_mutexes_mutex;
std::unordered_map<void*, std::shared_ptr<std::mutex>> HandleLock::handle_mutexes;
} // namespace xpum