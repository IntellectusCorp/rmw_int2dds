// Copyright 2024 Int2DDS Project
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

#ifndef GRAPH__DISCOVERY_HPP_
#define GRAPH__DISCOVERY_HPP_

#include <string>

#include "rmw/qos_profiles.h"
#include "rmw/ret_types.h"
#include "rmw/types.h"

namespace rmw_int2dds_cpp
{

struct ContextData;

// Stand up the standard rmw_dds_common node-graph discovery for this context:
// an rmw_dds_common::Context (holding the GraphCache) plus the ros_discovery_info
// publisher/subscriber and the listener thread. Built up across phases.
rmw_ret_t init_discovery(ContextData * context_data, const char * enclave);

// Tear down discovery: join the listener thread and release the Context.
void fini_discovery(ContextData * context_data);

// Publish local node graph changes through ros_discovery_info.
rmw_ret_t announce_node(
  ContextData * context_data, const std::string & name,
  const std::string & ns);
rmw_ret_t withdraw_node(
  ContextData * context_data, const std::string & name,
  const std::string & ns);

// Mirror local DDS endpoints into rmw_dds_common GraphCache and publish node
// associations over ros_discovery_info. Humble's rmw_dds_common::Context does
// not provide the Jazzy add_*_graph wrappers, so the RMW calls these adapters.
rmw_ret_t common_add_local_entity(
  ContextData * context_data,
  const rmw_gid_t & gid,
  const std::string & topic_name,
  const std::string & type_name,
  const rmw_qos_profile_t & qos,
  bool is_reader);

rmw_ret_t common_remove_local_entity(
  ContextData * context_data,
  const rmw_gid_t & gid,
  bool is_reader);

rmw_ret_t common_associate_local_writer(
  ContextData * context_data,
  const rmw_gid_t & writer_gid,
  const std::string & node_name,
  const std::string & node_namespace);

rmw_ret_t common_dissociate_local_writer(
  ContextData * context_data,
  const rmw_gid_t & writer_gid,
  const std::string & node_name,
  const std::string & node_namespace);

rmw_ret_t common_associate_local_reader(
  ContextData * context_data,
  const rmw_gid_t & reader_gid,
  const std::string & node_name,
  const std::string & node_namespace);

rmw_ret_t common_dissociate_local_reader(
  ContextData * context_data,
  const rmw_gid_t & reader_gid,
  const std::string & node_name,
  const std::string & node_namespace);

}  // namespace rmw_int2dds_cpp

#endif  // GRAPH__DISCOVERY_HPP_
