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

#ifndef COMMON__TYPE_HASH_QOS_HPP_
#define COMMON__TYPE_HASH_QOS_HPP_

#include <cstdint>
#include <string>

#include "rosidl_runtime_c/message_type_support_struct.h"
#include "rosidl_runtime_c/service_type_support_struct.h"

#include "rmw/types.h"

#include "int2dds-ffi.h"  // NOLINT(build/include_subdir): vendored FFI header

// ROS 2 Iron and newer (e.g. Jazzy) expose rosidl type hash hooks and the
// rmw_dds_common helper used to embed the type hash into DDS user_data QoS for
// type-compatibility matching. ROS 2 Humble does not. Feature-detect so a single
// source builds on both distros: real implementation where the API exists,
// empty user_data fallback where it does not.
#if __has_include("rosidl_runtime_c/type_hash.h")
#define RMW_INT2DDS_HAS_TYPE_HASH 1
#include "rmw/error_handling.h"
#include "rmw_dds_common/qos.hpp"
#endif

namespace rmw_int2dds_cpp
{

// Resolve system-default / unknown QoS policies to the concrete defaults that
// int2dds actually applies, so rmw_*_get_actual_qos reports usable values (the
// RMW conformance suite requires actual QoS to differ from system_default/unknown).
inline void
resolve_actual_qos(rmw_qos_profile_t * qos)
{
  if (qos == nullptr) {
    return;
  }
  if (qos->history == RMW_QOS_POLICY_HISTORY_SYSTEM_DEFAULT ||
    qos->history == RMW_QOS_POLICY_HISTORY_UNKNOWN)
  {
    qos->history = RMW_QOS_POLICY_HISTORY_KEEP_LAST;
  }
  if (qos->depth == 0) {
    qos->depth = 10;
  }
  if (qos->reliability == RMW_QOS_POLICY_RELIABILITY_SYSTEM_DEFAULT ||
    qos->reliability == RMW_QOS_POLICY_RELIABILITY_UNKNOWN)
  {
    qos->reliability = RMW_QOS_POLICY_RELIABILITY_RELIABLE;
  }
  if (qos->durability == RMW_QOS_POLICY_DURABILITY_SYSTEM_DEFAULT ||
    qos->durability == RMW_QOS_POLICY_DURABILITY_UNKNOWN)
  {
    qos->durability = RMW_QOS_POLICY_DURABILITY_VOLATILE;
  }
  if (qos->liveliness == RMW_QOS_POLICY_LIVELINESS_SYSTEM_DEFAULT ||
    qos->liveliness == RMW_QOS_POLICY_LIVELINESS_UNKNOWN)
  {
    qos->liveliness = RMW_QOS_POLICY_LIVELINESS_AUTOMATIC;
  }
  // A default ({0,0}) deadline/lifespan/lease duration resolves to infinite, which is
  // the effective DDS default. get_actual_qos must report that (not zero) for a default
  // profile, otherwise the value compares below the expected infinite duration.
  const rmw_time_t infinite_duration = RMW_DURATION_INFINITE;
  if (qos->deadline.sec == 0 && qos->deadline.nsec == 0) {
    qos->deadline = infinite_duration;
  }
  if (qos->lifespan.sec == 0 && qos->lifespan.nsec == 0) {
    qos->lifespan = infinite_duration;
  }
  if (qos->liveliness_lease_duration.sec == 0 && qos->liveliness_lease_duration.nsec == 0) {
    qos->liveliness_lease_duration = infinite_duration;
  }
}

inline std::string
encode_message_type_hash_user_data(const rosidl_message_type_support_t * type_support)
{
#ifdef RMW_INT2DDS_HAS_TYPE_HASH
  if (type_support == nullptr || type_support->get_type_hash_func == nullptr) {
    return {};
  }

  const rosidl_type_hash_t * type_hash = type_support->get_type_hash_func(type_support);
  if (type_hash == nullptr) {
    return {};
  }

  std::string encoded;
  if (RMW_RET_OK != rmw_dds_common::encode_type_hash_for_user_data_qos(*type_hash, encoded)) {
    rmw_reset_error();
    return {};
  }

  return encoded;
#else
  // ROS 2 Humble does not expose the newer rosidl type hash hooks used for
  // DDS user_data QoS matching. Leave user_data empty on this distro.
  (void)type_support;
  return {};
#endif
}

inline std::string
encode_service_request_type_hash_user_data(const rosidl_service_type_support_t * type_support)
{
  if (type_support == nullptr || type_support->request_typesupport == nullptr) {
    return {};
  }
  return encode_message_type_hash_user_data(type_support->request_typesupport);
}

inline std::string
encode_service_response_type_hash_user_data(const rosidl_service_type_support_t * type_support)
{
  if (type_support == nullptr || type_support->response_typesupport == nullptr) {
    return {};
  }
  return encode_message_type_hash_user_data(type_support->response_typesupport);
}

// REP-2011 hash of the *service* type as a whole, distinct from the per-message
// request/response hashes above.
inline const rosidl_type_hash_t *
get_service_type_hash(const rosidl_service_type_support_t * type_support)
{
#ifdef RMW_INT2DDS_HAS_TYPE_HASH
  if (type_support == nullptr || type_support->get_type_hash_func == nullptr) {
    return nullptr;
  }
  return type_support->get_type_hash_func(type_support);
#else
  (void)type_support;
  return nullptr;
#endif
}

// ROS 2 Lyrical transports the service type hash in USER_DATA as "sertypehash=...;"
// (appended after the message "typehash=...;" pair) and records it in the GraphCache
// so rmw_get_clients/servers_info_by_service can report it. Older distros have
// neither the encoder nor the GraphCache parameter; collapse to an empty string.
inline std::string
encode_service_type_hash_user_data(const rosidl_service_type_support_t * type_support)
{
#if defined(RMW_INT2DDS_HAS_TYPE_HASH) && __has_include("rmw/get_service_endpoint_info.h")
  const rosidl_type_hash_t * service_type_hash = get_service_type_hash(type_support);
  if (service_type_hash == nullptr) {
    return {};
  }
  std::string encoded;
  if (RMW_RET_OK !=
    rmw_dds_common::encode_sertype_hash_for_user_data_qos(*service_type_hash, encoded))
  {
    rmw_reset_error();
    return {};
  }
  return encoded;
#else
  (void)type_support;
  return {};
#endif
}

inline void
apply_type_hash_user_data(Int2DdsDataWriterQos * qos, const std::string & user_data)
{
  if (qos == nullptr || user_data.empty()) {
    return;
  }

  int2dds_datawriter_qos_set_user_data(
    qos,
    reinterpret_cast<const uint8_t *>(user_data.data()),
    user_data.size());
}

inline void
apply_type_hash_user_data(Int2DdsDataReaderQos * qos, const std::string & user_data)
{
  if (qos == nullptr || user_data.empty()) {
    return;
  }

  int2dds_datareader_qos_set_user_data(
    qos,
    reinterpret_cast<const uint8_t *>(user_data.data()),
    user_data.size());
}

}  // namespace rmw_int2dds_cpp

#endif  // COMMON__TYPE_HASH_QOS_HPP_
