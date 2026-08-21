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
#include <exception>
#include <functional>
#include <string>
#include <vector>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/int32.hpp"

using namespace std::chrono_literals;

namespace
{

bool spin_until(
  rclcpp::executors::SingleThreadedExecutor & executor,
  const std::function<bool()> & predicate,
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

void spin_for(
  rclcpp::executors::SingleThreadedExecutor & executor,
  std::chrono::milliseconds duration)
{
  const auto deadline = std::chrono::steady_clock::now() + duration;
  while (rclcpp::ok() && std::chrono::steady_clock::now() < deadline) {
    executor.spin_once(50ms);
  }
}

bool publish_values(
  const rclcpp::Publisher<std_msgs::msg::Int32>::SharedPtr & publisher,
  rclcpp::executors::SingleThreadedExecutor & executor,
  std::vector<int> & received,
  const std::vector<int> & values,
  const std::vector<int> & expected)
{
  received.clear();
  for (const int value : values) {
    std_msgs::msg::Int32 message;
    message.data = value;
    publisher->publish(message);
    spin_for(executor, 250ms);
  }
  spin_for(executor, 250ms);
  return received == expected;
}

std::string join_values(const std::vector<int> & values)
{
  std::string output;
  for (size_t i = 0; i < values.size(); ++i) {
    if (i != 0) {
      output += ",";
    }
    output += std::to_string(values[i]);
  }
  return output;
}

bool filter_matches(
  const rclcpp::ContentFilterOptions & options,
  const std::string & expression,
  const std::vector<std::string> & parameters)
{
  return options.filter_expression == expression &&
         options.expression_parameters == parameters;
}

}  // namespace

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);

  const auto publisher_node = std::make_shared<rclcpp::Node>(
    "content_filter_lifecycle_pub",
    rclcpp::NodeOptions().start_parameter_services(false).enable_rosout(false));
  const auto subscriber_node = std::make_shared<rclcpp::Node>(
    "content_filter_lifecycle_sub",
    rclcpp::NodeOptions().start_parameter_services(false).enable_rosout(false));

  rclcpp::executors::SingleThreadedExecutor executor;
  executor.add_node(publisher_node);
  executor.add_node(subscriber_node);

  std::vector<int> received;
  rclcpp::SubscriptionOptions subscription_options;
  subscription_options.content_filter_options.filter_expression = "data >= %0";
  subscription_options.content_filter_options.expression_parameters = {"3"};

  const auto qos = rclcpp::QoS(10);
  auto subscription = subscriber_node->create_subscription<std_msgs::msg::Int32>(
    "/cft_demo",
    qos,
    [&received](const std_msgs::msg::Int32::SharedPtr message) {
      received.push_back(message->data);
    },
    subscription_options);
  auto publisher = publisher_node->create_publisher<std_msgs::msg::Int32>("/cft_demo", qos);

  bool ok = false;
  try {
    const bool matched = spin_until(
      executor,
      [&publisher, &subscriber_node]() {
        return publisher->get_subscription_count() == 1 &&
               subscriber_node->count_publishers("/cft_demo") == 1;
      },
      3s);
    if (!matched) {
      RCLCPP_ERROR(publisher_node->get_logger(), "content filter match failed");
      rclcpp::shutdown();
      return EXIT_FAILURE;
    }

    RCLCPP_INFO(publisher_node->get_logger(), "content filter publisher matched subscriber");

    const bool phase1_ok = publish_values(publisher, executor, received, {1, 3, 5}, {3, 5});
    const auto phase1_filter = subscription->get_content_filter();
    RCLCPP_INFO(
      publisher_node->get_logger(),
      "phase1_filter expr='%s' params='%s'",
      phase1_filter.filter_expression.c_str(),
      phase1_filter.expression_parameters.empty() ? "" :
      phase1_filter.expression_parameters[0].c_str());
    RCLCPP_INFO(publisher_node->get_logger(), "phase1_received %s", join_values(received).c_str());

    subscription->set_content_filter("data >= %0", {"5"});
    spin_for(executor, 250ms);
    const bool phase2_ok = publish_values(publisher, executor, received, {3, 5, 7}, {5, 7});
    const auto phase2_filter = subscription->get_content_filter();
    RCLCPP_INFO(
      publisher_node->get_logger(),
      "phase2_filter expr='%s' params='%s'",
      phase2_filter.filter_expression.c_str(),
      phase2_filter.expression_parameters.empty() ? "" :
      phase2_filter.expression_parameters[0].c_str());
    RCLCPP_INFO(publisher_node->get_logger(), "phase2_received %s", join_values(received).c_str());

    subscription->set_content_filter("", {});
    spin_for(executor, 250ms);
    const bool phase3_ok = publish_values(publisher, executor, received, {2, 4, 6}, {2, 4, 6});
    RCLCPP_INFO(
      publisher_node->get_logger(),
      "phase3_enabled %s",
      subscription->is_cft_enabled() ? "true" : "false");
    RCLCPP_INFO(publisher_node->get_logger(), "phase3_received %s", join_values(received).c_str());

    ok = phase1_ok &&
      filter_matches(phase1_filter, "data >= %0", {"3"}) &&
      phase2_ok &&
      filter_matches(phase2_filter, "data >= %0", {"5"}) &&
      phase3_ok &&
      !subscription->is_cft_enabled();

    if (ok) {
      RCLCPP_INFO(publisher_node->get_logger(), "content filter lifecycle ok");
    } else {
      RCLCPP_ERROR(
        publisher_node->get_logger(),
        "content filter lifecycle failed: phase1_ok=%s phase2_ok=%s phase3_ok=%s",
        phase1_ok ? "true" : "false",
        phase2_ok ? "true" : "false",
        phase3_ok ? "true" : "false");
    }
  } catch (const std::exception & error) {
    RCLCPP_ERROR(publisher_node->get_logger(), "content filter lifecycle exception: %s",
      error.what());
  }

  executor.remove_node(subscriber_node);
  executor.remove_node(publisher_node);
  subscription.reset();
  publisher.reset();
  rclcpp::shutdown();
  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
