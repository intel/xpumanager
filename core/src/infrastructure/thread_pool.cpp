#include "thread_pool.h"

#include <future>

#include "infrastructure/exception/ilegal_state_exception.h"
#include "logger.h"

namespace xpum {

ThreadPool::ThreadPool(unsigned int size) {
    XPUM_LOG_DEBUG("ThreadPool()");
    init();
}

ThreadPool::~ThreadPool() {
    XPUM_LOG_DEBUG("~ThreadPool()");
    close();
}

void ThreadPool::init() {

}

void ThreadPool::close() {
}

} // end namespace xpum
