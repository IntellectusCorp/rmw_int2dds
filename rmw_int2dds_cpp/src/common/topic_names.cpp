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

#include <string>

#include "rosidl_typesupport_introspection_c/message_introspection.h"
#include "rosidl_typesupport_introspection_c/identifier.h"
#include "rosidl_typesupport_introspection_c/service_introspection.h"
#include "rosidl_typesupport_introspection_cpp/message_introspection.hpp"
#include "rosidl_typesupport_introspection_cpp/identifier.hpp"
#include "rosidl_typesupport_introspection_cpp/service_introspection.hpp"
#include "rosidl_typesupport_c/service_type_support_dispatch.h"
#include "rosidl_typesupport_cpp/service_type_support.hpp"
#include "rmw/error_handling.h"

namespace rmw_int2dds_cpp
{

namespace
{

std::string normalize_rosidl_namespace_colon(const char * rosidl_namespace)
{
  if (rosidl_namespace == nullptr) {
    return "";
  }

  std::string normalized = rosidl_namespace;
  std::string::size_type pos = 0;
  while ((pos = normalized.find("__", pos)) != std::string::npos) {
    normalized.replace(pos, 2, "::");
    pos += 2;
  }
  return normalized;
}

std::string normalize_rosidl_namespace(const char * rosidl_namespace)
{
  if (rosidl_namespace == nullptr) {
    return "";
  }

  std::string normalized = rosidl_namespace;
  std::string::size_type pos = 0;
  while ((pos = normalized.find("__", pos)) != std::string::npos) {
    normalized.replace(pos, 2, "/");
    pos += 1;
  }
  pos = 0;
  while ((pos = normalized.find("::", pos)) != std::string::npos) {
    normalized.replace(pos, 2, "/");
    pos += 1;
  }
  return normalized;
}

}  // namespace

// ROS2 to DDS topic name prefixes
constexpr const char * TOPIC_PREFIX = "rt";
constexpr const char * SERVICE_REQUEST_PREFIX = "rq";
constexpr const char * SERVICE_RESPONSE_PREFIX = "rr";

std::string ros_topic_to_dds_topic(
  const char * ros_topic_name,
  bool avoid_ros_namespace_conventions)
{
  if (avoid_ros_namespace_conventions) {
    return std::string(ros_topic_name);
  }

  // ROS topic: /my_topic -> DDS topic: rt/my_topic
  std::string dds_topic = TOPIC_PREFIX;
  dds_topic += ros_topic_name;
  return dds_topic;
}

std::string ros_service_to_dds_request_topic(const char * service_name)
{
  // ROS service: /add_two_ints -> DDS request topic: rq/add_two_intsRequest
  std::string dds_topic = SERVICE_REQUEST_PREFIX;
  dds_topic += service_name;
  dds_topic += "Request";
  return dds_topic;
}

std::string ros_service_to_dds_response_topic(const char * service_name)
{
  // ROS service: /add_two_ints -> DDS response topic: rr/add_two_intsResponse
  std::string dds_topic = SERVICE_RESPONSE_PREFIX;
  dds_topic += service_name;
  dds_topic += "Response";
  return dds_topic;
}

std::string dds_topic_to_ros_topic(const std::string & dds_topic_name)
{
  // Check for topic prefix
  if (dds_topic_name.compare(0, 2, TOPIC_PREFIX) == 0) {
    return dds_topic_name.substr(2);
  }
  return dds_topic_name;
}

std::string create_fully_qualified_name(
  const char * node_namespace,
  const char * node_name)
{
  std::string fqn;

  if (node_namespace && node_namespace[0] != '\0') {
    fqn = node_namespace;
    if (fqn.back() != '/') {
      fqn += '/';
    }
  } else {
    fqn = "/";
  }

  fqn += node_name;
  return fqn;
}

std::string get_type_name_from_type_support(
  const rosidl_message_type_support_t * type_support)
{
  // This will be implemented when we have proper type support introspection
  // For now, return a generic type name
  (void)type_support;
  return "RawData";
}

std::string get_type_name(const rosidl_message_type_support_t * type_support)
{
  if (type_support == nullptr) {
    return "UnknownType";
  }

  // Try C type support introspection
  if (type_support->typesupport_identifier ==
    rosidl_typesupport_introspection_c__identifier)
  {
    const auto * members =
      static_cast<const rosidl_typesupport_introspection_c__MessageMembers *>(
      type_support->data);
    if (members != nullptr) {
      std::string type_name = normalize_rosidl_namespace(members->message_namespace_);
      type_name += "/";
      type_name += members->message_name_;
      return type_name;
    }
  }

  // Try C++ type support introspection
  if (type_support->typesupport_identifier ==
    rosidl_typesupport_introspection_cpp::typesupport_identifier)
  {
    const auto * members =
      static_cast<const rosidl_typesupport_introspection_cpp::MessageMembers *>(
      type_support->data);
    if (members != nullptr) {
      std::string type_name = normalize_rosidl_namespace(members->message_namespace_);
      type_name += "/";
      type_name += members->message_name_;
      return type_name;
    }
  }

  return "UnknownType";
}

std::string get_dds_type_name(const rosidl_message_type_support_t * type_support)
{
  if (type_support == nullptr) {
    return "UnknownType";
  }

  if (type_support->typesupport_identifier ==
    rosidl_typesupport_introspection_c__identifier)
  {
    const auto * members =
      static_cast<const rosidl_typesupport_introspection_c__MessageMembers *>(
      type_support->data);
    if (members != nullptr) {
      std::string type_name = normalize_rosidl_namespace_colon(members->message_namespace_);
      type_name += "::dds_::";
      type_name += members->message_name_;
      type_name += "_";
      return type_name;
    }
  }

  if (type_support->typesupport_identifier ==
    rosidl_typesupport_introspection_cpp::typesupport_identifier)
  {
    const auto * members =
      static_cast<const rosidl_typesupport_introspection_cpp::MessageMembers *>(
      type_support->data);
    if (members != nullptr) {
      std::string type_name = normalize_rosidl_namespace_colon(members->message_namespace_);
      type_name += "::dds_::";
      type_name += members->message_name_;
      type_name += "_";
      return type_name;
    }
  }

  return "UnknownType";
}

std::string ros_topic_to_dds_topic(const std::string & ros_topic_name)
{
  return ros_topic_to_dds_topic(ros_topic_name.c_str(), false);
}

std::string get_service_request_type_name(const rosidl_service_type_support_t * type_support)
{
  if (type_support == nullptr) {
    return "UnknownServiceRequest";
  }

  // Try C type support introspection
  const rosidl_service_type_support_t * c_ts = get_service_typesupport_handle(
    type_support, rosidl_typesupport_introspection_c__identifier);
  if (c_ts != nullptr) {
    const auto * members =
      static_cast<const rosidl_typesupport_introspection_c__ServiceMembers *>(c_ts->data);
    if (members != nullptr && members->request_members_ != nullptr) {
      std::string type_name = normalize_rosidl_namespace(
        members->request_members_->message_namespace_);
      type_name += "/";
      type_name += members->service_name_;
      type_name += "_Request";
      return type_name;
    }
  }

  // Try C++ type support introspection
  // The introspection_c lookup above fails (and sets an rmw error) for C++
  // typesupport services; clear it so a recovered lookup leaves no stale error.
  rmw_reset_error();
  const rosidl_service_type_support_t * cpp_ts = get_service_typesupport_handle(
    type_support, rosidl_typesupport_introspection_cpp::typesupport_identifier);
  if (cpp_ts != nullptr) {
    const auto * members =
      static_cast<const rosidl_typesupport_introspection_cpp::ServiceMembers *>(cpp_ts->data);
    if (members != nullptr && members->request_members_ != nullptr) {
      std::string type_name = normalize_rosidl_namespace(
        members->request_members_->message_namespace_);
      type_name += "/";
      type_name += members->service_name_;
      type_name += "_Request";
      return type_name;
    }
  }

  return "UnknownServiceRequest";
}

std::string get_dds_service_request_type_name(const rosidl_service_type_support_t * type_support)
{
  if (type_support == nullptr) {
    return "UnknownServiceRequest";
  }

  const rosidl_service_type_support_t * c_ts = get_service_typesupport_handle(
    type_support, rosidl_typesupport_introspection_c__identifier);
  if (c_ts != nullptr) {
    const auto * members =
      static_cast<const rosidl_typesupport_introspection_c__ServiceMembers *>(c_ts->data);
    if (members != nullptr && members->request_members_ != nullptr) {
      std::string type_name = normalize_rosidl_namespace_colon(
        members->request_members_->message_namespace_);
      type_name += "::dds_::";
      type_name += members->service_name_;
      type_name += "_Request_";
      return type_name;
    }
  }

  // The introspection_c lookup above fails (and sets an rmw error) for C++
  // typesupport services; clear it so a recovered lookup leaves no stale error.
  rmw_reset_error();
  const rosidl_service_type_support_t * cpp_ts = get_service_typesupport_handle(
    type_support, rosidl_typesupport_introspection_cpp::typesupport_identifier);
  if (cpp_ts != nullptr) {
    const auto * members =
      static_cast<const rosidl_typesupport_introspection_cpp::ServiceMembers *>(cpp_ts->data);
    if (members != nullptr && members->request_members_ != nullptr) {
      std::string type_name = normalize_rosidl_namespace_colon(
        members->request_members_->message_namespace_);
      type_name += "::dds_::";
      type_name += members->service_name_;
      type_name += "_Request_";
      return type_name;
    }
  }

  return "UnknownServiceRequest";
}

std::string get_service_response_type_name(const rosidl_service_type_support_t * type_support)
{
  if (type_support == nullptr) {
    return "UnknownServiceResponse";
  }

  // Try C type support introspection
  const rosidl_service_type_support_t * c_ts = get_service_typesupport_handle(
    type_support, rosidl_typesupport_introspection_c__identifier);
  if (c_ts != nullptr) {
    const auto * members =
      static_cast<const rosidl_typesupport_introspection_c__ServiceMembers *>(c_ts->data);
    if (members != nullptr && members->response_members_ != nullptr) {
      std::string type_name = normalize_rosidl_namespace(
        members->response_members_->message_namespace_);
      type_name += "/";
      type_name += members->service_name_;
      type_name += "_Response";
      return type_name;
    }
  }

  // Try C++ type support introspection
  // The introspection_c lookup above fails (and sets an rmw error) for C++
  // typesupport services; clear it so a recovered lookup leaves no stale error.
  rmw_reset_error();
  const rosidl_service_type_support_t * cpp_ts = get_service_typesupport_handle(
    type_support, rosidl_typesupport_introspection_cpp::typesupport_identifier);
  if (cpp_ts != nullptr) {
    const auto * members =
      static_cast<const rosidl_typesupport_introspection_cpp::ServiceMembers *>(cpp_ts->data);
    if (members != nullptr && members->response_members_ != nullptr) {
      std::string type_name = normalize_rosidl_namespace(
        members->response_members_->message_namespace_);
      type_name += "/";
      type_name += members->service_name_;
      type_name += "_Response";
      return type_name;
    }
  }

  return "UnknownServiceResponse";
}

std::string get_dds_service_response_type_name(const rosidl_service_type_support_t * type_support)
{
  if (type_support == nullptr) {
    return "UnknownServiceResponse";
  }

  const rosidl_service_type_support_t * c_ts = get_service_typesupport_handle(
    type_support, rosidl_typesupport_introspection_c__identifier);
  if (c_ts != nullptr) {
    const auto * members =
      static_cast<const rosidl_typesupport_introspection_c__ServiceMembers *>(c_ts->data);
    if (members != nullptr && members->response_members_ != nullptr) {
      std::string type_name = normalize_rosidl_namespace_colon(
        members->response_members_->message_namespace_);
      type_name += "::dds_::";
      type_name += members->service_name_;
      type_name += "_Response_";
      return type_name;
    }
  }

  // The introspection_c lookup above fails (and sets an rmw error) for C++
  // typesupport services; clear it so a recovered lookup leaves no stale error.
  rmw_reset_error();
  const rosidl_service_type_support_t * cpp_ts = get_service_typesupport_handle(
    type_support, rosidl_typesupport_introspection_cpp::typesupport_identifier);
  if (cpp_ts != nullptr) {
    const auto * members =
      static_cast<const rosidl_typesupport_introspection_cpp::ServiceMembers *>(cpp_ts->data);
    if (members != nullptr && members->response_members_ != nullptr) {
      std::string type_name = normalize_rosidl_namespace_colon(
        members->response_members_->message_namespace_);
      type_name += "::dds_::";
      type_name += members->service_name_;
      type_name += "_Response_";
      return type_name;
    }
  }

  return "UnknownServiceResponse";
}

}  // namespace rmw_int2dds_cpp
