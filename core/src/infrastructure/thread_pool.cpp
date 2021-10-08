#include <future>

#include "logger.h"
#include "infrastructure/exception/ilegal_state_exception.h"
#include "thread_pool.h"

ThreadPool::ThreadPool(unsigned int size) : stop(false), size(size) {
  XPUM_LOG_INFO("ThreadPool()");
  init();
}

ThreadPool::~ThreadPool() {
  XPUM_LOG_INFO("~ThreadPool()");
  close();
}

void ThreadPool::init() {
  if (stop) {
    return ;
  }

  for (unsigned int i = 0; i < size; ++i) {
    workers.emplace_back(std::thread([this, i] {
      while (!stop) {
        std::function<void()> task = this->tasks.remove();
        if (task == nullptr) {
          continue;
        }

        try {
          task();
        } catch (std::exception& e) {
          std::string error = "Failed to execute task in thread pool: ";
          error += e.what();
          XPUM_LOG_ERROR(error);          
        } catch (...) {
          std::string error = "Failed to execute task in thread pool: unexpected exception";
          XPUM_LOG_ERROR(error);
        }

      }
    }));
  }
}

void ThreadPool::close() {
  if (stop) {
    return ;
  }

  stop = true;
  tasks.close();

  for (std::thread& worker : workers) {
    worker.join();
  }
  workers.clear();
}
