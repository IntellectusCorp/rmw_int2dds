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
#include <iostream>
#include <string>
#include <thread>

#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/string.hpp>

using namespace std::chrono_literals;

static std::string require_value(int & i, int argc, char ** argv)
{
  if (i + 1 >= argc) {
    throw std::runtime_error(std::string("missing value for ") + argv[i]);
  }
  return argv[++i];
}

int main(int argc, char ** argv)
{
  std::string topic;
  std::string prefix = "burst";
  int depth = 1;
  int burst_count = 5;
  double publish_period = 0.05;
  int wait_for_subscriber_count = 1;
  double wait_timeout = 10.0;
  double keep_alive = 2.0;

  for (int i = 1; i < argc; ++i) {
    std::string arg(argv[i]);
    if (arg == "--topic") {
      topic = require_value(i, argc, argv);
    } else if (arg == "--prefix") {
      prefix = require_value(i, argc, argv);
    } else if (arg == "--depth") {
      depth = std::stoi(require_value(i, argc, argv));
    } else if (arg == "--burst-count") {
      burst_count = std::stoi(require_value(i, argc, argv));
    } else if (arg == "--publish-period") {
      publish_period = std::stod(require_value(i, argc, argv));
    } else if (arg == "--wait-for-subscriber-count") {
      wait_for_subscriber_count = std::stoi(require_value(i, argc, argv));
    } else if (arg == "--wait-timeout") {
      wait_timeout = std::stod(require_value(i, argc, argv));
    } else if (arg == "--keep-alive") {
      keep_alive = std::stod(require_value(i, argc, argv));
    } else {
      throw std::runtime_error("unknown argument: " + arg);
    }
  }

  if (topic.empty()) {
    throw std::runtime_error("--topic is required");
  }

  rclcpp::init(argc, argv);
  auto node = std::make_shared<rclcpp::Node>("qos_history_depth_pub_cpp");
  auto qos = rclcpp::QoS(rclcpp::KeepLast(depth));
  auto publisher = node->create_publisher<std_msgs::msg::String>(topic, qos);
  RCLCPP_INFO(
    node->get_logger(),
    "publisher ready: topic=%s, history=keep_last, depth=%d, burst_count=%d",
    topic.c_str(), depth, burst_count);

  auto start = std::chrono::steady_clock::now();
  bool matched = false;
  while (std::chrono::steady_clock::now() - start < std::chrono::duration<double>(wait_timeout)) {
    auto count = publisher->get_subscription_count();
    RCLCPP_INFO(node->get_logger(), "subscriber_count observation: %zu", count);
    if (static_cast<int>(count) >= wait_for_subscriber_count) {
      matched = true;
      break;
    }
    rclcpp::spin_some(node);
    std::this_thread::sleep_for(400ms);
  }

  if (!matched) {
    RCLCPP_ERROR(node->get_logger(), "publisher failed: subscriber did not appear in time");
    rclcpp::shutdown();
    return 1;
  }

  RCLCPP_INFO(node->get_logger(), "history/depth publisher matched subscriber");
  for (int i = 0; i < burst_count; ++i) {
    std_msgs::msg::String msg;
    msg.data = prefix + "-" + std::to_string(i);
    publisher->publish(msg);
    RCLCPP_INFO(node->get_logger(), "published: %s", msg.data.c_str());
    std::this_thread::sleep_for(std::chrono::duration<double>(publish_period));
  }
  RCLCPP_INFO(
    node->get_logger(), "published %s-0..%s-%d burst", prefix.c_str(), prefix.c_str(),
    burst_count - 1);

  auto keep_deadline = std::chrono::steady_clock::now() +
    std::chrono::duration_cast<std::chrono::steady_clock::duration>(
    std::chrono::duration<double>(keep_alive));
  while (std::chrono::steady_clock::now() < keep_deadline) {
    auto count = publisher->get_subscription_count();
    RCLCPP_INFO(node->get_logger(), "subscriber_count observation: %zu", count);
    rclcpp::spin_some(node);
    std::this_thread::sleep_for(400ms);
  }
  RCLCPP_INFO(node->get_logger(), "publisher keep-alive finished");

  rclcpp::shutdown();
  return 0;
}
