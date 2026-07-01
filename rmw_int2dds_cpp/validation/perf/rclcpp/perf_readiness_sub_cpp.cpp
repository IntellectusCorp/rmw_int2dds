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

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"

namespace
{

uint64_t monotonic_now_ns()
{
  timespec ts{};
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return static_cast<uint64_t>(ts.tv_sec) * 1000000000ULL + static_cast<uint64_t>(ts.tv_nsec);
}

std::string get_arg(
  int argc, char ** argv, const std::string & name,
  const std::string & default_value)
{
  for (int i = 1; i + 1 < argc; ++i) {
    if (argv[i] == name) {
      return argv[i + 1];
    }
  }
  return default_value;
}

double get_arg_double(int argc, char ** argv, const std::string & name, double default_value)
{
  return std::stod(get_arg(argc, argv, name, std::to_string(default_value)));
}

class ReadinessSubscriber : public rclcpp::Node
{
public:
  ReadinessSubscriber(const std::string & topic, double timeout_sec)
  : Node("perf_readiness_sub_cpp"),
    deadline_(
      std::chrono::steady_clock::now() +
      std::chrono::duration_cast<std::chrono::steady_clock::duration>(
        std::chrono::duration<double>(timeout_sec)))
  {
    sub_ = create_subscription<std_msgs::msg::String>(
      topic,
      rclcpp::QoS(10).reliable(),
      [this](const std_msgs::msg::String::SharedPtr msg) {on_message(*msg);});
  }

  bool done() const
  {
    return done_;
  }

  int exit_code() const
  {
    return exit_code_;
  }

  void tick()
  {
    if (!done_ && std::chrono::steady_clock::now() >= deadline_) {
      done_ = true;
      exit_code_ = EXIT_FAILURE;
      std::cerr << "[ERROR] readiness subscriber timed out before first sample" << std::endl;
    }
  }

private:
  void on_message(const std_msgs::msg::String & msg)
  {
    if (done_) {
      return;
    }

    const auto comma = msg.data.find(',');
    if (comma == std::string::npos) {
      return;
    }

    const uint64_t start_ns = std::stoull(msg.data.substr(0, comma));
    const uint64_t now_ns = monotonic_now_ns();
    if (now_ns < start_ns) {
      return;
    }

    const double total_us = static_cast<double>(now_ns - start_ns) / 1000.0;
    const double first_arrival_ms = total_us / 1000.0;

    std::cout << std::fixed << std::setprecision(3)
              << "READINESS_DATA first_arrival_ms=" << first_arrival_ms
              << " first_sample_total_us=" << total_us
              << std::endl;

    done_ = true;
    exit_code_ = EXIT_SUCCESS;
  }

  bool done_{false};
  int exit_code_{EXIT_FAILURE};
  std::chrono::steady_clock::time_point deadline_;
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr sub_;
};

}  // namespace

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);

  const std::string topic = get_arg(argc, argv, "--topic", "/perf_ready");
  const double timeout_sec = get_arg_double(argc, argv, "--timeout-sec", 10.0);

  auto node = std::make_shared<ReadinessSubscriber>(topic, timeout_sec);

  while (rclcpp::ok() && !node->done()) {
    rclcpp::spin_some(node);
    node->tick();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  rclcpp::shutdown();
  return node->exit_code();
}
