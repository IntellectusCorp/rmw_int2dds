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

class ThroughputSubscriber : public rclcpp::Node
{
public:
  ThroughputSubscriber(
    const std::string & node_name,
    const std::string & topic,
    int warmup_count,
    double window_sec,
    double timeout_sec)
  : Node(node_name),
    warmup_count_(warmup_count),
    window_sec_(window_sec),
    deadline_(
      std::chrono::steady_clock::now() +
      std::chrono::duration_cast<std::chrono::steady_clock::duration>(
        std::chrono::duration<double>(timeout_sec)))
  {
    sub_ = create_subscription<std_msgs::msg::String>(
      topic,
      rclcpp::QoS(1000).reliable(),
      [this](const std_msgs::msg::String::SharedPtr msg) {on_message(*msg);});
    watchdog_timer_ = create_wall_timer(
      std::chrono::milliseconds(10),
      [this]() {on_watchdog_tick();});
  }

  bool done() const
  {
    return done_;
  }

  int exit_code() const
  {
    return exit_code_;
  }

  void on_watchdog_tick()
  {
    if (done_) {
      return;
    }

    if (started_) {
      const uint64_t now_ns = monotonic_now_ns();
      const double duration_s = static_cast<double>(now_ns - first_receive_ns_) / 1000000000.0;
      if (duration_s >= window_sec_) {
        finish_success(now_ns);
        return;
      }
    }

    if (std::chrono::steady_clock::now() >= deadline_) {
      done_ = true;
      exit_code_ = EXIT_FAILURE;
      std::cerr << "[ERROR] throughput subscriber timed out after collecting "
                << recv_msgs_ << " samples" << std::endl;
      rclcpp::shutdown();
    }
  }

private:
  void on_message(const std_msgs::msg::String & msg)
  {
    if (done_) {
      return;
    }

    ++seen_msgs_;
    if (seen_msgs_ <= static_cast<size_t>(warmup_count_)) {
      return;
    }

    const uint64_t now_ns = monotonic_now_ns();
    if (!started_) {
      started_ = true;
      first_receive_ns_ = now_ns;
    }

    ++recv_msgs_;
    recv_bytes_ += msg.data.size();

    const double duration_s = static_cast<double>(now_ns - first_receive_ns_) / 1000000000.0;
    if (duration_s >= window_sec_) {
      finish_success(now_ns);
    }
  }

  void finish_success(uint64_t now_ns)
  {
    if (done_) {
      return;
    }
    print_summary(now_ns);
    done_ = true;
    exit_code_ = EXIT_SUCCESS;
    rclcpp::shutdown();
  }

  void print_summary(uint64_t now_ns) const
  {
    const double duration_s = first_receive_ns_ == now_ns ? 0.0 :
      static_cast<double>(now_ns - first_receive_ns_) / 1000000000.0;
    const double recv_mps = duration_s > 0.0 ? static_cast<double>(recv_msgs_) / duration_s : 0.0;
    const double recv_mb_s = duration_s >
      0.0 ? static_cast<double>(recv_bytes_) / duration_s / 1000000.0 : 0.0;

    std::cout << std::fixed << std::setprecision(3)
              << "THROUGHPUT_SUB duration_s=" << duration_s
              << " recv_msgs=" << recv_msgs_
              << " recv_mps=" << recv_mps
              << " recv_MBps=" << recv_mb_s
              << std::endl;
  }

  int warmup_count_{0};
  double window_sec_{};
  bool started_{false};
  bool done_{false};
  int exit_code_{EXIT_FAILURE};
  size_t seen_msgs_{0};
  size_t recv_msgs_{0};
  size_t recv_bytes_{0};
  uint64_t first_receive_ns_{0};
  std::chrono::steady_clock::time_point deadline_;
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr sub_;
  rclcpp::TimerBase::SharedPtr watchdog_timer_;
};

}  // namespace

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);

  const std::string topic = get_arg(argc, argv, "--topic", "/perf_throughput");
  const std::string node_name = get_arg(argc, argv, "--node-name", "perf_throughput_sub_cpp");
  const int warmup_count = get_arg_int(argc, argv, "--warmup-count", 0);
  const double window_sec = get_arg_double(argc, argv, "--window-sec", 5.0);
  const double timeout_sec = get_arg_double(argc, argv, "--timeout-sec", 20.0);

  auto node = std::make_shared<ThroughputSubscriber>(
    node_name, topic, warmup_count, window_sec, timeout_sec);

  rclcpp::spin(node);
  return node->exit_code();
}
