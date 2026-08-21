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
//
// D4 verification: rmw_take_sequence now does one batch DDS take instead of N single
// takes. Publishes N messages, waits for reliable delivery, then pulls them all back in a
// single rmw_take_sequence call and checks the count is > 1 (a real batch) and values are
// correct and contiguous. The executor is intentionally not spun; DDS delivers to the
// datareader cache and rmw_take_sequence pulls directly.

#include <chrono>
#include <cstdio>
#include <memory>
#include <thread>
#include <vector>

#include "rclcpp/rclcpp.hpp"
#include "rcl/subscription.h"
#include "rmw/rmw.h"
#include "rcutils/allocator.h"

#if __has_include("std_msgs/msg/int32.hpp")
#include "std_msgs/msg/int32.hpp"
#define HAVE_INT32 1
#endif

int main(int argc, char ** argv)
{
#ifndef HAVE_INT32
  (void)argc;
  (void)argv;
  std::printf("SKIP: std_msgs unavailable\n");
  return 0;
#else
  rclcpp::init(argc, argv);
  auto node = std::make_shared<rclcpp::Node>("test_take_sequence");
  rclcpp::QoS qos(20);
  qos.reliable();

  auto pub = node->create_publisher<std_msgs::msg::Int32>("take_seq_topic", qos);
  auto sub = node->create_subscription<std_msgs::msg::Int32>(
    "take_seq_topic", qos, [](std_msgs::msg::Int32::SharedPtr) {});

  auto match_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (pub->get_subscription_count() < 1 &&
    std::chrono::steady_clock::now() < match_deadline)
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  const bool matched = pub->get_subscription_count() >= 1;

  const int kN = 5;
  for (int i = 0; i < kN; ++i) {
    std_msgs::msg::Int32 m;
    m.data = 100 + i;
    pub->publish(m);
  }
  // Reliable + localhost delivers the 5 tiny samples to the datareader cache quickly.
  std::this_thread::sleep_for(std::chrono::milliseconds(600));

  std::shared_ptr<rcl_subscription_t> rcl_sub_handle = sub->get_subscription_handle();
  rmw_subscription_t * rmw_sub = rcl_subscription_get_rmw_handle(rcl_sub_handle.get());

  std::vector<std_msgs::msg::Int32> msgs(kN);
  std::vector<void *> ptrs(kN);
  for (int i = 0; i < kN; ++i) {
    ptrs[i] = &msgs[i];
  }
  std::vector<rmw_message_info_t> infos(kN);

  rcutils_allocator_t alloc = rcutils_get_default_allocator();
  rmw_message_sequence_t seq = rmw_get_zero_initialized_message_sequence();
  seq.data = ptrs.data();
  seq.capacity = static_cast<size_t>(kN);
  seq.size = 0;
  seq.allocator = &alloc;
  rmw_message_info_sequence_t info_seq = rmw_get_zero_initialized_message_info_sequence();
  info_seq.data = infos.data();
  info_seq.capacity = static_cast<size_t>(kN);
  info_seq.size = 0;
  info_seq.allocator = &alloc;

  size_t taken = 0;
  rmw_ret_t rc = RMW_RET_ERROR;
  if (rmw_sub != nullptr) {
    rc = rmw_take_sequence(rmw_sub, static_cast<size_t>(kN), &seq, &info_seq, &taken, nullptr);
  }

  // A single call must return more than one sample (a real batch) and set the sequence sizes.
  bool ok = matched && (rc == RMW_RET_OK) && (taken > 1) &&
    (seq.size == taken) && (info_seq.size == taken);
  std::printf(
    "matched=%d take_sequence rc=%d taken=%zu (expect %d in one batch call) seq.size=%zu\n",
    static_cast<int>(matched), static_cast<int>(rc), taken, kN, seq.size);
  for (size_t i = 0; i < taken; ++i) {
    const int expect = 100 + static_cast<int>(i);
    std::printf("  msg[%zu]=%d (expect %d)\n", i, msgs[i].data, expect);
    if (msgs[i].data != expect) {
      ok = false;
    }
  }

  rclcpp::shutdown();
  std::printf("RESULT: %s\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
#endif
}
