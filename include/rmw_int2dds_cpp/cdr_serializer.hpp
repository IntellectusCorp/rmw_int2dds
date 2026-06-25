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

#ifndef RMW_INT2DDS_CPP__CDR_SERIALIZER_HPP_
#define RMW_INT2DDS_CPP__CDR_SERIALIZER_HPP_

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "rosidl_runtime_c/string.h"
#include "rosidl_runtime_c/string_functions.h"
#include "rosidl_runtime_c/u16string.h"
#include "rosidl_runtime_c/u16string_functions.h"
#include "rosidl_runtime_c/primitives_sequence.h"
#include "rosidl_runtime_c/primitives_sequence_functions.h"
#include "rosidl_typesupport_introspection_c/message_introspection.h"
#include "rosidl_typesupport_introspection_c/field_types.h"
#include "rosidl_typesupport_introspection_cpp/message_introspection.hpp"
#include "rosidl_typesupport_introspection_cpp/field_types.hpp"

namespace rmw_int2dds_cpp
{

// CDR encapsulation header for Little Endian
constexpr uint8_t CDR_ENCAPSULATION_LE[4] = {0x00, 0x01, 0x00, 0x00};

/// CDR Serializer class for ROS2 message serialization
class CdrSerializer
{
public:
  CdrSerializer();
  CdrSerializer(uint8_t * external_buffer, size_t external_capacity);
  ~CdrSerializer() = default;

  /// Reset the serializer for reuse
  void reset();

  /// Get the serialized buffer
  const std::vector<uint8_t> & get_buffer() const {return buffer_;}

  /// Get the serialized data pointer
  const uint8_t * get_data() const
  {
    return external_buffer_ != nullptr ? external_buffer_ : buffer_.data();
  }

  /// Get the serialized size
  size_t get_size() const {return pos_;}

  bool has_error() const {return overflow_;}

  // Primitive type serialization
  void serialize_bool(bool value);
  void serialize_byte(uint8_t value);
  void serialize_char(char value);
  void serialize_int8(int8_t value);
  void serialize_uint8(uint8_t value);
  void serialize_int16(int16_t value);
  void serialize_uint16(uint16_t value);
  void serialize_int32(int32_t value);
  void serialize_uint32(uint32_t value);
  void serialize_int64(int64_t value);
  void serialize_uint64(uint64_t value);
  void serialize_float32(float value);
  void serialize_float64(double value);

  // String serialization
  void serialize_string(const std::string & value);
  void serialize_string(const rosidl_runtime_c__String & value);
  void serialize_wstring(const std::u16string & value);
  void serialize_wstring(const rosidl_runtime_c__U16String & value);

  // Raw data serialization
  void serialize_raw(const void * data, size_t size);

  // Array serialization (fixed size)
  template<typename T>
  void serialize_array(const T * data, size_t count);

  // Sequence serialization (dynamic size)
  template<typename T>
  void serialize_sequence(const T * data, size_t count);

  /// Serialize a ROS message using C type support introspection
  bool serialize_message_c(
    const void * ros_message,
    const rosidl_typesupport_introspection_c__MessageMembers * members);

  /// Serialize a ROS message using C++ type support introspection
  bool serialize_message_cpp(
    const void * ros_message,
    const rosidl_typesupport_introspection_cpp::MessageMembers * members);

private:
  void align(size_t alignment);
  void ensure_capacity(size_t additional);
  void append_byte(uint8_t value);
  void append_bytes(const void * data, size_t size);
  void append_zeros(size_t count);
  uint8_t * append_uninitialized(size_t size);

  bool serialize_member_c(
    const void * member_data,
    const rosidl_typesupport_introspection_c__MessageMember * member);

  bool serialize_member_cpp(
    const void * member_data,
    const rosidl_typesupport_introspection_cpp::MessageMember * member);

  std::vector<uint8_t> buffer_;
  uint8_t * external_buffer_{nullptr};
  size_t external_capacity_{0};
  size_t pos_;
  bool overflow_{false};
};

/// CDR Deserializer class for ROS2 message deserialization
class CdrDeserializer
{
public:
  CdrDeserializer(const uint8_t * data, size_t size);
  ~CdrDeserializer() = default;

  /// Check if there's enough data remaining
  bool has_remaining(size_t count) const {return pos_ + count <= size_;}

  /// Get current position
  size_t get_position() const {return pos_;}

  // Primitive type deserialization
  bool deserialize_bool();
  uint8_t deserialize_byte();
  char deserialize_char();
  int8_t deserialize_int8();
  uint8_t deserialize_uint8();
  int16_t deserialize_int16();
  uint16_t deserialize_uint16();
  int32_t deserialize_int32();
  uint32_t deserialize_uint32();
  int64_t deserialize_int64();
  uint64_t deserialize_uint64();
  float deserialize_float32();
  double deserialize_float64();

  // String deserialization
  std::string deserialize_string();
  bool deserialize_string(rosidl_runtime_c__String & value);
  std::u16string deserialize_wstring();
  bool deserialize_wstring(rosidl_runtime_c__U16String & value);

  // Raw data deserialization
  bool deserialize_raw(void * data, size_t size);

  /// Deserialize a ROS message using C type support introspection
  bool deserialize_message_c(
    void * ros_message,
    const rosidl_typesupport_introspection_c__MessageMembers * members);

  /// Deserialize a ROS message using C++ type support introspection
  bool deserialize_message_cpp(
    void * ros_message,
    const rosidl_typesupport_introspection_cpp::MessageMembers * members);

private:
  void align(size_t alignment);

  bool deserialize_member_c(
    void * member_data,
    const rosidl_typesupport_introspection_c__MessageMember * member);

  bool deserialize_member_cpp(
    void * member_data,
    const rosidl_typesupport_introspection_cpp::MessageMember * member);

  const uint8_t * data_;
  size_t size_;
  size_t pos_;
};

}  // namespace rmw_int2dds_cpp

#endif  // RMW_INT2DDS_CPP__CDR_SERIALIZER_HPP_
