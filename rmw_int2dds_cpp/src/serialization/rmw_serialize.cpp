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

#include <limits>

#include "rmw/rmw.h"
#if __has_include("rmw/dynamic_message_type_support.h")
#include "rmw/dynamic_message_type_support.h"
#define RMW_INT2DDS_HAS_DYNAMIC_MESSAGE_TYPE_SUPPORT 1
#endif
#include "rmw/error_handling.h"

#include "rosidl_runtime_c/message_type_support_struct.h"
#include "rosidl_typesupport_introspection_c/field_types.h"
#include "rosidl_typesupport_introspection_c/identifier.h"
#include "rosidl_typesupport_introspection_c/message_introspection.h"
#include "rosidl_typesupport_introspection_cpp/field_types.hpp"
#include "rosidl_typesupport_introspection_cpp/identifier.hpp"
#include "rosidl_typesupport_introspection_cpp/message_introspection.hpp"
#include "rosidl_typesupport_cpp/identifier.hpp"
#include "rosidl_typesupport_cpp/message_type_support_dispatch.hpp"

#include "rmw_int2dds_cpp/cdr_serializer.hpp"
#include "rmw_int2dds_cpp/identifier.hpp"

namespace
{

const rosidl_message_type_support_t *
get_introspection_type_support(const rosidl_message_type_support_t * type_support)
{
  if (type_support == nullptr) {
    return nullptr;
  }

  if (type_support->typesupport_identifier == rosidl_typesupport_introspection_c__identifier ||
    type_support->typesupport_identifier ==
    rosidl_typesupport_introspection_cpp::typesupport_identifier)
  {
    return type_support;
  }

  if (type_support->typesupport_identifier == rosidl_typesupport_cpp::typesupport_identifier) {
    const rosidl_message_type_support_t * introspection_type_support =
      rosidl_typesupport_cpp::get_message_typesupport_handle_function(
      type_support, rosidl_typesupport_introspection_cpp::typesupport_identifier);
    if (introspection_type_support != nullptr) {
      return introspection_type_support;
    }
  }

  const rosidl_message_type_support_t * introspection_type_support =
    get_message_typesupport_handle(type_support, rosidl_typesupport_introspection_c__identifier);
  if (introspection_type_support != nullptr) {
    return introspection_type_support;
  }

  return get_message_typesupport_handle(
    type_support, rosidl_typesupport_introspection_cpp::typesupport_identifier);
}

size_t
align_size(size_t current_size, size_t alignment)
{
  constexpr size_t cdr_header_size = 4;
  const size_t payload_size =
    current_size >= cdr_header_size ? current_size - cdr_header_size : current_size;
  const size_t padding = (alignment - (payload_size % alignment)) % alignment;
  return current_size + padding;
}

bool
add_bounded_size(size_t & total_size, size_t addition)
{
  if (addition > std::numeric_limits<size_t>::max() - total_size) {
    return false;
  }
  total_size += addition;
  return true;
}

bool
add_aligned_bounded_size(size_t & total_size, size_t alignment, size_t addition)
{
  total_size = align_size(total_size, alignment);
  return add_bounded_size(total_size, addition);
}

bool
compute_member_size_c(
  const rosidl_typesupport_introspection_c__MessageMember * member,
  size_t & total_size,
  bool & supports_bounded_size);

bool
compute_message_size_c(
  const rosidl_typesupport_introspection_c__MessageMembers * members,
  size_t & total_size,
  bool & supports_bounded_size)
{
  if (members == nullptr) {
    return false;
  }

  for (uint32_t i = 0; i < members->member_count_; ++i) {
    if (!compute_member_size_c(&members->members_[i], total_size, supports_bounded_size)) {
      return false;
    }
  }

  return true;
}

bool
compute_member_size_cpp(
  const rosidl_typesupport_introspection_cpp::MessageMember * member,
  size_t & total_size,
  bool & supports_bounded_size);

bool
compute_message_size_cpp(
  const rosidl_typesupport_introspection_cpp::MessageMembers * members,
  size_t & total_size,
  bool & supports_bounded_size)
{
  if (members == nullptr) {
    return false;
  }

  for (uint32_t i = 0; i < members->member_count_; ++i) {
    if (!compute_member_size_cpp(&members->members_[i], total_size, supports_bounded_size)) {
      return false;
    }
  }

  return true;
}

template<typename MemberT, typename RecurseFn>
bool
compute_collection_size(
  const MemberT * member,
  size_t & total_size,
  bool & supports_bounded_size,
  RecurseFn recurse_fn)
{
  size_t element_count = member->array_size_;
  const bool is_sequence = member->is_upper_bound_ || element_count == 0;

  if (is_sequence) {
    if (element_count == 0) {
      supports_bounded_size = false;
      return true;
    }
    if (!add_aligned_bounded_size(total_size, 4, sizeof(uint32_t))) {
      return false;
    }
  }

  for (size_t i = 0; i < element_count; ++i) {
    switch (member->type_id_) {
      case rosidl_typesupport_introspection_c__ROS_TYPE_BOOL:
      case rosidl_typesupport_introspection_c__ROS_TYPE_BYTE:
      case rosidl_typesupport_introspection_c__ROS_TYPE_CHAR:
      case rosidl_typesupport_introspection_c__ROS_TYPE_INT8:
      case rosidl_typesupport_introspection_c__ROS_TYPE_UINT8:
        if (!add_bounded_size(total_size, 1)) {
          return false;
        }
        break;
      case rosidl_typesupport_introspection_c__ROS_TYPE_INT16:
      case rosidl_typesupport_introspection_c__ROS_TYPE_UINT16:
        if (!add_aligned_bounded_size(total_size, 2, 2)) {
          return false;
        }
        break;
      case rosidl_typesupport_introspection_c__ROS_TYPE_INT32:
      case rosidl_typesupport_introspection_c__ROS_TYPE_UINT32:
      case rosidl_typesupport_introspection_c__ROS_TYPE_FLOAT:
        if (!add_aligned_bounded_size(total_size, 4, 4)) {
          return false;
        }
        break;
      case rosidl_typesupport_introspection_c__ROS_TYPE_INT64:
      case rosidl_typesupport_introspection_c__ROS_TYPE_UINT64:
      case rosidl_typesupport_introspection_c__ROS_TYPE_DOUBLE:
        if (!add_aligned_bounded_size(total_size, 8, 8)) {
          return false;
        }
        break;
      case rosidl_typesupport_introspection_c__ROS_TYPE_STRING:
        if (member->string_upper_bound_ == 0) {
          supports_bounded_size = false;
          return true;
        }
        if (!add_aligned_bounded_size(
            total_size, 4,
            sizeof(uint32_t) + member->string_upper_bound_ + 1))
        {
          return false;
        }
        break;
      case rosidl_typesupport_introspection_c__ROS_TYPE_WSTRING:
        if (member->string_upper_bound_ == 0) {
          supports_bounded_size = false;
          return true;
        }
        if (!add_aligned_bounded_size(
            total_size, 4,
            sizeof(uint32_t) + (member->string_upper_bound_ * sizeof(uint32_t))))
        {
          return false;
        }
        break;
      case rosidl_typesupport_introspection_c__ROS_TYPE_MESSAGE:
        if (member->members_ == nullptr) {
          return false;
        }
        if (!recurse_fn(member->members_, total_size, supports_bounded_size)) {
          return false;
        }
        break;
      default:
        supports_bounded_size = false;
        return true;
    }
  }

  return true;
}

bool
compute_member_size_c(
  const rosidl_typesupport_introspection_c__MessageMember * member,
  size_t & total_size,
  bool & supports_bounded_size)
{
  if (member->is_array_) {
    return compute_collection_size(
      member, total_size, supports_bounded_size,
      [](const rosidl_message_type_support_t * nested_type_support, size_t & nested_total,
      bool & nested_supported) {
        if (nested_type_support == nullptr) {
          return false;
        }
        const auto * nested_members = static_cast<
          const rosidl_typesupport_introspection_c__MessageMembers *>(nested_type_support->data);
        return compute_message_size_c(nested_members, nested_total, nested_supported);
      });
  }

  switch (member->type_id_) {
    case rosidl_typesupport_introspection_c__ROS_TYPE_BOOL:
    case rosidl_typesupport_introspection_c__ROS_TYPE_BYTE:
    case rosidl_typesupport_introspection_c__ROS_TYPE_CHAR:
    case rosidl_typesupport_introspection_c__ROS_TYPE_INT8:
    case rosidl_typesupport_introspection_c__ROS_TYPE_UINT8:
      return add_bounded_size(total_size, 1);
    case rosidl_typesupport_introspection_c__ROS_TYPE_INT16:
    case rosidl_typesupport_introspection_c__ROS_TYPE_UINT16:
      return add_aligned_bounded_size(total_size, 2, 2);
    case rosidl_typesupport_introspection_c__ROS_TYPE_INT32:
    case rosidl_typesupport_introspection_c__ROS_TYPE_UINT32:
    case rosidl_typesupport_introspection_c__ROS_TYPE_FLOAT:
      return add_aligned_bounded_size(total_size, 4, 4);
    case rosidl_typesupport_introspection_c__ROS_TYPE_INT64:
    case rosidl_typesupport_introspection_c__ROS_TYPE_UINT64:
    case rosidl_typesupport_introspection_c__ROS_TYPE_DOUBLE:
      return add_aligned_bounded_size(total_size, 8, 8);
    case rosidl_typesupport_introspection_c__ROS_TYPE_STRING:
      if (member->string_upper_bound_ == 0) {
        supports_bounded_size = false;
        return true;
      }
      return add_aligned_bounded_size(
        total_size, 4, sizeof(uint32_t) + member->string_upper_bound_ + 1);
    case rosidl_typesupport_introspection_c__ROS_TYPE_WSTRING:
      if (member->string_upper_bound_ == 0) {
        supports_bounded_size = false;
        return true;
      }
      return add_aligned_bounded_size(
        total_size, 4,
        sizeof(uint32_t) + (member->string_upper_bound_ * sizeof(uint32_t)));
    case rosidl_typesupport_introspection_c__ROS_TYPE_MESSAGE:
      if (member->members_ == nullptr) {
        return false;
      }
      return compute_message_size_c(
        static_cast<const rosidl_typesupport_introspection_c__MessageMembers *>(member->members_->
        data),
        total_size, supports_bounded_size);
    default:
      supports_bounded_size = false;
      return true;
  }
}

bool
compute_member_size_cpp(
  const rosidl_typesupport_introspection_cpp::MessageMember * member,
  size_t & total_size,
  bool & supports_bounded_size)
{
  if (member->is_array_) {
    return compute_collection_size(
      member, total_size, supports_bounded_size,
      [](const rosidl_message_type_support_t * nested_type_support, size_t & nested_total,
      bool & nested_supported) {
        if (nested_type_support == nullptr) {
          return false;
        }
        const auto * nested_members = static_cast<
          const rosidl_typesupport_introspection_cpp::MessageMembers *>(nested_type_support->data);
        return compute_message_size_cpp(nested_members, nested_total, nested_supported);
      });
  }

  switch (member->type_id_) {
    case rosidl_typesupport_introspection_cpp::ROS_TYPE_BOOL:
    case rosidl_typesupport_introspection_cpp::ROS_TYPE_BYTE:
    case rosidl_typesupport_introspection_cpp::ROS_TYPE_CHAR:
    case rosidl_typesupport_introspection_cpp::ROS_TYPE_INT8:
    case rosidl_typesupport_introspection_cpp::ROS_TYPE_UINT8:
      return add_bounded_size(total_size, 1);
    case rosidl_typesupport_introspection_cpp::ROS_TYPE_INT16:
    case rosidl_typesupport_introspection_cpp::ROS_TYPE_UINT16:
      return add_aligned_bounded_size(total_size, 2, 2);
    case rosidl_typesupport_introspection_cpp::ROS_TYPE_INT32:
    case rosidl_typesupport_introspection_cpp::ROS_TYPE_UINT32:
    case rosidl_typesupport_introspection_cpp::ROS_TYPE_FLOAT:
      return add_aligned_bounded_size(total_size, 4, 4);
    case rosidl_typesupport_introspection_cpp::ROS_TYPE_INT64:
    case rosidl_typesupport_introspection_cpp::ROS_TYPE_UINT64:
    case rosidl_typesupport_introspection_cpp::ROS_TYPE_DOUBLE:
      return add_aligned_bounded_size(total_size, 8, 8);
    case rosidl_typesupport_introspection_cpp::ROS_TYPE_STRING:
      if (member->string_upper_bound_ == 0) {
        supports_bounded_size = false;
        return true;
      }
      return add_aligned_bounded_size(
        total_size, 4, sizeof(uint32_t) + member->string_upper_bound_ + 1);
    case rosidl_typesupport_introspection_cpp::ROS_TYPE_WSTRING:
      if (member->string_upper_bound_ == 0) {
        supports_bounded_size = false;
        return true;
      }
      return add_aligned_bounded_size(
        total_size, 4,
        sizeof(uint32_t) + (member->string_upper_bound_ * sizeof(uint32_t)));
    case rosidl_typesupport_introspection_cpp::ROS_TYPE_MESSAGE:
      if (member->members_ == nullptr) {
        return false;
      }
      return compute_message_size_cpp(
        static_cast<const rosidl_typesupport_introspection_cpp::MessageMembers *>(member->members_->
        data),
        total_size, supports_bounded_size);
    default:
      supports_bounded_size = false;
      return true;
  }
}

bool
serialize_message_with_type_support(
  const void * ros_message,
  const rosidl_message_type_support_t * type_support,
  rmw_int2dds_cpp::CdrSerializer & serializer)
{
  const rosidl_message_type_support_t * introspection_type_support =
    get_introspection_type_support(type_support);
  if (introspection_type_support == nullptr) {
    return false;
  }

  if (introspection_type_support->typesupport_identifier ==
    rosidl_typesupport_introspection_c__identifier)
  {
    const auto * members = static_cast<
      const rosidl_typesupport_introspection_c__MessageMembers *>(introspection_type_support->data);
    return serializer.serialize_message_c(ros_message, members);
  }

  if (
    introspection_type_support->typesupport_identifier ==
    rosidl_typesupport_introspection_cpp::typesupport_identifier)
  {
    const auto * members = static_cast<
      const rosidl_typesupport_introspection_cpp::MessageMembers *>(introspection_type_support->data);
    return serializer.serialize_message_cpp(ros_message, members);
  }

  return false;
}

bool
deserialize_message_with_type_support(
  const uint8_t * data,
  size_t data_size,
  const rosidl_message_type_support_t * type_support,
  void * ros_message)
{
  if (data_size < 4) {
    return false;
  }

  rmw_int2dds_cpp::CdrDeserializer deserializer(data + 4, data_size - 4);

  const rosidl_message_type_support_t * introspection_type_support =
    get_introspection_type_support(type_support);
  if (introspection_type_support == nullptr) {
    return false;
  }

  if (introspection_type_support->typesupport_identifier ==
    rosidl_typesupport_introspection_c__identifier)
  {
    const auto * members = static_cast<
      const rosidl_typesupport_introspection_c__MessageMembers *>(introspection_type_support->data);
    return deserializer.deserialize_message_c(ros_message, members);
  }

  if (
    introspection_type_support->typesupport_identifier ==
    rosidl_typesupport_introspection_cpp::typesupport_identifier)
  {
    const auto * members = static_cast<
      const rosidl_typesupport_introspection_cpp::MessageMembers *>(introspection_type_support->data);
    return deserializer.deserialize_message_cpp(ros_message, members);
  }

  return false;
}

}  // namespace

extern "C"
{

rmw_ret_t
rmw_serialize(
  const void * ros_message,
  const rosidl_message_type_support_t * type_support,
  rmw_serialized_message_t * serialized_message)
{
  RMW_CHECK_ARGUMENT_FOR_NULL(ros_message, RMW_RET_INVALID_ARGUMENT);
  RMW_CHECK_ARGUMENT_FOR_NULL(type_support, RMW_RET_INVALID_ARGUMENT);
  RMW_CHECK_ARGUMENT_FOR_NULL(serialized_message, RMW_RET_INVALID_ARGUMENT);

  rmw_int2dds_cpp::CdrSerializer serializer;
  if (!serialize_message_with_type_support(ros_message, type_support, serializer)) {
    RMW_SET_ERROR_MSG("failed to serialize ROS message");
    return RMW_RET_ERROR;
  }

  const size_t serialized_size = serializer.get_size();
  if (serialized_message->buffer_capacity < serialized_size) {
    const rmw_ret_t resize_ret = rmw_serialized_message_resize(serialized_message, serialized_size);
    if (resize_ret != RMW_RET_OK) {
      RMW_SET_ERROR_MSG("failed to resize serialized message buffer");
      return resize_ret;
    }
  }

  std::memcpy(serialized_message->buffer, serializer.get_data(), serialized_size);
  serialized_message->buffer_length = serialized_size;
  return RMW_RET_OK;
}

rmw_ret_t
rmw_deserialize(
  const rmw_serialized_message_t * serialized_message,
  const rosidl_message_type_support_t * type_support,
  void * ros_message)
{
  RMW_CHECK_ARGUMENT_FOR_NULL(serialized_message, RMW_RET_INVALID_ARGUMENT);
  RMW_CHECK_ARGUMENT_FOR_NULL(type_support, RMW_RET_INVALID_ARGUMENT);
  RMW_CHECK_ARGUMENT_FOR_NULL(ros_message, RMW_RET_INVALID_ARGUMENT);

  if (serialized_message->buffer == nullptr || serialized_message->buffer_length < 4) {
    RMW_SET_ERROR_MSG("serialized message is invalid or missing the CDR encapsulation header");
    return RMW_RET_ERROR;
  }

  if (!deserialize_message_with_type_support(
      serialized_message->buffer, serialized_message->buffer_length, type_support, ros_message))
  {
    RMW_SET_ERROR_MSG("failed to deserialize ROS message");
    return RMW_RET_ERROR;
  }

  return RMW_RET_OK;
}

rmw_ret_t
rmw_get_serialized_message_size(
  const rosidl_message_type_support_t * type_support,
  const rosidl_runtime_c__Sequence__bound * message_bounds,
  size_t * size)
{
  RMW_CHECK_ARGUMENT_FOR_NULL(type_support, RMW_RET_INVALID_ARGUMENT);
  RMW_CHECK_ARGUMENT_FOR_NULL(size, RMW_RET_INVALID_ARGUMENT);
  (void)message_bounds;

  const rosidl_message_type_support_t * introspection_type_support =
    get_introspection_type_support(type_support);
  if (introspection_type_support == nullptr) {
    RMW_SET_ERROR_MSG("failed to get introspection type support for serialized message size");
    return RMW_RET_ERROR;
  }

  size_t total_size = 4;  // CDR encapsulation header
  bool supports_bounded_size = true;
  bool size_ok = false;

  if (introspection_type_support->typesupport_identifier ==
    rosidl_typesupport_introspection_c__identifier)
  {
    const auto * members = static_cast<
      const rosidl_typesupport_introspection_c__MessageMembers *>(introspection_type_support->data);
    size_ok = compute_message_size_c(members, total_size, supports_bounded_size);
  } else if (
    introspection_type_support->typesupport_identifier ==
    rosidl_typesupport_introspection_cpp::typesupport_identifier)
  {
    const auto * members = static_cast<
      const rosidl_typesupport_introspection_cpp::MessageMembers *>(introspection_type_support->data);
    size_ok = compute_message_size_cpp(members, total_size, supports_bounded_size);
  } else {
    RMW_SET_ERROR_MSG("unsupported type support for serialized message size calculation");
    return RMW_RET_ERROR;
  }

  if (!size_ok) {
    RMW_SET_ERROR_MSG("failed to compute serialized message size");
    return RMW_RET_ERROR;
  }

  if (!supports_bounded_size) {
    RMW_SET_ERROR_MSG(
      "serialized message size is only available for bounded or fixed-size message types");
    return RMW_RET_UNSUPPORTED;
  }

  *size = total_size;
  return RMW_RET_OK;
}

#ifdef RMW_INT2DDS_HAS_DYNAMIC_MESSAGE_TYPE_SUPPORT
rmw_ret_t
rmw_serialization_support_init(
  const char * serialization_lib_name,
  rcutils_allocator_t * allocator,
  rosidl_dynamic_typesupport_serialization_support_t * serialization_support)
{
  (void)serialization_lib_name;
  (void)allocator;
  (void)serialization_support;
  // Dynamic message type support is not provided by int2dds
  RMW_SET_ERROR_MSG("rmw_serialization_support_init is not supported by rmw_int2dds_cpp");
  return RMW_RET_UNSUPPORTED;
}
#endif

}  // extern "C"
