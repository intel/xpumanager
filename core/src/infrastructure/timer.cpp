#include <chrono>
#include <ctime>
#include <mutex>
#include <thread>

#include "timer.h"
#include "utility.h"
#include "logger.h"
#include "ilegal_state_exception.h"
#include "ilegal_parameter_exception.h"

Timer::Timer() : canceled(true), to_cancel(false) {

}

Timer::~Timer() { 
  this->cancel(); 
}

void Timer::scheduleAtFixedRate(long delay, int interval, 
  std::function<void()> task) {
  if (delay < 0 || interval <= 0) {
    Logger::instance().warn("invalid parameter in scheduleAtFixedRate");
    throw IlegalParameterException("invalid parameter when schedule a timer");    
  }

  if (this->canceled == false) {
    Logger::instance().warn("invalid timer status");
    throw IlegalStateException("the timer has been started");
  }

  this->canceled = false;

  std::thread([this, delay, interval, task]() {
    int wait = delay;
    while (!this->to_cancel) {
      std::this_thread::sleep_for(std::chrono::milliseconds(wait));
      long begin = Utility::getCurrentMillisecond();
      task();
      long end = Utility::getCurrentMillisecond();
      long spent = end - begin;
      if (interval >= spent) {
        wait = interval - spent;
      } else {
        Logger::instance().warn("The timer interval will not be accurate");
        wait = 0;
      }
    }

    std::unique_lock<std::mutex> locker(this->mutex);
    this->canceled = true;
    this->cancel_condition.notify_one();
  }).detach();
}

void Timer::schedule(int delay, std::function<void()> task) {
  std::thread([delay, task]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(delay));
    task();
  }).detach();
}

void Timer::cancel() noexcept {
  if (this->canceled) {
    return;
  }

  if (this->to_cancel) {
    return;
  }

  this->to_cancel = true;

  std::unique_lock<std::mutex> locker(this->mutex);
  this->cancel_condition.wait(locker, [this] { 
    return this->canceled == true; 
  });

  if (this->canceled == true) {
    this->to_cancel = false;
  }
}
