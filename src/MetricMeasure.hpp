#ifndef METRICMEASURE_HPP
#define METRICMEASURE_HPP

#include <array>
#include <atomic>
#include <ctime>
#include <fstream>
#include <ios>
#include <memory>
#include <print>
#include <ranges>
#include <type_traits>
#include <vector>

namespace ddsolve {

template <size_t kMaxThreads>
class MetricMeasure {
 public:
  constexpr uint32_t getLocalIndex() {
    thread_local uint32_t local_index{
        next_idx.fetch_add(1, std::memory_order_relaxed)};

    if (!data_.at(local_index)) {
      data_.at(local_index) = std::make_unique<std::vector<timespec>>();
    }

    return local_index;
  }

  void start() {
    auto& vec{*data_[getLocalIndex()]};
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
    timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    vec.push_back(t);
  }

  void stop() {
    auto& last{data_[getLocalIndex()]->back()};
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
    timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);

    t.tv_sec = t.tv_sec - last.tv_sec;
    t.tv_nsec = t.tv_nsec - last.tv_nsec;
    if (t.tv_nsec < 0) {
      t.tv_sec -= 1;
      constexpr auto one_sec{1'000'000'000};
      t.tv_nsec += one_sec;
    }

    last = t;
  }

  void print() {
    // Not threadsafe!

    for (auto&& [tid, i] : std::views::enumerate(data_)) {
      if (i) {
        for (auto&& t : *i) {
          if (t.tv_sec == 0) {
            std::println("{} {}", tid, t.tv_nsec);
          } else {
            std::println("{} {}{:09}", tid, t.tv_sec, t.tv_nsec);
          }
        }
      }
    }
  }

  void write() {
    // Not threadsafe!
    auto file{std::ofstream{"/tmp/data.txt",
                            std::ios_base::out | std::ios_base::trunc}};

    for (auto&& [tid, i] : std::views::enumerate(data_)) {
      if (i) {
        for (auto&& t : *i) {
          if (t.tv_sec == 0) {
            std::println(file, "{} {}", tid, t.tv_nsec);
          } else {
            std::println(file, "{} {}{:09}", tid, t.tv_sec, t.tv_nsec);
          }
        }
      }
    }
  }

 private:
  std::atomic_uint32_t next_idx{0};
  std::array<std::unique_ptr<std::vector<timespec>>, kMaxThreads> data_;
};

static_assert(std::is_signed_v<decltype(timespec::tv_nsec)>);

}  // namespace ddsolve

#endif /* METRICMEASURE_HPP */
