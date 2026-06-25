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

int get_arg_int(int argc, char ** argv, const std::string & name, int default_value)
{
  return std::stoi(get_arg(argc, argv, name, std::to_string(default_value)));
}

double get_arg_double(int argc, char ** argv, const std::string & name, double default_value)
{
  return std::stod(get_arg(argc, argv, name, std::to_string(default_value)));
}

}  // namespace

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);

  const std::string topic = get_arg(argc, argv, "--topic", "/perf_throughput");
  const std::string node_name = get_arg(argc, argv, "--node-name", "perf_throughput_pub_cpp");
  const int payload_bytes = get_arg_int(argc, argv, "--payload-bytes", 4096);
  const int warmup_count = get_arg_int(argc, argv, "--warmup-count", 0);
  const double duration_sec = get_arg_double(argc, argv, "--duration-sec", 5.0);
  const double rate_hz = get_arg_double(argc, argv, "--rate-hz", 500.0);
  const double match_timeout_sec = get_arg_double(argc, argv, "--match-timeout-sec", 15.0);
  const int post_match_settle_ms = get_arg_int(argc, argv, "--post-match-settle-ms", 0);

  auto node = std::make_shared<rclcpp::Node>(node_name);
  auto pub = node->create_publisher<std_msgs::msg::String>(topic, rclcpp::QoS(1000).reliable());
  const auto match_deadline =
    std::chrono::steady_clock::now() +
    std::chrono::duration_cast<std::chrono::steady_clock::duration>(
      std::chrono::duration<double>(match_timeout_sec));
  while (rclcpp::ok() && pub->get_subscription_count() == 0 &&
    std::chrono::steady_clock::now() < match_deadline)
  {
    rclcpp::spin_some(node);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  rclcpp::spin_some(node);
  if (pub->get_subscription_count() == 0) {
    std::cerr << "[ERROR] perf throughput publisher timed out waiting for subscriber match" <<
      std::endl;
    rclcpp::shutdown();
    return EXIT_FAILURE;
  }
  const auto matched_subscriptions = pub->get_subscription_count();

  if (post_match_settle_ms > 0) {
    std::this_thread::sleep_for(std::chrono::milliseconds(post_match_settle_ms));
  }

  rclcpp::WallRate rate(rate_hz);
  int seq = 0;
  for (; seq < warmup_count && rclcpp::ok(); ++seq) {
    const std::string prefix = std::to_string(seq) + "," + std::to_string(monotonic_now_ns()) + ",";
    std_msgs::msg::String msg;
    if (payload_bytes <= static_cast<int>(prefix.size())) {
      msg.data = prefix;
    } else {
      msg.data = prefix + std::string(static_cast<size_t>(payload_bytes - prefix.size()), 'x');
    }
    pub->publish(msg);
    rate.sleep();
  }

  int sent = 0;
  const auto send_deadline =
    std::chrono::steady_clock::now() +
    std::chrono::duration_cast<std::chrono::steady_clock::duration>(
      std::chrono::duration<double>(duration_sec));
  for (; rclcpp::ok() && std::chrono::steady_clock::now() < send_deadline; ++seq) {
    const std::string prefix = std::to_string(seq) + "," + std::to_string(monotonic_now_ns()) + ",";
    std_msgs::msg::String msg;
    if (payload_bytes <= static_cast<int>(prefix.size())) {
      msg.data = prefix;
    } else {
      msg.data = prefix + std::string(static_cast<size_t>(payload_bytes - prefix.size()), 'x');
    }
    pub->publish(msg);
    ++sent;
    rate.sleep();
  }

  std::cout << "[INFO] [perf_throughput_pub_cpp]: THROUGHPUT_PUB sent=" << sent
            << " payload_bytes=" << payload_bytes
            << " matched_subscriptions=" << matched_subscriptions
            << " duration_sec=" << std::fixed << std::setprecision(3) << duration_sec
            << " rate_hz=" << std::fixed << std::setprecision(3) << rate_hz
            << std::endl;

  rclcpp::shutdown();
  return EXIT_SUCCESS;
}
