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

int get_arg_int(int argc, char ** argv, const std::string & name, int default_value)
{
  return std::stoi(get_arg(argc, argv, name, std::to_string(default_value)));
}

}  // namespace

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);

  const std::string topic = get_arg(argc, argv, "--topic", "/perf_ready");
  const int period_ms = get_arg_int(argc, argv, "--period-ms", 20);
  const int sample_limit = get_arg_int(argc, argv, "--sample-limit", 50);

  auto node = std::make_shared<rclcpp::Node>("perf_readiness_pub_cpp");
  auto pub = node->create_publisher<std_msgs::msg::String>(topic, rclcpp::QoS(10).reliable());
  rclcpp::WallRate rate(1000.0 / static_cast<double>(period_ms));

  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  const uint64_t start_ns = monotonic_now_ns();

  for (int seq = 0; seq < sample_limit && rclcpp::ok(); ++seq) {
    std_msgs::msg::String msg;
    msg.data = std::to_string(start_ns) + "," + std::to_string(seq);
    pub->publish(msg);
    rate.sleep();
  }

  std::cout << "[INFO] [perf_readiness_pub_cpp]: READINESS_PUB sent=" << sample_limit
            << " period_ms=" << period_ms << std::endl;

  rclcpp::shutdown();
  return EXIT_SUCCESS;
}
