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

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <exception>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <thread>
#include <vector>

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

double percentile(std::vector<double> samples, double q)
{
  if (samples.empty()) {
    return 0.0;
  }
  std::sort(samples.begin(), samples.end());
  const double pos = q * static_cast<double>(samples.size() - 1);
  const auto lower = static_cast<size_t>(std::floor(pos));
  const auto upper = static_cast<size_t>(std::ceil(pos));
  if (lower == upper) {
    return samples[lower];
  }
  const double weight = pos - static_cast<double>(lower);
  return samples[lower] * (1.0 - weight) + samples[upper] * weight;
}

class LatencySubscriber : public rclcpp::Node
{
public:
  LatencySubscriber(
    const std::string & node_name,
    const std::string & topic,
    int expected,
    int warmup,
    double timeout_sec)
  : Node(node_name),
    expected_(expected),
    warmup_(warmup),
    deadline_(
      std::chrono::steady_clock::now() +
      std::chrono::duration_cast<std::chrono::steady_clock::duration>(
        std::chrono::duration<double>(timeout_sec)))
  {
    sub_ = create_subscription<std_msgs::msg::String>(
      topic,
      rclcpp::QoS(1000).reliable(),
      [this](const std_msgs::msg::String::SharedPtr msg) {on_message(*msg);});
    timer_ = create_wall_timer(
      std::chrono::milliseconds(10),
      [this]() {on_tick();});
  }

  bool done() const
  {
    return done_;
  }

  int exit_code() const
  {
    return exit_code_;
  }

  void on_tick()
  {
    if (done_) {
      return;
    }
    if (std::chrono::steady_clock::now() >= deadline_) {
      done_ = true;
      exit_code_ = EXIT_FAILURE;
      std::cerr << "[ERROR] perf latency subscriber timed out after collecting "
                << latencies_us_.size() << " samples" << std::endl;
      rclcpp::shutdown();
    }
  }

private:
  void on_message(const std_msgs::msg::String & msg)
  {
    if (done_) {
      return;
    }

    ++seen_messages_;
    if (seen_messages_ <= warmup_) {
      return;
    }

    const auto comma = msg.data.find(',');
    if (comma == std::string::npos) {
      return;
    }

    uint64_t send_ns = 0;
    try {
      send_ns = std::stoull(msg.data.substr(comma + 1));
    } catch (const std::exception &) {
      return;
    }
    const uint64_t now_ns = monotonic_now_ns();
    if (now_ns < send_ns) {
      return;
    }

    latencies_us_.push_back(static_cast<double>(now_ns - send_ns) / 1000.0);
    if (static_cast<int>(latencies_us_.size()) >= expected_) {
      print_summary();
      done_ = true;
      exit_code_ = EXIT_SUCCESS;
      rclcpp::shutdown();
    }
  }

  void print_summary() const
  {
    const auto samples = static_cast<int>(latencies_us_.size());
    double sum = 0.0;
    double min_v = std::numeric_limits<double>::max();
    double max_v = 0.0;
    for (double value : latencies_us_) {
      sum += value;
      min_v = std::min(min_v, value);
      max_v = std::max(max_v, value);
    }
    const double avg = samples > 0 ? sum / static_cast<double>(samples) : 0.0;

    std::cout << std::fixed << std::setprecision(3)
              << "LATENCY samples=" << samples
              << " avg_us=" << avg
              << " p50_us=" << percentile(latencies_us_, 0.50)
              << " p95_us=" << percentile(latencies_us_, 0.95)
              << " p99_us=" << percentile(latencies_us_, 0.99)
              << " min_us=" << min_v
              << " max_us=" << max_v
              << std::endl;
  }

  int expected_{};
  int warmup_{};
  int seen_messages_{0};
  bool done_{false};
  int exit_code_{EXIT_FAILURE};
  std::chrono::steady_clock::time_point deadline_;
  std::vector<double> latencies_us_;
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr sub_;
  rclcpp::TimerBase::SharedPtr timer_;
};

}  // namespace

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);

  const std::string topic = get_arg(argc, argv, "--topic", "/perf_latency");
  const std::string node_name = get_arg(argc, argv, "--node-name", "perf_latency_sub_cpp");
  const int expected = get_arg_int(argc, argv, "--expected", 200);
  const int warmup = get_arg_int(argc, argv, "--warmup", 20);
  const double timeout_sec = get_arg_double(argc, argv, "--timeout-sec", 10.0);

  auto node = std::make_shared<LatencySubscriber>(
    node_name,
    topic,
    expected,
    warmup,
    timeout_sec);

  rclcpp::spin(node);
  return node->exit_code();
}
