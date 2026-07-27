#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>

#include "rcutils/allocator.h"
#include "rcutils/strdup.h"

#include "rmw/event.h"
#include "rmw/events_statuses/matched.h"
#include "rmw/error_handling.h"
#include "rmw/init.h"
#include "rmw/init_options.h"
#include "rmw/publisher_options.h"
#include "rmw/qos_profiles.h"
#include "rmw/rmw.h"
#include "rmw/subscription_options.h"

#include "rosidl_typesupport_cpp/message_type_support.hpp"
#include "std_msgs/msg/string.hpp"

namespace
{

void
print_error_and_return(const char * label)
{
  const rmw_error_string_t error_string = rmw_get_error_string();
  std::cerr << "[ERROR] " << label;
  if (error_string.str[0] != '\0') {
    std::cerr << ": " << error_string.str;
  }
  std::cerr << std::endl;
  rmw_reset_error();
}

bool
check_ret(rmw_ret_t ret, const char * label)
{
  if (ret == RMW_RET_OK) {
    return true;
  }
  print_error_and_return(label);
  return false;
}

void
ignore_ret(rmw_ret_t ret)
{
  (void)ret;
}

}  // namespace

int
main()
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();

  rmw_init_options_t init_options = rmw_get_zero_initialized_init_options();
  if (!check_ret(rmw_init_options_init(&init_options, allocator), "rmw_init_options_init")) {
    return EXIT_FAILURE;
  }

  init_options.enclave = rcutils_strdup("/", allocator);
  if (init_options.enclave == nullptr) {
    std::cerr << "[ERROR] failed to allocate enclave" << std::endl;
    ignore_ret(rmw_init_options_fini(&init_options));
    return EXIT_FAILURE;
  }

  rmw_context_t context = rmw_get_zero_initialized_context();
  if (!check_ret(rmw_init(&init_options, &context), "rmw_init")) {
    ignore_ret(rmw_init_options_fini(&init_options));
    return EXIT_FAILURE;
  }

  rmw_node_t * subscriber_node = rmw_create_node(&context, "event_sub_node", "/");
  if (subscriber_node == nullptr) {
    print_error_and_return("rmw_create_node(subscriber)");
    ignore_ret(rmw_shutdown(&context));
    ignore_ret(rmw_context_fini(&context));
    ignore_ret(rmw_init_options_fini(&init_options));
    return EXIT_FAILURE;
  }

  rmw_node_t * publisher_node = rmw_create_node(&context, "event_pub_node", "/");
  if (publisher_node == nullptr) {
    print_error_and_return("rmw_create_node(publisher)");
    ignore_ret(rmw_destroy_node(subscriber_node));
    ignore_ret(rmw_shutdown(&context));
    ignore_ret(rmw_context_fini(&context));
    ignore_ret(rmw_init_options_fini(&init_options));
    return EXIT_FAILURE;
  }

  const auto * type_support =
    rosidl_typesupport_cpp::get_message_type_support_handle<std_msgs::msg::String>();

  rmw_subscription_options_t subscription_options = rmw_get_default_subscription_options();
  rmw_subscription_t * subscription = rmw_create_subscription(
    subscriber_node,
    type_support,
    "/event_smoke_topic",
    &rmw_qos_profile_default,
    &subscription_options);
  if (subscription == nullptr) {
    print_error_and_return("rmw_create_subscription");
    ignore_ret(rmw_destroy_node(publisher_node));
    ignore_ret(rmw_destroy_node(subscriber_node));
    ignore_ret(rmw_shutdown(&context));
    ignore_ret(rmw_context_fini(&context));
    ignore_ret(rmw_init_options_fini(&init_options));
    return EXIT_FAILURE;
  }

  rmw_event_t event = rmw_get_zero_initialized_event();
  if (!check_ret(
      rmw_subscription_event_init(&event, subscription, RMW_EVENT_SUBSCRIPTION_MATCHED),
      "rmw_subscription_event_init"))
  {
    ignore_ret(rmw_destroy_subscription(subscriber_node, subscription));
    ignore_ret(rmw_destroy_node(publisher_node));
    ignore_ret(rmw_destroy_node(subscriber_node));
    ignore_ret(rmw_shutdown(&context));
    ignore_ret(rmw_context_fini(&context));
    ignore_ret(rmw_init_options_fini(&init_options));
    return EXIT_FAILURE;
  }

  rmw_wait_set_t * wait_set = rmw_create_wait_set(&context, 1);
  if (wait_set == nullptr) {
    print_error_and_return("rmw_create_wait_set");
    ignore_ret(rmw_destroy_subscription(subscriber_node, subscription));
    ignore_ret(rmw_destroy_node(publisher_node));
    ignore_ret(rmw_destroy_node(subscriber_node));
    ignore_ret(rmw_shutdown(&context));
    ignore_ret(rmw_context_fini(&context));
    ignore_ret(rmw_init_options_fini(&init_options));
    return EXIT_FAILURE;
  }

  rmw_publisher_options_t publisher_options = rmw_get_default_publisher_options();
  rmw_publisher_t * publisher = rmw_create_publisher(
    publisher_node,
    type_support,
    "/event_smoke_topic",
    &rmw_qos_profile_default,
    &publisher_options);
  if (publisher == nullptr) {
    print_error_and_return("rmw_create_publisher");
    ignore_ret(rmw_destroy_wait_set(wait_set));
    ignore_ret(rmw_destroy_subscription(subscriber_node, subscription));
    ignore_ret(rmw_destroy_node(publisher_node));
    ignore_ret(rmw_destroy_node(subscriber_node));
    ignore_ret(rmw_shutdown(&context));
    ignore_ret(rmw_context_fini(&context));
    ignore_ret(rmw_init_options_fini(&init_options));
    return EXIT_FAILURE;
  }

  rmw_ret_t wait_ret = RMW_RET_TIMEOUT;
  rmw_matched_status_t matched_status{};
  bool taken = false;
  bool success = false;

  for (int attempt = 0; attempt < 20 && !success; ++attempt) {
    void * event_entries[1] = {&event};
    rmw_events_t events{1, event_entries};
    rmw_time_t timeout{0, 200000000};

    wait_ret = rmw_wait(nullptr, nullptr, nullptr, nullptr, &events, wait_set, &timeout);
    if (wait_ret == RMW_RET_TIMEOUT) {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      continue;
    }
    if (!check_ret(wait_ret, "rmw_wait")) {
      break;
    }
    if (events.events[0] == nullptr) {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      continue;
    }

    if (!check_ret(rmw_take_event(&event, &matched_status, &taken), "rmw_take_event")) {
      break;
    }

    if (taken && matched_status.current_count >= 1) {
      success = true;
    }
  }

  std::cout << "wait_ret=" << wait_ret
            << " taken=" << taken
            << " total_count=" << matched_status.total_count
            << " total_count_change=" << matched_status.total_count_change
            << " current_count=" << matched_status.current_count
            << " current_count_change=" << matched_status.current_count_change
            << std::endl;

  ignore_ret(rmw_event_fini(&event));
  ignore_ret(rmw_destroy_publisher(publisher_node, publisher));
  ignore_ret(rmw_destroy_subscription(subscriber_node, subscription));
  ignore_ret(rmw_destroy_wait_set(wait_set));
  ignore_ret(rmw_destroy_node(publisher_node));
  ignore_ret(rmw_destroy_node(subscriber_node));
  ignore_ret(rmw_shutdown(&context));
  ignore_ret(rmw_context_fini(&context));
  ignore_ret(rmw_init_options_fini(&init_options));

  if (!success) {
    std::cerr << "[FAIL] matched event smoke test did not observe a ready event" << std::endl;
    return EXIT_FAILURE;
  }

  std::cout << "[OK] matched event smoke test passed" << std::endl;
  return EXIT_SUCCESS;
}
