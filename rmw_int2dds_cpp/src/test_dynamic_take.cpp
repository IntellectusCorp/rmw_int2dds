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
// C3 receive-path verification. Publishes a concrete message, then pulls it back through
// rmw_take_dynamic_message into a dynamically-built message and compares field values.
// Unlike test_dynamic_message (which drives the serialization-support vtable directly),
// this exercises the real subscription receive path in rmw_take.cpp end to end. The read
// is intentionally not driven by an executor: the sample is delivered to the datareader
// by DDS, and rmw_take_dynamic_message pulls it directly.

#include <chrono>
#include <cstdio>
#include <memory>
#include <string>
#include <thread>

#include "rclcpp/rclcpp.hpp"
#include "rcl/subscription.h"

#include "rmw/rmw.h"
#include "rmw/dynamic_message_type_support.h"

#include "rosidl_dynamic_typesupport/api/serialization_support.h"
#include "rosidl_dynamic_typesupport/api/dynamic_type.h"
#include "rosidl_dynamic_typesupport/api/dynamic_data.h"
#include "rosidl_dynamic_typesupport/types.h"

#if __has_include("diagnostic_msgs/msg/key_value.hpp")
#include "diagnostic_msgs/msg/key_value.hpp"
#define HAVE_KEYVALUE 1
#endif

int main(int argc, char ** argv)
{
#ifndef HAVE_KEYVALUE
  (void)argc;
  (void)argv;
  std::printf("SKIP: diagnostic_msgs unavailable\n");
  return 0;
#else
  rclcpp::init(argc, argv);
  auto node = std::make_shared<rclcpp::Node>("test_dynamic_take");
  rclcpp::QoS qos(10);
  qos.reliable();

  auto pub = node->create_publisher<diagnostic_msgs::msg::KeyValue>("dyn_take_topic", qos);
  auto sub = node->create_subscription<diagnostic_msgs::msg::KeyValue>(
    "dyn_take_topic", qos, [](diagnostic_msgs::msg::KeyValue::SharedPtr) {});

  // Wait for the publisher and subscription to match before publishing.
  auto match_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (pub->get_subscription_count() < 1 &&
    std::chrono::steady_clock::now() < match_deadline)
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  const bool matched = pub->get_subscription_count() >= 1;
  std::printf("matched=%d\n", static_cast<int>(matched));

  diagnostic_msgs::msg::KeyValue msg;
  msg.key = "take-key";
  msg.value = "take-value-42";
  pub->publish(msg);

  // Build a dynamic message for KeyValue through this rmw's serialization support.
  rcutils_allocator_t alloc = rcutils_get_default_allocator();
  rosidl_dynamic_typesupport_serialization_support_t ss =
    rosidl_dynamic_typesupport_get_zero_initialized_serialization_support();
  bool ok = false;
  if (rmw_serialization_support_init("", &alloc, &ss) == RMW_RET_OK) {
    rosidl_dynamic_typesupport_dynamic_type_builder_t bld =
      rosidl_dynamic_typesupport_get_zero_initialized_dynamic_type_builder();
    rosidl_dynamic_typesupport_dynamic_type_t type =
      rosidl_dynamic_typesupport_get_zero_initialized_dynamic_type();
    rosidl_dynamic_typesupport_dynamic_data_t data =
      rosidl_dynamic_typesupport_get_zero_initialized_dynamic_data();
    rcutils_ret_t rc = RCUTILS_RET_OK;
#define STEP(call) do {if (rc == RCUTILS_RET_OK) {rc = (call);}} while (0)
    STEP(rosidl_dynamic_typesupport_dynamic_type_builder_init(
        &ss, "diagnostic_msgs::msg::KeyValue", 30, &alloc, &bld));
    STEP(rosidl_dynamic_typesupport_dynamic_type_builder_add_string_member(
        &bld, 0, "key", 3, "", 0));
    STEP(rosidl_dynamic_typesupport_dynamic_type_builder_add_string_member(
        &bld, 1, "value", 5, "", 0));
    STEP(rosidl_dynamic_typesupport_dynamic_type_init_from_dynamic_type_builder(
        &bld, &alloc, &type));
    STEP(rosidl_dynamic_typesupport_dynamic_data_init_from_dynamic_type(&type, &alloc, &data));
#undef STEP

    // Resolve the rmw subscription handle (hold the shared_ptr for the pointer's lifetime).
    std::shared_ptr<rcl_subscription_t> rcl_sub_handle = sub->get_subscription_handle();
    rmw_subscription_t * rmw_sub = rcl_subscription_get_rmw_handle(rcl_sub_handle.get());

    bool taken = false;
    rmw_ret_t last_take_ret = RMW_RET_OK;
    if (rc == RCUTILS_RET_OK && rmw_sub != nullptr) {
      auto take_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
      while (!taken && std::chrono::steady_clock::now() < take_deadline) {
        last_take_ret = rmw_take_dynamic_message(rmw_sub, &data, &taken, nullptr);
        if (last_take_ret != RMW_RET_OK) {
          break;
        }
        if (!taken) {
          std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
      }
    }

    std::string key_out;
    std::string value_out;
    rcutils_ret_t g1 = RCUTILS_RET_ERROR;
    rcutils_ret_t g2 = RCUTILS_RET_ERROR;
    if (taken) {
      char * ks = nullptr;
      size_t klen = 0;
      char * vs = nullptr;
      size_t vlen = 0;
      g1 = rosidl_dynamic_typesupport_dynamic_data_get_string_value(&data, 0, &ks, &klen);
      g2 = rosidl_dynamic_typesupport_dynamic_data_get_string_value(&data, 1, &vs, &vlen);
      if (g1 == RCUTILS_RET_OK && g2 == RCUTILS_RET_OK) {
        key_out.assign(ks, klen);
        value_out.assign(vs, vlen);
      }
      delete[] ks;  // owned by caller (new char[] in the vtable getter)
      delete[] vs;
    }
    std::printf(
      "matched=%d take_ret=%d taken=%d get=(%d,%d) key='%s' value='%s' "
      "(expect 'take-key'/'take-value-42')\n",
      static_cast<int>(matched), static_cast<int>(last_take_ret), static_cast<int>(taken),
      static_cast<int>(g1), static_cast<int>(g2), key_out.c_str(), value_out.c_str());
    ok = (matched && taken && key_out == "take-key" && value_out == "take-value-42");
  } else {
    std::printf("FAIL: rmw_serialization_support_init\n");
  }

  rclcpp::shutdown();
  std::printf("RESULT: %s\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
#endif
}
