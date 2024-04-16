#pragma once

#include <chrono>
#include <mutex>
#include <iostream>
#include <format>

namespace utils {

auto getRandomTimeout() {
  return std::chrono::seconds(3  + (rand() % 3));
}

template<typename... Args>
void log(std::format_string<Args...> fmt, Args&&... args) {
  static std::mutex mutex;
  std::lock_guard<std::mutex> lock(mutex);
  std::cout << std::format(fmt, std::forward<Args>(args)...) << std::endl;
}

} // namespace utils