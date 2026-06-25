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
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/string.hpp>

using namespace std::chrono_literals;

struct ReceivedMessage
{
  std::string data;
  std::chrono::steady_clock::time_point received_at;
};

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
  std::string expect_latest;
  int depth = 1;
  double timeout = 10.0;
  double drain_after = 1.0;
  std::vector<ReceivedMessage> received;

  for (int i = 1; i < argc; ++i) {
    std::string arg(argv[i]);
    if (arg == "--topic") {
      topic = require_value(i, argc, argv);
    } else if (arg == "--expect-latest") {
      expect_latest = require_value(i, argc, argv);
    } else if (arg == "--depth") {
      depth = std::stoi(require_value(i, argc, argv));
    } else if (arg == "--timeout") {
      timeout = std::stod(require_value(i, argc, argv));
    } else if (arg == "--drain-after") {
      drain_after = std::stod(require_value(i, argc, argv));
    } else {
      throw std::runtime_error("unknown argument: " + arg);
    }
  }

  if (topic.empty()) {
    throw std::runtime_error("--topic is required");
  }
  if (expect_latest.empty()) {
    throw std::runtime_error("--expect-latest is required");
  }

  rclcpp::init(argc, argv);
  auto node = std::make_shared<rclcpp::Node>("qos_history_depth_sub_cpp");
  auto qos = rclcpp::QoS(rclcpp::KeepLast(depth));
  auto subscription = node->create_subscription<std_msgs::msg::String>(
    topic,
    qos,
    [&received, node](std_msgs::msg::String::SharedPtr msg) {
      received.push_back({msg->data, std::chrono::steady_clock::now()});
      RCLCPP_INFO(node->get_logger(), "received: %s", msg->data.c_str());
    });

  RCLCPP_INFO(
    node->get_logger(), "subscriber ready: topic=%s, history=keep_last, depth=%d",
    topic.c_str(), depth);

  rclcpp::executors::SingleThreadedExecutor executor;
  executor.add_node(node);
  auto start = std::chrono::steady_clock::now();
  std::optional<std::chrono::steady_clock::time_point> drain_time;
  while (std::chrono::steady_clock::now() - start < std::chrono::duration<double>(timeout)) {
    auto count = subscription->get_publisher_count();
    RCLCPP_INFO(node->get_logger(), "publisher_count observation: %zu", count);
    if (count > 0 && !drain_time.has_value()) {
      drain_time = std::chrono::steady_clock::now() +
        std::chrono::duration_cast<std::chrono::steady_clock::duration>(
        std::chrono::duration<double>(drain_after));
    }
    if (drain_time.has_value() && std::chrono::steady_clock::now() >= drain_time.value()) {
      break;
    }
    std::this_thread::sleep_for(500ms);
  }

  if (!drain_time.has_value()) {
    RCLCPP_ERROR(node->get_logger(),
      "history/depth subscriber failed: publisher not observed in time");
    rclcpp::shutdown();
    return 1;
  }

  auto drain_deadline = std::chrono::steady_clock::now() + 2s;
  while (std::chrono::steady_clock::now() < drain_deadline) {
    executor.spin_some();
    std::this_thread::sleep_for(50ms);
  }

  std::vector<std::string> drained;
  for (const auto & message : received) {
    if (message.received_at >= drain_time.value()) {
      drained.push_back(message.data);
    }
  }

  if (drained.empty()) {
    RCLCPP_ERROR(node->get_logger(), "history/depth subscriber failed: received_count=0");
    executor.remove_node(node);
    rclcpp::shutdown();
    return 1;
  }

  const auto & latest = drained.back();
  RCLCPP_INFO(node->get_logger(), "depth subscriber heard: %s", latest.c_str());
  if (latest != expect_latest || drained.size() != 1) {
    RCLCPP_ERROR(
      node->get_logger(),
      "history/depth subscriber failed: expected only %s after drain,"
      "drained_count=%zu, total_received=%zu",
      expect_latest.c_str(), drained.size(), received.size());
    executor.remove_node(node);
    rclcpp::shutdown();
    return 1;
  }

  RCLCPP_INFO(
    node->get_logger(), "history/depth subscriber ok: retained latest sample only (%s)",
    expect_latest.c_str());
  executor.remove_node(node);
  rclcpp::shutdown();
  return 0;
}
