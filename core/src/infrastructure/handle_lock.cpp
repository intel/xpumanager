#include "handle_lock.h"

#include <map>
#include <memory>
#include <mutex>

namespace xpum {
std::mutex HandleLock::handle_mutexes_mutex;
std::map<void*, std::shared_ptr<std::mutex>> HandleLock::handle_mutexes;
} // namespace xpum