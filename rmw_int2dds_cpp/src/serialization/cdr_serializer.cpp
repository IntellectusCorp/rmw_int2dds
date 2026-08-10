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

#include "rmw_int2dds_cpp/cdr_serializer.hpp"

#include <algorithm>
#include <cstdlib>
#include <type_traits>

namespace rmw_int2dds_cpp
{

namespace
{

// Byte width of a fixed-size primitive whose array body can be bulk-copied, or 0 for
// types needing per-element handling. The C and C++ introspection ids share values.
size_t fixed_primitive_size(uint8_t type_id)
{
  switch (type_id) {
    case rosidl_typesupport_introspection_c__ROS_TYPE_BYTE:
    case rosidl_typesupport_introspection_c__ROS_TYPE_UINT8:
    case rosidl_typesupport_introspection_c__ROS_TYPE_CHAR:
    case rosidl_typesupport_introspection_c__ROS_TYPE_INT8:
      return 1;
    case rosidl_typesupport_introspection_c__ROS_TYPE_INT16:
    case rosidl_typesupport_introspection_c__ROS_TYPE_UINT16:
      return 2;
    case rosidl_typesupport_introspection_c__ROS_TYPE_INT32:
    case rosidl_typesupport_introspection_c__ROS_TYPE_UINT32:
    case rosidl_typesupport_introspection_c__ROS_TYPE_FLOAT:
      return 4;
    case rosidl_typesupport_introspection_c__ROS_TYPE_INT64:
    case rosidl_typesupport_introspection_c__ROS_TYPE_UINT64:
    case rosidl_typesupport_introspection_c__ROS_TYPE_DOUBLE:
      return 8;
    default:
      return 0;
  }
}

}  // namespace

// ============================================================================
// CdrSerializer Implementation
// ============================================================================

CdrSerializer::CdrSerializer()
: buffer_(), external_buffer_(nullptr), external_capacity_(0), pos_(0), overflow_(false)
{
  buffer_.reserve(256);
  // Write CDR encapsulation header (Little Endian)
  append_bytes(CDR_ENCAPSULATION_LE, 4);
}

CdrSerializer::CdrSerializer(uint8_t * external_buffer, size_t external_capacity)
: buffer_(),
  external_buffer_(external_buffer),
  external_capacity_(external_capacity),
  pos_(0),
  overflow_(false)
{
  append_bytes(CDR_ENCAPSULATION_LE, 4);
}

void CdrSerializer::reset()
{
  overflow_ = false;
  buffer_.clear();
  pos_ = 0;
  append_bytes(CDR_ENCAPSULATION_LE, 4);
}

void CdrSerializer::align(size_t alignment)
{
  constexpr size_t cdr_header_size = 4;
  const size_t payload_pos = pos_ >= cdr_header_size ? pos_ - cdr_header_size : pos_;
  size_t padding = (alignment - (payload_pos % alignment)) % alignment;
  append_zeros(padding);
}

void CdrSerializer::ensure_capacity(size_t additional)
{
  if (external_buffer_ != nullptr) {
    if (pos_ + additional > external_capacity_) {
      overflow_ = true;
    }
    return;
  }
  if (buffer_.capacity() < buffer_.size() + additional) {
    buffer_.reserve(buffer_.size() + additional + 256);
  }
}

void CdrSerializer::append_byte(uint8_t value)
{
  uint8_t * dst = append_uninitialized(1);
  if (dst != nullptr) {
    *dst = value;
  }
}

void CdrSerializer::append_bytes(const void * data, size_t size)
{
  if (size == 0) {
    return;
  }
  uint8_t * dst = append_uninitialized(size);
  if (dst != nullptr) {
    std::memcpy(dst, data, size);
  }
}

void CdrSerializer::append_zeros(size_t count)
{
  if (count == 0) {
    return;
  }
  uint8_t * dst = append_uninitialized(count);
  if (dst != nullptr) {
    std::memset(dst, 0, count);
  }
}

uint8_t * CdrSerializer::append_uninitialized(size_t size)
{
  ensure_capacity(size);
  if (overflow_) {
    return nullptr;
  }

  if (external_buffer_ != nullptr) {
    uint8_t * dst = external_buffer_ + pos_;
    pos_ += size;
    return dst;
  }

  const size_t old_size = buffer_.size();
  buffer_.resize(old_size + size);
  pos_ += size;
  return buffer_.data() + old_size;
}

void CdrSerializer::serialize_bool(bool value)
{
  append_byte(value ? 1 : 0);
}

void CdrSerializer::serialize_byte(uint8_t value)
{
  append_byte(value);
}

void CdrSerializer::serialize_char(char value)
{
  append_byte(static_cast<uint8_t>(value));
}

void CdrSerializer::serialize_int8(int8_t value)
{
  append_byte(static_cast<uint8_t>(value));
}

void CdrSerializer::serialize_uint8(uint8_t value)
{
  append_byte(value);
}

void CdrSerializer::serialize_int16(int16_t value)
{
  align(2);
  append_bytes(&value, 2);
}

void CdrSerializer::serialize_uint16(uint16_t value)
{
  align(2);
  append_bytes(&value, 2);
}

void CdrSerializer::serialize_int32(int32_t value)
{
  align(4);
  append_bytes(&value, 4);
}

void CdrSerializer::serialize_uint32(uint32_t value)
{
  align(4);
  append_bytes(&value, 4);
}

void CdrSerializer::serialize_int64(int64_t value)
{
  align(8);
  append_bytes(&value, 8);
}

void CdrSerializer::serialize_uint64(uint64_t value)
{
  align(8);
  append_bytes(&value, 8);
}

void CdrSerializer::serialize_float32(float value)
{
  align(4);
  append_bytes(&value, 4);
}

void CdrSerializer::serialize_float64(double value)
{
  align(8);
  append_bytes(&value, 8);
}

void CdrSerializer::serialize_string(const std::string & value)
{
  uint32_t length = static_cast<uint32_t>(value.size() + 1);  // +1 for null terminator
  serialize_uint32(length);
  append_bytes(value.data(), value.size());
  append_byte(0);
}

void CdrSerializer::serialize_string(const rosidl_runtime_c__String & value)
{
  uint32_t length = static_cast<uint32_t>(value.size + 1);
  serialize_uint32(length);
  append_bytes(value.data, value.size);
  append_byte(0);
}

// Wire format note: unlike narrow strings, ROS 2's reference RMW (rmw_fastrtps,
// fastcdr XCDR1) serializes wstring as: uint32 length = number of characters
// (NO null terminator counted), then each character as a 4-byte unsigned value,
// with no trailing null. Verified against an rmw_fastrtps byte dump; the
// previous null-terminated 2-byte-per-char encoding broke cross-vendor
// (int2dds<->fastrtps) WStrings pub/sub while remaining self-consistent.
void CdrSerializer::serialize_wstring(const std::u16string & value)
{
  serialize_uint32(static_cast<uint32_t>(value.size()));
  for (char16_t ch : value) {
    serialize_uint32(static_cast<uint32_t>(static_cast<uint16_t>(ch)));
  }
}

void CdrSerializer::serialize_wstring(const rosidl_runtime_c__U16String & value)
{
  serialize_uint32(static_cast<uint32_t>(value.size));
  for (size_t i = 0; i < value.size; ++i) {
    serialize_uint32(static_cast<uint32_t>(static_cast<uint16_t>(value.data[i])));
  }
}

void CdrSerializer::serialize_raw(const void * data, size_t size)
{
  append_bytes(data, size);
}

bool CdrSerializer::serialize_message_c(
  const void * ros_message,
  const rosidl_typesupport_introspection_c__MessageMembers * members)
{
  if (ros_message == nullptr || members == nullptr) {
    return false;
  }

  for (uint32_t i = 0; i < members->member_count_; ++i) {
    const auto & member = members->members_[i];
    const void * member_data =
      static_cast<const uint8_t *>(ros_message) + member.offset_;

    if (!serialize_member_c(member_data, &member)) {
      return false;
    }
  }
  return true;
}

bool CdrSerializer::serialize_member_c(
  const void * member_data,
  const rosidl_typesupport_introspection_c__MessageMember * member)
{
  if (member->is_array_) {
    // Handle arrays and sequences
    size_t array_size = member->array_size_;
    const void * collection_data = member_data;

    if (member->is_upper_bound_ || array_size == 0) {
      // Dynamic sequence - get size from sequence structure
      const auto * seq = static_cast<const rosidl_runtime_c__char__Sequence *>(member_data);
      array_size = seq->size;

      // Serialize sequence length
      serialize_uint32(static_cast<uint32_t>(array_size));
    }

    const size_t element_size = fixed_primitive_size(member->type_id_);
    if (element_size != 0) {
      if (array_size == 0) {
        return true;
      }
      const void * first_element = member->get_const_function != nullptr ?
        member->get_const_function(collection_data, 0) : nullptr;
      if (first_element != nullptr) {
        align(element_size);
        serialize_raw(first_element, array_size * element_size);
        return true;
      }
      // For a fixed primitive the per-element loop below dereferences the accessor
      // result directly (no fetch fallback here); a null first element cannot be
      // bulk-copied and would be dereferenced as null, so fail cleanly instead.
      return false;
    }

    // Serialize each element
    for (size_t i = 0; i < array_size; ++i) {
      const void * element_data = nullptr;
      if (member->get_const_function != nullptr) {
        element_data = member->get_const_function(collection_data, i);
      } else {
        // Fallback: this shouldn't happen in ROS2 Humble
        return false;
      }

      switch (member->type_id_) {
        case rosidl_typesupport_introspection_c__ROS_TYPE_BOOL:
          serialize_bool(*static_cast<const bool *>(element_data));
          break;
        case rosidl_typesupport_introspection_c__ROS_TYPE_BYTE:
        case rosidl_typesupport_introspection_c__ROS_TYPE_UINT8:
          serialize_uint8(*static_cast<const uint8_t *>(element_data));
          break;
        case rosidl_typesupport_introspection_c__ROS_TYPE_CHAR:
        case rosidl_typesupport_introspection_c__ROS_TYPE_INT8:
          serialize_int8(*static_cast<const int8_t *>(element_data));
          break;
        case rosidl_typesupport_introspection_c__ROS_TYPE_INT16:
          serialize_int16(*static_cast<const int16_t *>(element_data));
          break;
        case rosidl_typesupport_introspection_c__ROS_TYPE_UINT16:
          serialize_uint16(*static_cast<const uint16_t *>(element_data));
          break;
        case rosidl_typesupport_introspection_c__ROS_TYPE_INT32:
          serialize_int32(*static_cast<const int32_t *>(element_data));
          break;
        case rosidl_typesupport_introspection_c__ROS_TYPE_UINT32:
          serialize_uint32(*static_cast<const uint32_t *>(element_data));
          break;
        case rosidl_typesupport_introspection_c__ROS_TYPE_INT64:
          serialize_int64(*static_cast<const int64_t *>(element_data));
          break;
        case rosidl_typesupport_introspection_c__ROS_TYPE_UINT64:
          serialize_uint64(*static_cast<const uint64_t *>(element_data));
          break;
        case rosidl_typesupport_introspection_c__ROS_TYPE_FLOAT:
          serialize_float32(*static_cast<const float *>(element_data));
          break;
        case rosidl_typesupport_introspection_c__ROS_TYPE_DOUBLE:
          serialize_float64(*static_cast<const double *>(element_data));
          break;
        case rosidl_typesupport_introspection_c__ROS_TYPE_STRING:
          serialize_string(*static_cast<const rosidl_runtime_c__String *>(element_data));
          break;
        case rosidl_typesupport_introspection_c__ROS_TYPE_WSTRING:
          serialize_wstring(*static_cast<const rosidl_runtime_c__U16String *>(element_data));
          break;
        case rosidl_typesupport_introspection_c__ROS_TYPE_MESSAGE:
          if (member->members_ != nullptr) {
            if (!serialize_message_c(
                element_data,
                static_cast<const rosidl_typesupport_introspection_c__MessageMembers *>(
                  member->members_->data)))
            {
              return false;
            }
          }
          break;
        default:
          return false;
      }
    }
  } else {
    // Single element
    switch (member->type_id_) {
      case rosidl_typesupport_introspection_c__ROS_TYPE_BOOL:
        serialize_bool(*static_cast<const bool *>(member_data));
        break;
      case rosidl_typesupport_introspection_c__ROS_TYPE_BYTE:
      case rosidl_typesupport_introspection_c__ROS_TYPE_UINT8:
        serialize_uint8(*static_cast<const uint8_t *>(member_data));
        break;
      case rosidl_typesupport_introspection_c__ROS_TYPE_CHAR:
      case rosidl_typesupport_introspection_c__ROS_TYPE_INT8:
        serialize_int8(*static_cast<const int8_t *>(member_data));
        break;
      case rosidl_typesupport_introspection_c__ROS_TYPE_INT16:
        serialize_int16(*static_cast<const int16_t *>(member_data));
        break;
      case rosidl_typesupport_introspection_c__ROS_TYPE_UINT16:
        serialize_uint16(*static_cast<const uint16_t *>(member_data));
        break;
      case rosidl_typesupport_introspection_c__ROS_TYPE_INT32:
        serialize_int32(*static_cast<const int32_t *>(member_data));
        break;
      case rosidl_typesupport_introspection_c__ROS_TYPE_UINT32:
        serialize_uint32(*static_cast<const uint32_t *>(member_data));
        break;
      case rosidl_typesupport_introspection_c__ROS_TYPE_INT64:
        serialize_int64(*static_cast<const int64_t *>(member_data));
        break;
      case rosidl_typesupport_introspection_c__ROS_TYPE_UINT64:
        serialize_uint64(*static_cast<const uint64_t *>(member_data));
        break;
      case rosidl_typesupport_introspection_c__ROS_TYPE_FLOAT:
        serialize_float32(*static_cast<const float *>(member_data));
        break;
      case rosidl_typesupport_introspection_c__ROS_TYPE_DOUBLE:
        serialize_float64(*static_cast<const double *>(member_data));
        break;
      case rosidl_typesupport_introspection_c__ROS_TYPE_STRING:
        serialize_string(*static_cast<const rosidl_runtime_c__String *>(member_data));
        break;
      case rosidl_typesupport_introspection_c__ROS_TYPE_WSTRING:
        serialize_wstring(*static_cast<const rosidl_runtime_c__U16String *>(member_data));
        break;
      case rosidl_typesupport_introspection_c__ROS_TYPE_MESSAGE:
        if (member->members_ != nullptr) {
          if (!serialize_message_c(
              member_data,
              static_cast<const rosidl_typesupport_introspection_c__MessageMembers *>(
                member->members_->data)))
          {
            return false;
          }
        }
        break;
      default:
        return false;
    }
  }
  return true;
}

bool CdrSerializer::serialize_message_cpp(
  const void * ros_message,
  const rosidl_typesupport_introspection_cpp::MessageMembers * members)
{
  if (ros_message == nullptr || members == nullptr) {
    return false;
  }

  for (uint32_t i = 0; i < members->member_count_; ++i) {
    const auto & member = members->members_[i];
    const void * member_data =
      static_cast<const uint8_t *>(ros_message) + member.offset_;

    if (!serialize_member_cpp(member_data, &member)) {
      return false;
    }
  }
  return true;
}

bool CdrSerializer::serialize_member_cpp(
  const void * member_data,
  const rosidl_typesupport_introspection_cpp::MessageMember * member)
{
  if (member->is_array_) {
    size_t array_size = member->array_size_;

    if (member->is_upper_bound_ || array_size == 0) {
      // Dynamic sequence - use size_function
      if (member->size_function != nullptr) {
        array_size = member->size_function(member_data);
      }
      serialize_uint32(static_cast<uint32_t>(array_size));
    }

    const size_t element_size = fixed_primitive_size(member->type_id_);
    if (element_size != 0) {
      if (array_size == 0) {
        return true;
      }
      const void * first_element = member->get_const_function != nullptr ?
        member->get_const_function(member_data, 0) : nullptr;
      if (first_element != nullptr) {
        align(element_size);
        serialize_raw(first_element, array_size * element_size);
        return true;
      }
    }

    for (size_t i = 0; i < array_size; ++i) {
      const void * element_data = nullptr;
      if (member->get_const_function != nullptr) {
        element_data = member->get_const_function(member_data, i);
      }

      auto fetch_value = [&](auto & value) -> bool {
          using ValueT = typename std::decay<decltype(value)>::type;
          if (element_data != nullptr) {
            value = *static_cast<const ValueT *>(element_data);
            return true;
          }
          if (member->fetch_function != nullptr) {
            member->fetch_function(member_data, i, &value);
            return true;
          }
          return false;
        };

      switch (member->type_id_) {
        case rosidl_typesupport_introspection_cpp::ROS_TYPE_BOOL: {
            bool value{};
            if (!fetch_value(value)) {
              return false;
            }
            serialize_bool(value);
            break;
          }
        case rosidl_typesupport_introspection_cpp::ROS_TYPE_BYTE:
        case rosidl_typesupport_introspection_cpp::ROS_TYPE_UINT8: {
            uint8_t value{};
            if (!fetch_value(value)) {
              return false;
            }
            serialize_uint8(value);
            break;
          }
        case rosidl_typesupport_introspection_cpp::ROS_TYPE_CHAR:
        case rosidl_typesupport_introspection_cpp::ROS_TYPE_INT8: {
            int8_t value{};
            if (!fetch_value(value)) {
              return false;
            }
            serialize_int8(value);
            break;
          }
        case rosidl_typesupport_introspection_cpp::ROS_TYPE_INT16: {
            int16_t value{};
            if (!fetch_value(value)) {
              return false;
            }
            serialize_int16(value);
            break;
          }
        case rosidl_typesupport_introspection_cpp::ROS_TYPE_UINT16: {
            uint16_t value{};
            if (!fetch_value(value)) {
              return false;
            }
            serialize_uint16(value);
            break;
          }
        case rosidl_typesupport_introspection_cpp::ROS_TYPE_INT32: {
            int32_t value{};
            if (!fetch_value(value)) {
              return false;
            }
            serialize_int32(value);
            break;
          }
        case rosidl_typesupport_introspection_cpp::ROS_TYPE_UINT32: {
            uint32_t value{};
            if (!fetch_value(value)) {
              return false;
            }
            serialize_uint32(value);
            break;
          }
        case rosidl_typesupport_introspection_cpp::ROS_TYPE_INT64: {
            int64_t value{};
            if (!fetch_value(value)) {
              return false;
            }
            serialize_int64(value);
            break;
          }
        case rosidl_typesupport_introspection_cpp::ROS_TYPE_UINT64: {
            uint64_t value{};
            if (!fetch_value(value)) {
              return false;
            }
            serialize_uint64(value);
            break;
          }
        case rosidl_typesupport_introspection_cpp::ROS_TYPE_FLOAT: {
            float value{};
            if (!fetch_value(value)) {
              return false;
            }
            serialize_float32(value);
            break;
          }
        case rosidl_typesupport_introspection_cpp::ROS_TYPE_DOUBLE: {
            double value{};
            if (!fetch_value(value)) {
              return false;
            }
            serialize_float64(value);
            break;
          }
        case rosidl_typesupport_introspection_cpp::ROS_TYPE_STRING: {
            std::string value;
            if (!fetch_value(value)) {
              return false;
            }
            serialize_string(value);
            break;
          }
        case rosidl_typesupport_introspection_cpp::ROS_TYPE_WSTRING: {
            std::u16string value;
            if (!fetch_value(value)) {
              return false;
            }
            serialize_wstring(value);
            break;
          }
        case rosidl_typesupport_introspection_cpp::ROS_TYPE_MESSAGE:
          if (element_data == nullptr) {
            return false;
          }
          if (member->members_ != nullptr) {
            if (!serialize_message_cpp(
                element_data,
                static_cast<const rosidl_typesupport_introspection_cpp::MessageMembers *>(
                  member->members_->data)))
            {
              return false;
            }
          }
          break;
        default:
          return false;
      }
    }
  } else {
    switch (member->type_id_) {
      case rosidl_typesupport_introspection_cpp::ROS_TYPE_BOOL:
        serialize_bool(*static_cast<const bool *>(member_data));
        break;
      case rosidl_typesupport_introspection_cpp::ROS_TYPE_BYTE:
      case rosidl_typesupport_introspection_cpp::ROS_TYPE_UINT8:
        serialize_uint8(*static_cast<const uint8_t *>(member_data));
        break;
      case rosidl_typesupport_introspection_cpp::ROS_TYPE_CHAR:
      case rosidl_typesupport_introspection_cpp::ROS_TYPE_INT8:
        serialize_int8(*static_cast<const int8_t *>(member_data));
        break;
      case rosidl_typesupport_introspection_cpp::ROS_TYPE_INT16:
        serialize_int16(*static_cast<const int16_t *>(member_data));
        break;
      case rosidl_typesupport_introspection_cpp::ROS_TYPE_UINT16:
        serialize_uint16(*static_cast<const uint16_t *>(member_data));
        break;
      case rosidl_typesupport_introspection_cpp::ROS_TYPE_INT32:
        serialize_int32(*static_cast<const int32_t *>(member_data));
        break;
      case rosidl_typesupport_introspection_cpp::ROS_TYPE_UINT32:
        serialize_uint32(*static_cast<const uint32_t *>(member_data));
        break;
      case rosidl_typesupport_introspection_cpp::ROS_TYPE_INT64:
        serialize_int64(*static_cast<const int64_t *>(member_data));
        break;
      case rosidl_typesupport_introspection_cpp::ROS_TYPE_UINT64:
        serialize_uint64(*static_cast<const uint64_t *>(member_data));
        break;
      case rosidl_typesupport_introspection_cpp::ROS_TYPE_FLOAT:
        serialize_float32(*static_cast<const float *>(member_data));
        break;
      case rosidl_typesupport_introspection_cpp::ROS_TYPE_DOUBLE:
        serialize_float64(*static_cast<const double *>(member_data));
        break;
      case rosidl_typesupport_introspection_cpp::ROS_TYPE_STRING:
        serialize_string(*static_cast<const std::string *>(member_data));
        break;
      case rosidl_typesupport_introspection_cpp::ROS_TYPE_WSTRING:
        serialize_wstring(*static_cast<const std::u16string *>(member_data));
        break;
      case rosidl_typesupport_introspection_cpp::ROS_TYPE_MESSAGE:
        if (member->members_ != nullptr) {
          if (!serialize_message_cpp(
              member_data,
              static_cast<const rosidl_typesupport_introspection_cpp::MessageMembers *>(
                member->members_->data)))
          {
            return false;
          }
        }
        break;
      default:
        return false;
    }
  }
  return true;
}

// ============================================================================
// CdrDeserializer Implementation
// ============================================================================

CdrDeserializer::CdrDeserializer(const uint8_t * data, size_t size)
: data_(data), size_(size), pos_(0)
{
}

void CdrDeserializer::align(size_t alignment)
{
  // The deserializer receives payload bytes after the 4-byte CDR encapsulation header.
  pos_ += (alignment - (pos_ % alignment)) % alignment;
}

bool CdrDeserializer::deserialize_bool()
{
  if (!has_remaining(1)) {
    return false;
  }
  return data_[pos_++] != 0;
}

uint8_t CdrDeserializer::deserialize_byte()
{
  if (!has_remaining(1)) {
    return 0;
  }
  return data_[pos_++];
}

char CdrDeserializer::deserialize_char()
{
  if (!has_remaining(1)) {
    return 0;
  }
  return static_cast<char>(data_[pos_++]);
}

int8_t CdrDeserializer::deserialize_int8()
{
  if (!has_remaining(1)) {
    return 0;
  }
  return static_cast<int8_t>(data_[pos_++]);
}

uint8_t CdrDeserializer::deserialize_uint8()
{
  if (!has_remaining(1)) {
    return 0;
  }
  return data_[pos_++];
}

int16_t CdrDeserializer::deserialize_int16()
{
  align(2);
  if (!has_remaining(2)) {
    return 0;
  }
  int16_t value;
  std::memcpy(&value, &data_[pos_], 2);
  pos_ += 2;
  return value;
}

uint16_t CdrDeserializer::deserialize_uint16()
{
  align(2);
  if (!has_remaining(2)) {
    return 0;
  }
  uint16_t value;
  std::memcpy(&value, &data_[pos_], 2);
  pos_ += 2;
  return value;
}

int32_t CdrDeserializer::deserialize_int32()
{
  align(4);
  if (!has_remaining(4)) {
    return 0;
  }
  int32_t value;
  std::memcpy(&value, &data_[pos_], 4);
  pos_ += 4;
  return value;
}

uint32_t CdrDeserializer::deserialize_uint32()
{
  align(4);
  if (!has_remaining(4)) {
    return 0;
  }
  uint32_t value;
  std::memcpy(&value, &data_[pos_], 4);
  pos_ += 4;
  return value;
}

int64_t CdrDeserializer::deserialize_int64()
{
  align(8);
  if (!has_remaining(8)) {
    return 0;
  }
  int64_t value;
  std::memcpy(&value, &data_[pos_], 8);
  pos_ += 8;
  return value;
}

uint64_t CdrDeserializer::deserialize_uint64()
{
  align(8);
  if (!has_remaining(8)) {
    return 0;
  }
  uint64_t value;
  std::memcpy(&value, &data_[pos_], 8);
  pos_ += 8;
  return value;
}

float CdrDeserializer::deserialize_float32()
{
  align(4);
  if (!has_remaining(4)) {
    return 0.0f;
  }
  float value;
  std::memcpy(&value, &data_[pos_], 4);
  pos_ += 4;
  return value;
}

double CdrDeserializer::deserialize_float64()
{
  align(8);
  if (!has_remaining(8)) {
    return 0.0;
  }
  double value;
  std::memcpy(&value, &data_[pos_], 8);
  pos_ += 8;
  return value;
}

std::string CdrDeserializer::deserialize_string()
{
  uint32_t length = deserialize_uint32();
  if (length == 0 || !has_remaining(length)) {
    return "";
  }
  std::string value(reinterpret_cast<const char *>(&data_[pos_]), length - 1);
  pos_ += length;
  return value;
}

bool CdrDeserializer::deserialize_string(rosidl_runtime_c__String & value)
{
  uint32_t length = deserialize_uint32();
  if (length == 0 || !has_remaining(length)) {
    return false;
  }
  if (!rosidl_runtime_c__String__assignn(&value, reinterpret_cast<const char *>(&data_[pos_]),
    length - 1))
  {
    return false;
  }
  pos_ += length;
  return true;
}

// See serialize_wstring: length = character count (no null), 4 bytes per character.
std::u16string CdrDeserializer::deserialize_wstring()
{
  uint32_t length = deserialize_uint32();
  if (length == 0) {
    return {};
  }

  const size_t bytes = static_cast<size_t>(length) * sizeof(uint32_t);
  if (!has_remaining(bytes)) {
    return {};
  }

  std::u16string value;
  value.reserve(length);
  for (uint32_t i = 0; i < length; ++i) {
    value.push_back(static_cast<char16_t>(deserialize_uint32()));
  }
  return value;
}

bool CdrDeserializer::deserialize_wstring(rosidl_runtime_c__U16String & value)
{
  uint32_t length = deserialize_uint32();
  if (length == 0) {
    static constexpr uint16_t empty_string = 0;
    return rosidl_runtime_c__U16String__assignn(&value, &empty_string, 0);
  }

  const size_t bytes = static_cast<size_t>(length) * sizeof(uint32_t);
  if (!has_remaining(bytes)) {
    return false;
  }

  std::vector<uint16_t> decoded;
  decoded.reserve(length);
  for (uint32_t i = 0; i < length; ++i) {
    decoded.push_back(static_cast<uint16_t>(deserialize_uint32()));
  }

  return rosidl_runtime_c__U16String__assignn(&value, decoded.data(), decoded.size());
}

bool CdrDeserializer::deserialize_raw(void * data, size_t size)
{
  if (!has_remaining(size)) {
    return false;
  }
  std::memcpy(data, &data_[pos_], size);
  pos_ += size;
  return true;
}

bool CdrDeserializer::deserialize_message_c(
  void * ros_message,
  const rosidl_typesupport_introspection_c__MessageMembers * members)
{
  if (ros_message == nullptr || members == nullptr) {
    return false;
  }

  for (uint32_t i = 0; i < members->member_count_; ++i) {
    const auto & member = members->members_[i];
    void * member_data = static_cast<uint8_t *>(ros_message) + member.offset_;

    if (!deserialize_member_c(member_data, &member)) {
      return false;
    }
  }
  return true;
}

bool CdrDeserializer::deserialize_member_c(
  void * member_data,
  const rosidl_typesupport_introspection_c__MessageMember * member)
{
  if (member->is_array_) {
    size_t array_size = member->array_size_;
    void * collection_data = member_data;

    if (member->is_upper_bound_ || array_size == 0) {
              // Dynamic sequence
      array_size = deserialize_uint32();
      if (array_size > size_ - pos_) {
        return false;
      }

              // Resize sequence
      if (member->resize_function != nullptr) {
        if (!member->resize_function(member_data, array_size)) {
          return false;
        }
      }
    }

    const size_t element_size = fixed_primitive_size(member->type_id_);
    if (element_size != 0) {
      if (array_size == 0) {
        return true;
      }
      void * first_element = member->get_function != nullptr ?
        member->get_function(collection_data, 0) : nullptr;
      if (first_element != nullptr) {
        align(element_size);
        return deserialize_raw(first_element, array_size * element_size);
      }
    }

    for (size_t i = 0; i < array_size; ++i) {
      void * element_data = nullptr;
      if (member->get_function != nullptr) {
        element_data = member->get_function(collection_data, i);
      } else {
        // Fallback: this shouldn't happen in ROS2 Humble
        return false;
      }

      switch (member->type_id_) {
        case rosidl_typesupport_introspection_c__ROS_TYPE_BOOL:
          *static_cast<bool *>(element_data) = deserialize_bool();
          break;
        case rosidl_typesupport_introspection_c__ROS_TYPE_BYTE:
        case rosidl_typesupport_introspection_c__ROS_TYPE_UINT8:
          *static_cast<uint8_t *>(element_data) = deserialize_uint8();
          break;
        case rosidl_typesupport_introspection_c__ROS_TYPE_CHAR:
        case rosidl_typesupport_introspection_c__ROS_TYPE_INT8:
          *static_cast<int8_t *>(element_data) = deserialize_int8();
          break;
        case rosidl_typesupport_introspection_c__ROS_TYPE_INT16:
          *static_cast<int16_t *>(element_data) = deserialize_int16();
          break;
        case rosidl_typesupport_introspection_c__ROS_TYPE_UINT16:
          *static_cast<uint16_t *>(element_data) = deserialize_uint16();
          break;
        case rosidl_typesupport_introspection_c__ROS_TYPE_INT32:
          *static_cast<int32_t *>(element_data) = deserialize_int32();
          break;
        case rosidl_typesupport_introspection_c__ROS_TYPE_UINT32:
          *static_cast<uint32_t *>(element_data) = deserialize_uint32();
          break;
        case rosidl_typesupport_introspection_c__ROS_TYPE_INT64:
          *static_cast<int64_t *>(element_data) = deserialize_int64();
          break;
        case rosidl_typesupport_introspection_c__ROS_TYPE_UINT64:
          *static_cast<uint64_t *>(element_data) = deserialize_uint64();
          break;
        case rosidl_typesupport_introspection_c__ROS_TYPE_FLOAT:
          *static_cast<float *>(element_data) = deserialize_float32();
          break;
        case rosidl_typesupport_introspection_c__ROS_TYPE_DOUBLE:
          *static_cast<double *>(element_data) = deserialize_float64();
          break;
        case rosidl_typesupport_introspection_c__ROS_TYPE_STRING:
          if (!deserialize_string(*static_cast<rosidl_runtime_c__String *>(element_data))) {
            return false;
          }
          break;
        case rosidl_typesupport_introspection_c__ROS_TYPE_WSTRING:
          if (!deserialize_wstring(*static_cast<rosidl_runtime_c__U16String *>(element_data))) {
            return false;
          }
          break;
        case rosidl_typesupport_introspection_c__ROS_TYPE_MESSAGE:
          if (member->members_ != nullptr) {
            if (!deserialize_message_c(
                element_data,
                static_cast<const rosidl_typesupport_introspection_c__MessageMembers *>(
                  member->members_->data)))
            {
              return false;
            }
          }
          break;
        default:
          return false;
      }
    }
  } else {
    switch (member->type_id_) {
      case rosidl_typesupport_introspection_c__ROS_TYPE_BOOL:
        *static_cast<bool *>(member_data) = deserialize_bool();
        break;
      case rosidl_typesupport_introspection_c__ROS_TYPE_BYTE:
      case rosidl_typesupport_introspection_c__ROS_TYPE_UINT8:
        *static_cast<uint8_t *>(member_data) = deserialize_uint8();
        break;
      case rosidl_typesupport_introspection_c__ROS_TYPE_CHAR:
      case rosidl_typesupport_introspection_c__ROS_TYPE_INT8:
        *static_cast<int8_t *>(member_data) = deserialize_int8();
        break;
      case rosidl_typesupport_introspection_c__ROS_TYPE_INT16:
        *static_cast<int16_t *>(member_data) = deserialize_int16();
        break;
      case rosidl_typesupport_introspection_c__ROS_TYPE_UINT16:
        *static_cast<uint16_t *>(member_data) = deserialize_uint16();
        break;
      case rosidl_typesupport_introspection_c__ROS_TYPE_INT32:
        *static_cast<int32_t *>(member_data) = deserialize_int32();
        break;
      case rosidl_typesupport_introspection_c__ROS_TYPE_UINT32:
        *static_cast<uint32_t *>(member_data) = deserialize_uint32();
        break;
      case rosidl_typesupport_introspection_c__ROS_TYPE_INT64:
        *static_cast<int64_t *>(member_data) = deserialize_int64();
        break;
      case rosidl_typesupport_introspection_c__ROS_TYPE_UINT64:
        *static_cast<uint64_t *>(member_data) = deserialize_uint64();
        break;
      case rosidl_typesupport_introspection_c__ROS_TYPE_FLOAT:
        *static_cast<float *>(member_data) = deserialize_float32();
        break;
      case rosidl_typesupport_introspection_c__ROS_TYPE_DOUBLE:
        *static_cast<double *>(member_data) = deserialize_float64();
        break;
      case rosidl_typesupport_introspection_c__ROS_TYPE_STRING:
        if (!deserialize_string(*static_cast<rosidl_runtime_c__String *>(member_data))) {
          return false;
        }
        break;
      case rosidl_typesupport_introspection_c__ROS_TYPE_WSTRING:
        if (!deserialize_wstring(*static_cast<rosidl_runtime_c__U16String *>(member_data))) {
          return false;
        }
        break;
      case rosidl_typesupport_introspection_c__ROS_TYPE_MESSAGE:
        if (member->members_ != nullptr) {
          if (!deserialize_message_c(
              member_data,
              static_cast<const rosidl_typesupport_introspection_c__MessageMembers *>(
                member->members_->data)))
          {
            return false;
          }
        }
        break;
      default:
        return false;
    }
  }
  return true;
}

bool CdrDeserializer::deserialize_message_cpp(
  void * ros_message,
  const rosidl_typesupport_introspection_cpp::MessageMembers * members)
{
  if (ros_message == nullptr || members == nullptr) {
    return false;
  }

  for (uint32_t i = 0; i < members->member_count_; ++i) {
    const auto & member = members->members_[i];
    void * member_data = static_cast<uint8_t *>(ros_message) + member.offset_;

    if (!deserialize_member_cpp(member_data, &member)) {
      return false;
    }
  }
  return true;
}

bool CdrDeserializer::deserialize_member_cpp(
  void * member_data,
  const rosidl_typesupport_introspection_cpp::MessageMember * member)
{
  if (member->is_array_) {
    size_t array_size = member->array_size_;

    if (member->is_upper_bound_ || array_size == 0) {
      array_size = deserialize_uint32();
      if (array_size > size_ - pos_) {
        return false;
      }
      if (member->resize_function != nullptr) {
        member->resize_function(member_data, array_size);
      }
    }

    const size_t element_size = fixed_primitive_size(member->type_id_);
    if (element_size != 0) {
      if (array_size == 0) {
        return true;
      }
      void * first_element = member->get_function != nullptr ?
        member->get_function(member_data, 0) : nullptr;
      if (first_element != nullptr) {
        align(element_size);
        return deserialize_raw(first_element, array_size * element_size);
      }
    }

    for (size_t i = 0; i < array_size; ++i) {
      void * element_data = nullptr;
      if (member->get_function != nullptr) {
        element_data = member->get_function(member_data, i);
      }

      auto store_value = [&](const auto & value) -> bool {
          using ValueT = typename std::decay<decltype(value)>::type;
          if (element_data != nullptr) {
            *static_cast<ValueT *>(element_data) = value;
            return true;
          }
          if (member->assign_function != nullptr) {
            member->assign_function(member_data, i, &value);
            return true;
          }
          return false;
        };

      switch (member->type_id_) {
        case rosidl_typesupport_introspection_cpp::ROS_TYPE_BOOL: {
            const bool value = deserialize_bool();
            if (!store_value(value)) {
              return false;
            }
            break;
          }
        case rosidl_typesupport_introspection_cpp::ROS_TYPE_BYTE:
        case rosidl_typesupport_introspection_cpp::ROS_TYPE_UINT8: {
            const uint8_t value = deserialize_uint8();
            if (!store_value(value)) {
              return false;
            }
            break;
          }
        case rosidl_typesupport_introspection_cpp::ROS_TYPE_CHAR:
        case rosidl_typesupport_introspection_cpp::ROS_TYPE_INT8: {
            const int8_t value = deserialize_int8();
            if (!store_value(value)) {
              return false;
            }
            break;
          }
        case rosidl_typesupport_introspection_cpp::ROS_TYPE_INT16: {
            const int16_t value = deserialize_int16();
            if (!store_value(value)) {
              return false;
            }
            break;
          }
        case rosidl_typesupport_introspection_cpp::ROS_TYPE_UINT16: {
            const uint16_t value = deserialize_uint16();
            if (!store_value(value)) {
              return false;
            }
            break;
          }
        case rosidl_typesupport_introspection_cpp::ROS_TYPE_INT32: {
            const int32_t value = deserialize_int32();
            if (!store_value(value)) {
              return false;
            }
            break;
          }
        case rosidl_typesupport_introspection_cpp::ROS_TYPE_UINT32: {
            const uint32_t value = deserialize_uint32();
            if (!store_value(value)) {
              return false;
            }
            break;
          }
        case rosidl_typesupport_introspection_cpp::ROS_TYPE_INT64: {
            const int64_t value = deserialize_int64();
            if (!store_value(value)) {
              return false;
            }
            break;
          }
        case rosidl_typesupport_introspection_cpp::ROS_TYPE_UINT64: {
            const uint64_t value = deserialize_uint64();
            if (!store_value(value)) {
              return false;
            }
            break;
          }
        case rosidl_typesupport_introspection_cpp::ROS_TYPE_FLOAT: {
            const float value = deserialize_float32();
            if (!store_value(value)) {
              return false;
            }
            break;
          }
        case rosidl_typesupport_introspection_cpp::ROS_TYPE_DOUBLE: {
            const double value = deserialize_float64();
            if (!store_value(value)) {
              return false;
            }
            break;
          }
        case rosidl_typesupport_introspection_cpp::ROS_TYPE_STRING: {
            const std::string value = deserialize_string();
            if (!store_value(value)) {
              return false;
            }
            break;
          }
        case rosidl_typesupport_introspection_cpp::ROS_TYPE_WSTRING: {
            const std::u16string value = deserialize_wstring();
            if (!store_value(value)) {
              return false;
            }
            break;
          }
        case rosidl_typesupport_introspection_cpp::ROS_TYPE_MESSAGE:
          if (element_data == nullptr) {
            return false;
          }
          if (member->members_ != nullptr) {
            if (!deserialize_message_cpp(
                element_data,
                static_cast<const rosidl_typesupport_introspection_cpp::MessageMembers *>(
                  member->members_->data)))
            {
              return false;
            }
          }
          break;
        default:
          return false;
      }
    }
  } else {
    switch (member->type_id_) {
      case rosidl_typesupport_introspection_cpp::ROS_TYPE_BOOL:
        *static_cast<bool *>(member_data) = deserialize_bool();
        break;
      case rosidl_typesupport_introspection_cpp::ROS_TYPE_BYTE:
      case rosidl_typesupport_introspection_cpp::ROS_TYPE_UINT8:
        *static_cast<uint8_t *>(member_data) = deserialize_uint8();
        break;
      case rosidl_typesupport_introspection_cpp::ROS_TYPE_CHAR:
      case rosidl_typesupport_introspection_cpp::ROS_TYPE_INT8:
        *static_cast<int8_t *>(member_data) = deserialize_int8();
        break;
      case rosidl_typesupport_introspection_cpp::ROS_TYPE_INT16:
        *static_cast<int16_t *>(member_data) = deserialize_int16();
        break;
      case rosidl_typesupport_introspection_cpp::ROS_TYPE_UINT16:
        *static_cast<uint16_t *>(member_data) = deserialize_uint16();
        break;
      case rosidl_typesupport_introspection_cpp::ROS_TYPE_INT32:
        *static_cast<int32_t *>(member_data) = deserialize_int32();
        break;
      case rosidl_typesupport_introspection_cpp::ROS_TYPE_UINT32:
        *static_cast<uint32_t *>(member_data) = deserialize_uint32();
        break;
      case rosidl_typesupport_introspection_cpp::ROS_TYPE_INT64:
        *static_cast<int64_t *>(member_data) = deserialize_int64();
        break;
      case rosidl_typesupport_introspection_cpp::ROS_TYPE_UINT64:
        *static_cast<uint64_t *>(member_data) = deserialize_uint64();
        break;
      case rosidl_typesupport_introspection_cpp::ROS_TYPE_FLOAT:
        *static_cast<float *>(member_data) = deserialize_float32();
        break;
      case rosidl_typesupport_introspection_cpp::ROS_TYPE_DOUBLE:
        *static_cast<double *>(member_data) = deserialize_float64();
        break;
      case rosidl_typesupport_introspection_cpp::ROS_TYPE_STRING:
        *static_cast<std::string *>(member_data) = deserialize_string();
        break;
      case rosidl_typesupport_introspection_cpp::ROS_TYPE_WSTRING:
        *static_cast<std::u16string *>(member_data) = deserialize_wstring();
        break;
      case rosidl_typesupport_introspection_cpp::ROS_TYPE_MESSAGE:
        if (member->members_ != nullptr) {
          if (!deserialize_message_cpp(
              member_data,
              static_cast<const rosidl_typesupport_introspection_cpp::MessageMembers *>(
                member->members_->data)))
          {
            return false;
          }
        }
        break;
      default:
        return false;
    }
  }
  return true;
}

}  // namespace rmw_int2dds_cpp
