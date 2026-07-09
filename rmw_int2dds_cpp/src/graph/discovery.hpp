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

#include "rmw/ret_types.h"
#include "rmw/types.h"
#include "rosidl_runtime_c/type_hash.h"

namespace rmw_int2dds_cpp
{

struct ContextData;

// Stand up the standard rmw_dds_common node-graph discovery for this context:
// an rmw_dds_common::Context (holding the GraphCache) plus the ros_discovery_info
// publisher/subscriber and the listener thread. Built up across phases.
rmw_ret_t init_discovery(ContextData * context_data, const char * enclave);

// Tear down discovery: join the listener thread and release the Context.
void fini_discovery(ContextData * context_data);

// Register / withdraw a locally-created endpoint in the standard rmw_dds_common
// GraphCache entity layer (topic name + type), keyed by its DDS endpoint GUID
// (`gid`). The node-association layer is handled separately by the caller via
// the rmw_dds_common Context add_*_graph / remove_*_graph helpers. No-ops when
// the common discovery context is absent.
// `service_type_hash` is only meaningful for service/client endpoints: on ROS 2
// Lyrical+ the GraphCache records it so rmw_get_clients/servers_info_by_service
// reports it; on older distros it is ignored.
void common_add_local_entity(
  ContextData * context_data,
  const rmw_gid_t & gid,
  const std::string & dds_topic_name,
  const std::string & type_name,
  const rosidl_type_hash_t & type_hash,
  const rmw_qos_profile_t & qos,
  bool is_reader,
  const rosidl_type_hash_t * service_type_hash = nullptr);
void common_remove_local_entity(
  ContextData * context_data,
  const rmw_gid_t & gid,
  bool is_reader);

}  // namespace rmw_int2dds_cpp

#endif  // GRAPH__DISCOVERY_HPP_
