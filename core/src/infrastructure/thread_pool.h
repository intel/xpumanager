#pragma once

#include <thread>
#include <tuple>

#include "const.h"
#include "infrastructure/exception/base_exception.h"
#include "logger.h"
#include "shared_queue.h"

namespace xpum {

class ThreadPool {
   public:
    ThreadPool(unsigned int size);

    ~ThreadPool();

    template <class F, class... Args>
    void addTask(Callback_t callback, F &&f, Args &&... args) {
        if (stop) {
            throw BaseException("add task to a stopped ThreadPool");
        }

        auto task = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
        auto taskInfo = std::make_tuple(task, callback);

        tasks.add([taskInfo]() {
            try {
                auto ret = (std::get<0>(taskInfo))();
                (std::get<1>(taskInfo))(std::static_pointer_cast<void>(ret), nullptr);
            } catch (std::exception &e) {
                std::string error = "Failed to execute task in thread pool:";
                error += e.what();
                XPUM_LOG_DEBUG(error);
                (std::get<1>(taskInfo))(nullptr, std::make_shared<BaseException>(e.what()));
            } catch (...) {
                std::string error =
                    "Failed to execute task in thread pool: unexpected exception";
                XPUM_LOG_DEBUG(error);
                (std::get<1>(taskInfo))(nullptr, std::make_shared<BaseException>(error));
            }
        });
    }

   private:
    void init();

    void close();

   private:
    std::vector<std::thread> workers;

    SharedQueue<std::function<void()>> tasks;

    std::atomic<bool> stop;

    unsigned int size;
};

} // end namespace xpum
