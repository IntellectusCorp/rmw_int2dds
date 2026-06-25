// Copyright 2026 Int2DDS Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "rclcpp/rclcpp.hpp"

using namespace std::chrono_literals;

namespace
{

rclcpp::NodeOptions quiet_node_options()
{
  return rclcpp::NodeOptions()
         .start_parameter_services(false)
         .enable_rosout(false);
}

template<typename PredicateT>
bool spin_until(
  rclcpp::Executor & executor,
  PredicateT predicate,
  std::chrono::milliseconds timeout)
{
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (rclcpp::ok() && std::chrono::steady_clock::now() < deadline) {
    if (predicate()) {
      return true;
    }
    executor.spin_once(50ms);
  }
  return predicate();
}

bool run_single_thread_timer_check()
{
  auto node = std::make_shared<rclcpp::Node>(
    "callback_executor_single_thread_smoke_cpp",
    quiet_node_options());
  rclcpp::executors::SingleThreadedExecutor executor;
  executor.add_node(node);

  std::atomic<int> callback_count{0};
  rclcpp::TimerBase::SharedPtr timer;
  timer = node->create_wall_timer(
    20ms,
    [&callback_count, &timer, node]() {
      const int count = ++callback_count;
      RCLCPP_INFO(node->get_logger(), "single-thread timer callback #%d", count);
      if (count >= 3 && timer) {
        timer->cancel();
      }
    });

  const bool ok = spin_until(
    executor,
    [&callback_count]() {
      return callback_count.load() >= 3;
    },
    2s);

  executor.remove_node(node);

  if (ok) {
    RCLCPP_INFO(node->get_logger(), "rclcpp single-thread executor callback ok");
  } else {
    RCLCPP_ERROR(
      node->get_logger(),
      "rclcpp single-thread executor callback failed: count=%d",
      callback_count.load());
  }
  return ok;
}

bool run_callback_group_check()
{
  auto node = std::make_shared<rclcpp::Node>(
    "callback_group_smoke_cpp",
    quiet_node_options());
  rclcpp::executors::MultiThreadedExecutor executor(
    rclcpp::ExecutorOptions(),
    4);
  executor.add_node(node);

  auto mutex_group = node->create_callback_group(rclcpp::CallbackGroupType::MutuallyExclusive);
  auto reentrant_group = node->create_callback_group(rclcpp::CallbackGroupType::Reentrant);

  std::atomic<int> mutex_active{0};
  std::atomic<int> reentrant_active{0};
  std::atomic<int> mutex_hits{0};
  std::atomic<int> reentrant_hits{0};
  std::atomic<bool> mutex_overlap{false};
  std::atomic<bool> reentrant_overlap{false};

  auto make_timer = [&](rclcpp::CallbackGroup::SharedPtr group, auto && callback) {
      return node->create_wall_timer(
        10ms,
        std::forward<decltype(callback)>(callback),
        group);
    };

  std::vector<rclcpp::TimerBase::SharedPtr> timers;
  timers.push_back(make_timer(
      mutex_group,
      [&]() {
        if (++mutex_active > 1) {
          mutex_overlap = true;
        }
        ++mutex_hits;
        std::this_thread::sleep_for(200ms);
        --mutex_active;
      }));
  timers.push_back(make_timer(
      mutex_group,
      [&]() {
        if (++mutex_active > 1) {
          mutex_overlap = true;
        }
        ++mutex_hits;
        std::this_thread::sleep_for(200ms);
        --mutex_active;
      }));
  timers.push_back(make_timer(
      reentrant_group,
      [&]() {
        if (++reentrant_active > 1) {
          reentrant_overlap = true;
        }
        ++reentrant_hits;
        std::this_thread::sleep_for(200ms);
        --reentrant_active;
      }));
  timers.push_back(make_timer(
      reentrant_group,
      [&]() {
        if (++reentrant_active > 1) {
          reentrant_overlap = true;
        }
        ++reentrant_hits;
        std::this_thread::sleep_for(200ms);
        --reentrant_active;
      }));

  std::thread spin_thread([&executor]() {
      executor.spin();
    });
  std::this_thread::sleep_for(2s);
  executor.cancel();
  if (spin_thread.joinable()) {
    spin_thread.join();
  }

  for (auto & timer : timers) {
    timer->cancel();
  }
  executor.remove_node(node);

  const bool ok =
    mutex_hits.load() >= 2 &&
    reentrant_hits.load() >= 2 &&
    !mutex_overlap.load() &&
    reentrant_overlap.load();

  if (ok) {
    RCLCPP_INFO(node->get_logger(), "rclcpp callback group behavior ok");
  } else {
    RCLCPP_ERROR(
      node->get_logger(),
      "rclcpp callback group behavior failed: mutex_hits=%d mutex_overlap=%s "
      "reentrant_hits=%d reentrant_overlap=%s",
      mutex_hits.load(),
      mutex_overlap.load() ? "true" : "false",
      reentrant_hits.load(),
      reentrant_overlap.load() ? "true" : "false");
  }
  return ok;
}

}  // namespace

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);

  const bool timer_ok = run_single_thread_timer_check();
  const bool group_ok = run_callback_group_check();
  const bool ok = timer_ok && group_ok;

  rclcpp::shutdown();

  if (ok) {
    RCUTILS_LOG_INFO_NAMED("callback_executor_smoke_cpp", "rclcpp callback/executor smoke ok");
    return EXIT_SUCCESS;
  }

  RCUTILS_LOG_ERROR_NAMED(
    "callback_executor_smoke_cpp",
    "rclcpp callback/executor smoke failed: timer_ok=%s group_ok=%s",
    timer_ok ? "true" : "false",
    group_ok ? "true" : "false");
  return EXIT_FAILURE;
}
