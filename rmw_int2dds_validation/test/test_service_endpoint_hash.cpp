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
// Regression check: rmw_get_{servers,clients}_info_by_service must report a
// service_type_hash for REMOTE endpoints, not only local ones.
//
// rmw_int2dds_cpp parses that hash out of the endpoint's USER_DATA and hands it
// to rmw_dds_common's GraphCache::add_entity. Two code paths register a remote
// endpoint - the SEDP endpoint-discovery push callback and
// sync_remote_entities_to_common - and they share one synced_remote_entities
// dedup map, so whichever sees an endpoint first is the only one that ever
// registers it. The push callback normally wins. It used to call add_entity
// without the service hash (that parameter defaults to nullptr, so nothing
// warned). Observed effect with that defect in place: rmw_dds_common cannot
// pair a request/reply endpoint that carries no service hash, so the query does
// not merely report version 0 - it fails outright with RMW_RET_ERROR, for every
// remote peer, permanently. Local endpoints looked correct throughout, because
// common_add_local_entity does pass the hash.
//
// Two rclcpp contexts give two DDS participants inside one process, which is
// all it takes for one side's endpoints to be remote to the other - so this
// reproduces without a second process.
//
// A service contributes a request reader and a reply writer; a client
// contributes a reply reader and a request writer. Querying both therefore
// exercises the reader branch and the writer branch of the push callback from
// both directions - dropping the hash in either one fails this check.

#include <chrono>
#include <cstdio>
#include <memory>
#include <string>
#include <thread>

#include "rclcpp/rclcpp.hpp"
#include "rcl/node.h"
#include "rcutils/allocator.h"
#include "rmw/rmw.h"
#include "rmw/get_service_endpoint_info.h"
#include "rmw/service_endpoint_info_array.h"
#include "std_srvs/srv/trigger.hpp"

namespace
{

constexpr char kServiceName[] = "/service_type_hash_probe";
constexpr char kClientServiceName[] = "/service_type_hash_probe_client";

// Let the push callback ingest the remote endpoints before the first query.
// Without this the very first rmw_get_*_info_by_service could run
// sync_remote_entities_to_common before the callback has fired, and the sync
// path would register the endpoints - which would mask exactly the defect this
// check exists to catch.
constexpr auto kSettleDelay = std::chrono::seconds(2);
constexpr auto kDiscoveryTimeout = std::chrono::seconds(15);
constexpr auto kPollInterval = std::chrono::milliseconds(100);

rmw_node_t * rmw_handle_of(const std::shared_ptr<rclcpp::Node> & node)
{
  rcl_node_t * rcl_node = node->get_node_base_interface()->get_rcl_node_handle();
  if (rcl_node == nullptr) {
    return nullptr;
  }
  return rcl_node_get_rmw_handle(rcl_node);
}

// Polls one query until it reports at least one endpoint carrying a set hash,
// or the timeout expires. Returns true only when a hash was actually seen.
// `found_any` distinguishes "the endpoint never showed up" (a discovery problem
// in the test) from "the endpoint showed up without a hash" (the defect).
bool wait_for_hashed_endpoint(
  rmw_ret_t (* query)(
    const rmw_node_t *, rcutils_allocator_t *, const char *, bool,
    rmw_service_endpoint_info_array_t *),
  const rmw_node_t * observer,
  const char * service_name,
  const char * label,
  bool * found_any,
  bool * query_failed)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  const auto deadline = std::chrono::steady_clock::now() + kDiscoveryTimeout;
  *found_any = false;
  *query_failed = false;

  while (std::chrono::steady_clock::now() < deadline) {
    rmw_service_endpoint_info_array_t infos =
      rmw_get_zero_initialized_service_endpoint_info_array();
    const rmw_ret_t ret = query(observer, &allocator, service_name, false, &infos);
    if (ret != RMW_RET_OK) {
      // This is the shape the defect took: rmw_dds_common refuses to pair
      // endpoints with no service hash, so the query errors rather than
      // returning a hashless entry.
      std::printf("FAIL: %s returned %d: %s\n", label, ret, rmw_get_error_string().str);
      rmw_reset_error();
      *query_failed = true;
      return false;
    }

    bool hashed = false;
    for (size_t i = 0; i < infos.size; ++i) {
      *found_any = true;
      if (infos.info_array[i].service_type_hash.version != 0) {
        hashed = true;
      } else {
        std::printf(
          "      %s[%zu] node=%s%s type=%s -> service_type_hash UNSET\n",
          label, i,
          infos.info_array[i].node_namespace != nullptr ? infos.info_array[i].node_namespace : "?",
          infos.info_array[i].node_name != nullptr ? infos.info_array[i].node_name : "?",
          infos.info_array[i].service_type != nullptr ? infos.info_array[i].service_type : "?");
      }
    }
    const rmw_ret_t fini_ret = rmw_service_endpoint_info_array_fini(&infos, &allocator);
    if (fini_ret != RMW_RET_OK) {
      std::printf("FAIL: %s array fini returned %d\n", label, fini_ret);
      return false;
    }
    if (hashed) {
      return true;
    }
    std::this_thread::sleep_for(kPollInterval);
  }
  return false;
}

}  // namespace

int main(int argc, char ** argv)
{
  // Separate contexts, so each gets its own DDS participant.
  auto provider_context = std::make_shared<rclcpp::Context>();
  provider_context->init(argc, argv);
  auto observer_context = std::make_shared<rclcpp::Context>();
  observer_context->init(argc, argv);

  rclcpp::NodeOptions provider_options;
  provider_options.context(provider_context);
  auto provider_node = std::make_shared<rclcpp::Node>("svc_hash_provider", provider_options);

  auto service = provider_node->create_service<std_srvs::srv::Trigger>(
    kServiceName,
    [](
      const std::shared_ptr<std_srvs::srv::Trigger::Request>,
      std::shared_ptr<std_srvs::srv::Trigger::Response> response) {response->success = true;});
  auto client = provider_node->create_client<std_srvs::srv::Trigger>(kClientServiceName);

  rclcpp::NodeOptions observer_options;
  observer_options.context(observer_context);
  auto observer_node = std::make_shared<rclcpp::Node>("svc_hash_observer", observer_options);

  rmw_node_t * observer = rmw_handle_of(observer_node);
  if (observer == nullptr) {
    std::printf("FAIL: could not reach the observer's rmw node handle\n");
    return 1;
  }

  std::this_thread::sleep_for(kSettleDelay);

  bool saw_server = false;
  bool server_query_failed = false;
  const bool server_hashed = wait_for_hashed_endpoint(
    rmw_get_servers_info_by_service, observer, kServiceName, "servers_info",
    &saw_server, &server_query_failed);

  bool saw_client = false;
  bool client_query_failed = false;
  const bool client_hashed = wait_for_hashed_endpoint(
    rmw_get_clients_info_by_service, observer, kClientServiceName, "clients_info",
    &saw_client, &client_query_failed);

  int rc = 0;
  if (server_query_failed) {
    std::printf(
      "FAIL: rmw_get_servers_info_by_service errored on %s - the usual cause is a "
      "remote endpoint registered without its service type hash\n", kServiceName);
    rc = 1;
  } else if (!saw_server) {
    std::printf(
      "FAIL: the remote server on %s was never discovered - "
      "this is a discovery failure, not a hash failure\n", kServiceName);
    rc = 1;
  } else if (!server_hashed) {
    std::printf(
      "FAIL: the remote server on %s reports an unset service_type_hash\n", kServiceName);
    rc = 1;
  } else {
    std::printf("OK: remote server on %s carries a service_type_hash\n", kServiceName);
  }

  if (client_query_failed) {
    std::printf(
      "FAIL: rmw_get_clients_info_by_service errored on %s - the usual cause is a "
      "remote endpoint registered without its service type hash\n", kClientServiceName);
    rc = 1;
  } else if (!saw_client) {
    std::printf(
      "FAIL: the remote client on %s was never discovered - "
      "this is a discovery failure, not a hash failure\n", kClientServiceName);
    rc = 1;
  } else if (!client_hashed) {
    std::printf(
      "FAIL: the remote client on %s reports an unset service_type_hash\n", kClientServiceName);
    rc = 1;
  } else {
    std::printf("OK: remote client on %s carries a service_type_hash\n", kClientServiceName);
  }

  client.reset();
  service.reset();
  observer_node.reset();
  provider_node.reset();
  observer_context->shutdown("done");
  provider_context->shutdown("done");
  return rc;
}
