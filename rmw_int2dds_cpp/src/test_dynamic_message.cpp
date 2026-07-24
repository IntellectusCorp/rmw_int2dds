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
// C3 verification. Two phases, both judged only by the actual value read back:
//   Phase 1 (foundation): the int2dds FFI dynamic building blocks directly --
//     build a dynamic type, read a primitive out of a real CDR buffer.
//   Phase 2 (bridge): the same read but driven through the rosidl_dynamic_typesupport
//     public API, which exercises this rmw's serialization-support vtable end to end
//     (rmw_serialization_support_init -> our vtable -> int2dds FFI).
// The CDR is produced by the real int2dds serializer (never hand-crafted).

#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <string>

#include "rcutils/allocator.h"

#include "rclcpp/serialization.hpp"
#include "rclcpp/serialized_message.hpp"
#include "std_msgs/msg/int32.hpp"

#if __has_include("geometry_msgs/msg/vector3.hpp")
#include "geometry_msgs/msg/vector3.hpp"
#define HAVE_VECTOR3 1
#endif

#if __has_include("geometry_msgs/msg/pose.hpp")
#include "geometry_msgs/msg/pose.hpp"
#define HAVE_POSE 1
#endif

#if __has_include("geometry_msgs/msg/polygon.hpp")
#include "geometry_msgs/msg/polygon.hpp"
#define HAVE_POLYGON 1
#endif

#if __has_include("test_msgs/msg/basic_types.hpp")
#include "test_msgs/msg/basic_types.hpp"
#define HAVE_BASICTYPES 1
#endif

#if __has_include("diagnostic_msgs/msg/key_value.hpp")
#include "diagnostic_msgs/msg/key_value.hpp"
#define HAVE_KEYVALUE 1
#endif

#if __has_include("test_msgs/msg/w_strings.hpp")
#include "test_msgs/msg/w_strings.hpp"
#define HAVE_WSTRINGS 1
#endif

#if __has_include("test_msgs/msg/arrays.hpp")
#include "test_msgs/msg/arrays.hpp"
#define HAVE_ARRAYS 1
#endif

#if __has_include("test_msgs/msg/unbounded_sequences.hpp")
#include "test_msgs/msg/unbounded_sequences.hpp"
#define HAVE_USEQ 1
#endif

#include "rmw/rmw.h"
#include "rmw/dynamic_message_type_support.h"

#include "rosidl_dynamic_typesupport/api/serialization_support.h"
#include "rosidl_dynamic_typesupport/api/dynamic_type.h"
#include "rosidl_dynamic_typesupport/api/dynamic_data.h"
#include "rosidl_dynamic_typesupport/types.h"

extern "C"
{
#include "int2dds-ffi.h"  // NOLINT(build/include_subdir): vendored FFI header
}

namespace
{
// Field type codes are the INT2DDS_FIELD_* constants (int2dds-ffi.h), not XTypes TypeKind.
constexpr int32_t FIELD_INT32 = INT2DDS_FIELD_INT32;
constexpr int32_t FIELD_FLOAT64 = INT2DDS_FIELD_FLOAT64;
}

// Phases 7-10 (arrays and sequences of primitives), split out of main() so each function
// stays focused. Phases 7-8 are direct-FFI spikes isolating the flat-sample path; phases
// 9-10 drive the rosidl loan model through this rmw's vtable.
void run_collection_phases(bool * p7, bool * p8, bool * p9, bool * p10);

void run_collection_phases(bool * p7, bool * p8, bool * p9, bool * p10)
{
  // ---- Phase 7: fixed-array element reads by indexed path (direct FFI spike) ----
  // Capability check: do the flat sample accessors resolve indexed paths like
  // "bool_values[2]" and offsets past a preceding array? (test_msgs/Arrays)
  bool phase7_ok = true;
#ifdef HAVE_ARRAYS
  phase7_ok = false;
  {
    test_msgs::msg::Arrays arr;
    arr.bool_values = {{true, false, true}};
    arr.byte_values = {{10, 20, 30}};
    rclcpp::Serialization<test_msgs::msg::Arrays> aser;
    rclcpp::SerializedMessage aserialized;
    aser.serialize_message(&arr, &aserialized);
    const rcl_serialized_message_t & acdr = aserialized.get_rcl_serialized_message();

    Int2DdsTypeInfo * ai = nullptr;
    Int2DdsTypeObject * aobj = nullptr;
    const bool built =
      int2dds_type_info_create("test_msgs::msg::Arrays", 0, &ai) == INT2DDS_RET_OK &&
      int2dds_type_info_add_array_field(ai, "bool_values", INT2DDS_FIELD_BOOL, 3, 0) ==
      INT2DDS_RET_OK &&
      int2dds_type_info_add_array_field(ai, "byte_values", INT2DDS_FIELD_BYTE, 3, 0) ==
      INT2DDS_RET_OK &&
      int2dds_type_info_to_type_object(ai, &aobj) == INT2DDS_RET_OK;
    if (built) {
      bool b[3] = {false, false, false};
      uint8_t y[3] = {0, 0, 0};
      Int2DdsRet rc = INT2DDS_RET_OK;
      for (int i = 0; i < 3 && rc == INT2DDS_RET_OK; ++i) {
        char path[32];
        std::snprintf(path, sizeof(path), "bool_values[%d]", i);
        rc = int2dds_dynamic_sample_get_bool(acdr.buffer, acdr.buffer_length, aobj, path, &b[i]);
      }
      for (int i = 0; i < 3 && rc == INT2DDS_RET_OK; ++i) {
        char path[32];
        std::snprintf(path, sizeof(path), "byte_values[%d]", i);
        rc = int2dds_dynamic_sample_get_byte(acdr.buffer, acdr.buffer_length, aobj, path, &y[i]);
      }
      std::printf(
        "[phase7 array]   rc=%d bool=[%d,%d,%d] byte=[%d,%d,%d] (expect 1,0,1 / 10,20,30)\n",
        static_cast<int>(rc), b[0], b[1], b[2], y[0], y[1], y[2]);
      phase7_ok = (rc == INT2DDS_RET_OK && b[0] && !b[1] && b[2] &&
        y[0] == 10 && y[1] == 20 && y[2] == 30);
    } else {
      std::printf("[phase7 array]   FAIL: type build\n");
    }
    if (aobj) {int2dds_type_object_destroy(aobj);}
    if (ai) {int2dds_type_info_destroy(ai);}
  }
#else
  std::printf("[phase7 array]   SKIP: test_msgs/Arrays unavailable\n");
#endif

  // ---- Phase 8: unbounded-sequence element reads + out-of-range probe (direct FFI) ----
  // Also checks whether an out-of-range index returns FIELD_NOT_FOUND (200), which would
  // let the vtable discover a sequence length by probing. (test_msgs/UnboundedSequences)
  bool phase8_ok = true;
#ifdef HAVE_USEQ
  phase8_ok = false;
  {
    test_msgs::msg::UnboundedSequences seq;
    seq.bool_values = {true, false};
    seq.byte_values = {40, 50, 60};
    rclcpp::Serialization<test_msgs::msg::UnboundedSequences> sser;
    rclcpp::SerializedMessage sserialized;
    sser.serialize_message(&seq, &sserialized);
    const rcl_serialized_message_t & scdr = sserialized.get_rcl_serialized_message();

    Int2DdsTypeInfo * si = nullptr;
    Int2DdsTypeObject * sobj = nullptr;
    const bool built =
      int2dds_type_info_create("test_msgs::msg::UnboundedSequences", 0, &si) == INT2DDS_RET_OK &&
      int2dds_type_info_add_sequence_field(si, "bool_values", INT2DDS_FIELD_BOOL, 0, 0) ==
      INT2DDS_RET_OK &&
      int2dds_type_info_add_sequence_field(si, "byte_values", INT2DDS_FIELD_BYTE, 0, 0) ==
      INT2DDS_RET_OK &&
      int2dds_type_info_to_type_object(si, &sobj) == INT2DDS_RET_OK;
    if (built) {
      bool b0 = false;
      bool b1 = true;
      uint8_t y[3] = {0, 0, 0};
      Int2DdsRet rb0 = int2dds_dynamic_sample_get_bool(
        scdr.buffer, scdr.buffer_length, sobj, "bool_values[0]", &b0);
      Int2DdsRet rb1 = int2dds_dynamic_sample_get_bool(
        scdr.buffer, scdr.buffer_length, sobj, "bool_values[1]", &b1);
      bool oob = false;
      Int2DdsRet roob = int2dds_dynamic_sample_get_bool(
        scdr.buffer, scdr.buffer_length, sobj, "bool_values[2]", &oob);
      Int2DdsRet ry = INT2DDS_RET_OK;
      for (int i = 0; i < 3 && ry == INT2DDS_RET_OK; ++i) {
        char path[32];
        std::snprintf(path, sizeof(path), "byte_values[%d]", i);
        ry = int2dds_dynamic_sample_get_byte(scdr.buffer, scdr.buffer_length, sobj, path, &y[i]);
      }
      std::printf(
        "[phase8 seq]     bool[0]=%d(rc%d) bool[1]=%d(rc%d) oob rc=%d(expect 200) "
        "byte=[%d,%d,%d](rc%d) (expect 1,0 / 40,50,60)\n",
        b0, static_cast<int>(rb0), b1, static_cast<int>(rb1), static_cast<int>(roob),
        y[0], y[1], y[2], static_cast<int>(ry));
      phase8_ok = (rb0 == INT2DDS_RET_OK && b0 && rb1 == INT2DDS_RET_OK && !b1 &&
        roob == INT2DDS_RET_DYNAMIC_FIELD_NOT_FOUND && ry == INT2DDS_RET_OK &&
        y[0] == 40 && y[1] == 50 && y[2] == 60);
    } else {
      std::printf("[phase8 seq]     FAIL: type build\n");
    }
    if (sobj) {int2dds_type_object_destroy(sobj);}
    if (si) {int2dds_type_info_destroy(si);}
  }
#else
  std::printf("[phase8 seq]     SKIP: test_msgs/UnboundedSequences unavailable\n");
#endif

  // ---- Phase 9: fixed array through the vtable loan model (test_msgs/Arrays) ----
  // Drives the real rosidl loan path: loan_value(member) -> get_item_count -> element get,
  // exercising this rmw's loan_value/get_item_count/return_loaned_value implementation.
  bool phase9_ok = true;
#ifdef HAVE_ARRAYS
  phase9_ok = false;
  {
    test_msgs::msg::Arrays arr;
    arr.bool_values = {{true, false, true}};
    arr.byte_values = {{11, 22, 33}};
    rclcpp::Serialization<test_msgs::msg::Arrays> aser;
    rclcpp::SerializedMessage aserialized;
    aser.serialize_message(&arr, &aserialized);
    const rcl_serialized_message_t & acdr = aserialized.get_rcl_serialized_message();

    rcutils_allocator_t alloc = rcutils_get_default_allocator();
    rosidl_dynamic_typesupport_serialization_support_t ss =
      rosidl_dynamic_typesupport_get_zero_initialized_serialization_support();
    if (rmw_serialization_support_init("", &alloc, &ss) == RMW_RET_OK) {
      rosidl_dynamic_typesupport_dynamic_type_builder_t bld =
        rosidl_dynamic_typesupport_get_zero_initialized_dynamic_type_builder();
      rosidl_dynamic_typesupport_dynamic_type_t type =
        rosidl_dynamic_typesupport_get_zero_initialized_dynamic_type();
      rosidl_dynamic_typesupport_dynamic_data_t data =
        rosidl_dynamic_typesupport_get_zero_initialized_dynamic_data();
      rcutils_ret_t rc = RCUTILS_RET_OK;
#define P9STEP(call) do {if (rc == RCUTILS_RET_OK) {rc = (call);}} while (0)
      P9STEP(rosidl_dynamic_typesupport_dynamic_type_builder_init(
          &ss, "test_msgs::msg::Arrays", 22, &alloc, &bld));
      P9STEP(rosidl_dynamic_typesupport_dynamic_type_builder_add_bool_array_member(
          &bld, 0, "bool_values", 11, "", 0, 3));
      P9STEP(rosidl_dynamic_typesupport_dynamic_type_builder_add_byte_array_member(
          &bld, 1, "byte_values", 11, "", 0, 3));
      P9STEP(rosidl_dynamic_typesupport_dynamic_type_init_from_dynamic_type_builder(
          &bld, &alloc, &type));
      P9STEP(rosidl_dynamic_typesupport_dynamic_data_init_from_dynamic_type(&type, &alloc, &data));
      {
        rcutils_uint8_array_t buf = acdr;
        P9STEP(rosidl_dynamic_typesupport_dynamic_data_deserialize(&data, &buf));
      }
      size_t bcount = 0;
      size_t ycount = 0;
      bool b[3] = {false, false, false};
      uint8_t y[3] = {0, 0, 0};
      // Loan the bool array, count and read its elements.
      rosidl_dynamic_typesupport_dynamic_data_t lb =
        rosidl_dynamic_typesupport_get_zero_initialized_dynamic_data();
      P9STEP(rosidl_dynamic_typesupport_dynamic_data_loan_value(&data, 0, &alloc, &lb));
      P9STEP(rosidl_dynamic_typesupport_dynamic_data_get_item_count(&lb, &bcount));
      for (size_t i = 0; i < 3 && rc == RCUTILS_RET_OK; ++i) {
        P9STEP(rosidl_dynamic_typesupport_dynamic_data_get_bool_value(&lb, i, &b[i]));
      }
      P9STEP(rosidl_dynamic_typesupport_dynamic_data_return_loaned_value(&data, &lb));
      // Loan the byte array (second member, offset past the first array).
      rosidl_dynamic_typesupport_dynamic_data_t ly =
        rosidl_dynamic_typesupport_get_zero_initialized_dynamic_data();
      P9STEP(rosidl_dynamic_typesupport_dynamic_data_loan_value(&data, 1, &alloc, &ly));
      P9STEP(rosidl_dynamic_typesupport_dynamic_data_get_item_count(&ly, &ycount));
      for (size_t i = 0; i < 3 && rc == RCUTILS_RET_OK; ++i) {
        P9STEP(rosidl_dynamic_typesupport_dynamic_data_get_byte_value(&ly, i, &y[i]));
      }
      P9STEP(rosidl_dynamic_typesupport_dynamic_data_return_loaned_value(&data, &ly));
#undef P9STEP
      std::printf(
        "[phase9 arr-vt]  rc=%d bcount=%zu bool=[%d,%d,%d] ycount=%zu byte=[%d,%d,%d] "
        "(expect 3/1,0,1 3/11,22,33)\n",
        static_cast<int>(rc), bcount, b[0], b[1], b[2], ycount, y[0], y[1], y[2]);
      phase9_ok = (rc == RCUTILS_RET_OK && bcount == 3 && b[0] && !b[1] && b[2] &&
        ycount == 3 && y[0] == 11 && y[1] == 22 && y[2] == 33);
    } else {
      std::printf("[phase9 arr-vt]  FAIL: rmw_serialization_support_init\n");
    }
  }
#else
  std::printf("[phase9 arr-vt]  SKIP: test_msgs/Arrays unavailable\n");
#endif

  // ---- Phase 10: unbounded sequences through the vtable loan model ----
  // Different element counts (3 then 2) exercise get_item_count's probe and the offset
  // past a variable-length sequence. (test_msgs/UnboundedSequences)
  bool phase10_ok = true;
#ifdef HAVE_USEQ
  phase10_ok = false;
  {
    test_msgs::msg::UnboundedSequences seq;
    seq.bool_values = {true, true, false};
    seq.byte_values = {44, 55};
    rclcpp::Serialization<test_msgs::msg::UnboundedSequences> sser;
    rclcpp::SerializedMessage sserialized;
    sser.serialize_message(&seq, &sserialized);
    const rcl_serialized_message_t & scdr = sserialized.get_rcl_serialized_message();

    rcutils_allocator_t alloc = rcutils_get_default_allocator();
    rosidl_dynamic_typesupport_serialization_support_t ss =
      rosidl_dynamic_typesupport_get_zero_initialized_serialization_support();
    if (rmw_serialization_support_init("", &alloc, &ss) == RMW_RET_OK) {
      rosidl_dynamic_typesupport_dynamic_type_builder_t bld =
        rosidl_dynamic_typesupport_get_zero_initialized_dynamic_type_builder();
      rosidl_dynamic_typesupport_dynamic_type_t type =
        rosidl_dynamic_typesupport_get_zero_initialized_dynamic_type();
      rosidl_dynamic_typesupport_dynamic_data_t data =
        rosidl_dynamic_typesupport_get_zero_initialized_dynamic_data();
      rcutils_ret_t rc = RCUTILS_RET_OK;
#define P10STEP(call) do {if (rc == RCUTILS_RET_OK) {rc = (call);}} while (0)
      P10STEP(rosidl_dynamic_typesupport_dynamic_type_builder_init(
          &ss, "test_msgs::msg::UnboundedSequences", 33, &alloc, &bld));
      P10STEP(rosidl_dynamic_typesupport_dynamic_type_builder_add_bool_unbounded_sequence_member(
          &bld, 0, "bool_values", 11, "", 0));
      P10STEP(rosidl_dynamic_typesupport_dynamic_type_builder_add_byte_unbounded_sequence_member(
          &bld, 1, "byte_values", 11, "", 0));
      P10STEP(rosidl_dynamic_typesupport_dynamic_type_init_from_dynamic_type_builder(
          &bld, &alloc, &type));
      P10STEP(rosidl_dynamic_typesupport_dynamic_data_init_from_dynamic_type(&type, &alloc, &data));
      {
        rcutils_uint8_array_t buf = scdr;
        P10STEP(rosidl_dynamic_typesupport_dynamic_data_deserialize(&data, &buf));
      }
      size_t bcount = 0;
      size_t ycount = 0;
      bool b[3] = {false, false, true};
      uint8_t y[2] = {0, 0};
      rosidl_dynamic_typesupport_dynamic_data_t lb =
        rosidl_dynamic_typesupport_get_zero_initialized_dynamic_data();
      P10STEP(rosidl_dynamic_typesupport_dynamic_data_loan_value(&data, 0, &alloc, &lb));
      P10STEP(rosidl_dynamic_typesupport_dynamic_data_get_item_count(&lb, &bcount));
      for (size_t i = 0; i < 3 && rc == RCUTILS_RET_OK; ++i) {
        P10STEP(rosidl_dynamic_typesupport_dynamic_data_get_bool_value(&lb, i, &b[i]));
      }
      P10STEP(rosidl_dynamic_typesupport_dynamic_data_return_loaned_value(&data, &lb));
      rosidl_dynamic_typesupport_dynamic_data_t ly =
        rosidl_dynamic_typesupport_get_zero_initialized_dynamic_data();
      P10STEP(rosidl_dynamic_typesupport_dynamic_data_loan_value(&data, 1, &alloc, &ly));
      P10STEP(rosidl_dynamic_typesupport_dynamic_data_get_item_count(&ly, &ycount));
      for (size_t i = 0; i < 2 && rc == RCUTILS_RET_OK; ++i) {
        P10STEP(rosidl_dynamic_typesupport_dynamic_data_get_byte_value(&ly, i, &y[i]));
      }
      P10STEP(rosidl_dynamic_typesupport_dynamic_data_return_loaned_value(&data, &ly));
#undef P10STEP
      std::printf(
        "[phase10 seq-vt] rc=%d bcount=%zu bool=[%d,%d,%d] ycount=%zu byte=[%d,%d] "
        "(expect 3/1,1,0 2/44,55)\n",
        static_cast<int>(rc), bcount, b[0], b[1], b[2], ycount, y[0], y[1]);
      phase10_ok = (rc == RCUTILS_RET_OK && bcount == 3 && b[0] && b[1] && !b[2] &&
        ycount == 2 && y[0] == 44 && y[1] == 55);
    } else {
      std::printf("[phase10 seq-vt] FAIL: rmw_serialization_support_init\n");
    }
  }
#else
  std::printf("[phase10 seq-vt] SKIP: test_msgs/UnboundedSequences unavailable\n");
#endif

  *p7 = phase7_ok;
  *p8 = phase8_ok;
  *p9 = phase9_ok;
  *p10 = phase10_ok;
}

// Phase 11: nested struct via flattening (direct FFI spike). For FINAL extensibility a
// nested struct is encoded inline with no member header, so a flat type describing the
// leaf fields in CDR order reads them back. This probes whether nested is reachable
// RMW-side (the add_named_type_field path is not: without a type registry the flat
// decoder resolves a nested reference to an opaque ExternalType).
void run_nested_phase(bool * p11);

void run_nested_phase(bool * p11)
{
  bool phase11_ok = true;
#ifdef HAVE_POSE
  phase11_ok = false;
  {
    geometry_msgs::msg::Pose pose;
    pose.position.x = 1.5;
    pose.position.y = 2.5;
    pose.position.z = 3.5;
    pose.orientation.x = 0.125;
    pose.orientation.y = 0.25;
    pose.orientation.z = 0.375;
    pose.orientation.w = 0.5;
    rclcpp::Serialization<geometry_msgs::msg::Pose> pser;
    rclcpp::SerializedMessage pserialized;
    pser.serialize_message(&pose, &pserialized);
    const rcl_serialized_message_t & pcdr = pserialized.get_rcl_serialized_message();

    const char * names[7] = {
      "position_x", "position_y", "position_z",
      "orientation_x", "orientation_y", "orientation_z", "orientation_w"};
    Int2DdsTypeInfo * pi = nullptr;
    Int2DdsTypeObject * pobj = nullptr;
    bool built = int2dds_type_info_create("geometry_msgs::msg::Pose", 0, &pi) == INT2DDS_RET_OK;
    for (int i = 0; i < 7 && built; ++i) {
      built = int2dds_type_info_add_field(pi, names[i], INT2DDS_FIELD_FLOAT64, 0) == INT2DDS_RET_OK;
    }
    built = built && int2dds_type_info_to_type_object(pi, &pobj) == INT2DDS_RET_OK;
    if (built) {
      double v[7] = {0, 0, 0, 0, 0, 0, 0};
      Int2DdsRet rc = INT2DDS_RET_OK;
      for (int i = 0; i < 7 && rc == INT2DDS_RET_OK; ++i) {
        rc = int2dds_dynamic_sample_get_f64(pcdr.buffer, pcdr.buffer_length, pobj, names[i], &v[i]);
      }
      std::printf(
        "[phase11 nested] rc=%d vals=[%g %g %g %g %g %g %g] "
        "(expect 1.5 2.5 3.5 0.125 0.25 0.375 0.5)\n",
        static_cast<int>(rc), v[0], v[1], v[2], v[3], v[4], v[5], v[6]);
      phase11_ok = (rc == INT2DDS_RET_OK && v[0] == 1.5 && v[1] == 2.5 && v[2] == 3.5 &&
        v[3] == 0.125 && v[4] == 0.25 && v[5] == 0.375 && v[6] == 0.5);
    } else {
      std::printf("[phase11 nested] FAIL: type build\n");
    }
    if (pobj) {int2dds_type_object_destroy(pobj);}
    if (pi) {int2dds_type_info_destroy(pi);}
  }
#else
  std::printf("[phase11 nested] SKIP: geometry_msgs/Pose unavailable\n");
#endif
  *p11 = phase11_ok;
}

// Phase 12: nested struct through this rmw's vtable (add_complex_member + loan model).
// Builds Point and Quaternion nested types, composes them into Pose via add_complex_member,
// then loans each nested member and reads its float64 leaves. This exercises the flatten
// build path and the nested loan navigation end to end (not just the direct-FFI spike).
void run_nested_vtable_phase(bool * p12);

void run_nested_vtable_phase(bool * p12)
{
  bool phase12_ok = true;
#ifdef HAVE_POSE
  phase12_ok = false;
  {
    geometry_msgs::msg::Pose pose;
    pose.position.x = 1.5;
    pose.position.y = 2.5;
    pose.position.z = 3.5;
    pose.orientation.x = 0.125;
    pose.orientation.y = 0.25;
    pose.orientation.z = 0.375;
    pose.orientation.w = 0.5;
    rclcpp::Serialization<geometry_msgs::msg::Pose> pser;
    rclcpp::SerializedMessage pserialized;
    pser.serialize_message(&pose, &pserialized);
    const rcl_serialized_message_t & pcdr = pserialized.get_rcl_serialized_message();

    rcutils_allocator_t alloc = rcutils_get_default_allocator();
    rosidl_dynamic_typesupport_serialization_support_t ss =
      rosidl_dynamic_typesupport_get_zero_initialized_serialization_support();
    if (rmw_serialization_support_init("", &alloc, &ss) == RMW_RET_OK) {
      rosidl_dynamic_typesupport_dynamic_type_builder_t pbld =
        rosidl_dynamic_typesupport_get_zero_initialized_dynamic_type_builder();
      rosidl_dynamic_typesupport_dynamic_type_t ptype =
        rosidl_dynamic_typesupport_get_zero_initialized_dynamic_type();
      rosidl_dynamic_typesupport_dynamic_type_builder_t qbld =
        rosidl_dynamic_typesupport_get_zero_initialized_dynamic_type_builder();
      rosidl_dynamic_typesupport_dynamic_type_t qtype =
        rosidl_dynamic_typesupport_get_zero_initialized_dynamic_type();
      rosidl_dynamic_typesupport_dynamic_type_builder_t posebld =
        rosidl_dynamic_typesupport_get_zero_initialized_dynamic_type_builder();
      rosidl_dynamic_typesupport_dynamic_type_t posetype =
        rosidl_dynamic_typesupport_get_zero_initialized_dynamic_type();
      rosidl_dynamic_typesupport_dynamic_data_t data =
        rosidl_dynamic_typesupport_get_zero_initialized_dynamic_data();
      rcutils_ret_t rc = RCUTILS_RET_OK;
#define P12STEP(call) do {if (rc == RCUTILS_RET_OK) {rc = (call);}} while (0)
      P12STEP(rosidl_dynamic_typesupport_dynamic_type_builder_init(
          &ss, "geometry_msgs::msg::Point", 25, &alloc, &pbld));
      P12STEP(rosidl_dynamic_typesupport_dynamic_type_builder_add_float64_member(
          &pbld, 0, "x", 1, "", 0));
      P12STEP(rosidl_dynamic_typesupport_dynamic_type_builder_add_float64_member(
          &pbld, 1, "y", 1, "", 0));
      P12STEP(rosidl_dynamic_typesupport_dynamic_type_builder_add_float64_member(
          &pbld, 2, "z", 1, "", 0));
      P12STEP(rosidl_dynamic_typesupport_dynamic_type_init_from_dynamic_type_builder(
          &pbld, &alloc, &ptype));
      P12STEP(rosidl_dynamic_typesupport_dynamic_type_builder_init(
          &ss, "geometry_msgs::msg::Quaternion", 30, &alloc, &qbld));
      P12STEP(rosidl_dynamic_typesupport_dynamic_type_builder_add_float64_member(
          &qbld, 0, "x", 1, "", 0));
      P12STEP(rosidl_dynamic_typesupport_dynamic_type_builder_add_float64_member(
          &qbld, 1, "y", 1, "", 0));
      P12STEP(rosidl_dynamic_typesupport_dynamic_type_builder_add_float64_member(
          &qbld, 2, "z", 1, "", 0));
      P12STEP(rosidl_dynamic_typesupport_dynamic_type_builder_add_float64_member(
          &qbld, 3, "w", 1, "", 0));
      P12STEP(rosidl_dynamic_typesupport_dynamic_type_init_from_dynamic_type_builder(
          &qbld, &alloc, &qtype));
      P12STEP(rosidl_dynamic_typesupport_dynamic_type_builder_init(
          &ss, "geometry_msgs::msg::Pose", 24, &alloc, &posebld));
      P12STEP(rosidl_dynamic_typesupport_dynamic_type_builder_add_complex_member(
          &posebld, 0, "position", 8, "", 0, &ptype));
      P12STEP(rosidl_dynamic_typesupport_dynamic_type_builder_add_complex_member(
          &posebld, 1, "orientation", 11, "", 0, &qtype));
      P12STEP(rosidl_dynamic_typesupport_dynamic_type_init_from_dynamic_type_builder(
          &posebld, &alloc, &posetype));
      P12STEP(rosidl_dynamic_typesupport_dynamic_data_init_from_dynamic_type(
          &posetype, &alloc, &data));
      {
        rcutils_uint8_array_t buf = pcdr;
        P12STEP(rosidl_dynamic_typesupport_dynamic_data_deserialize(&data, &buf));
      }
      double p[3] = {0, 0, 0};
      double q[4] = {0, 0, 0, 0};
      rosidl_dynamic_typesupport_dynamic_data_t lp =
        rosidl_dynamic_typesupport_get_zero_initialized_dynamic_data();
      P12STEP(rosidl_dynamic_typesupport_dynamic_data_loan_value(&data, 0, &alloc, &lp));
      for (size_t i = 0; i < 3 && rc == RCUTILS_RET_OK; ++i) {
        P12STEP(rosidl_dynamic_typesupport_dynamic_data_get_float64_value(&lp, i, &p[i]));
      }
      P12STEP(rosidl_dynamic_typesupport_dynamic_data_return_loaned_value(&data, &lp));
      rosidl_dynamic_typesupport_dynamic_data_t lo =
        rosidl_dynamic_typesupport_get_zero_initialized_dynamic_data();
      P12STEP(rosidl_dynamic_typesupport_dynamic_data_loan_value(&data, 1, &alloc, &lo));
      for (size_t i = 0; i < 4 && rc == RCUTILS_RET_OK; ++i) {
        P12STEP(rosidl_dynamic_typesupport_dynamic_data_get_float64_value(&lo, i, &q[i]));
      }
      P12STEP(rosidl_dynamic_typesupport_dynamic_data_return_loaned_value(&data, &lo));
#undef P12STEP
      std::printf(
        "[phase12 nst-vt] rc=%d position=[%g %g %g] orientation=[%g %g %g %g] "
        "(expect 1.5 2.5 3.5 / 0.125 0.25 0.375 0.5)\n",
        static_cast<int>(rc), p[0], p[1], p[2], q[0], q[1], q[2], q[3]);
      phase12_ok = (rc == RCUTILS_RET_OK && p[0] == 1.5 && p[1] == 2.5 && p[2] == 3.5 &&
        q[0] == 0.125 && q[1] == 0.25 && q[2] == 0.375 && q[3] == 0.5);
    } else {
      std::printf("[phase12 nst-vt] FAIL: rmw_serialization_support_init\n");
    }
  }
#else
  std::printf("[phase12 nst-vt] SKIP: geometry_msgs/Pose unavailable\n");
#endif
  *p12 = phase12_ok;
}

// Phase 13: boundary investigation (direct FFI). (a) Dumps the CDR encapsulation header of
// a real message to confirm the wire encoding (FINAL/plain vs XCDR2 with headers), which
// the flatten strategy relies on. (b) Confirms that a sequence of nested structs cannot be
// read via the flat path: add_sequence_of_named_field references the element by hash, and
// the participant-free decoder resolves it to an opaque ExternalType, so element reads fail
// (rather than returning wrong data). PASS = the read fails, i.e. the limitation is honest.
void run_edge_phase(bool * p13);

void run_edge_phase(bool * p13)
{
  bool phase13_ok = true;
#ifdef HAVE_POLYGON
  phase13_ok = false;
  {
    geometry_msgs::msg::Polygon poly;
    geometry_msgs::msg::Point32 pt;
    pt.x = 1.0f;
    pt.y = 2.0f;
    pt.z = 3.0f;
    poly.points.push_back(pt);
    pt.x = 4.0f;
    pt.y = 5.0f;
    pt.z = 6.0f;
    poly.points.push_back(pt);
    rclcpp::Serialization<geometry_msgs::msg::Polygon> gser;
    rclcpp::SerializedMessage gserialized;
    gser.serialize_message(&poly, &gserialized);
    const rcl_serialized_message_t & gcdr = gserialized.get_rcl_serialized_message();

    unsigned h0 = gcdr.buffer_length > 0 ? gcdr.buffer[0] : 0;
    unsigned h1 = gcdr.buffer_length > 1 ? gcdr.buffer[1] : 0;
    unsigned h2 = gcdr.buffer_length > 2 ? gcdr.buffer[2] : 0;
    unsigned h3 = gcdr.buffer_length > 3 ? gcdr.buffer[3] : 0;

    Int2DdsTypeInfo * ti = nullptr;
    Int2DdsTypeObject * obj = nullptr;
    const bool built =
      int2dds_type_info_create("geometry_msgs::msg::Polygon", 0, &ti) == INT2DDS_RET_OK &&
      int2dds_type_info_add_sequence_of_named_field(
        ti, "points", "geometry_msgs::msg::Point32", 0, 0) == INT2DDS_RET_OK &&
      int2dds_type_info_to_type_object(ti, &obj) == INT2DDS_RET_OK;
    Int2DdsRet r = INT2DDS_RET_ERROR;
    float x = -1.0f;
    if (built) {
      r = int2dds_dynamic_sample_get_f32(gcdr.buffer, gcdr.buffer_length, obj, "points[0].x", &x);
    }
    std::printf(
      "[phase13 edge]   encap=[%02x %02x %02x %02x] arr-of-nested points[0].x ret=%d x=%g "
      "(expect read failure: element struct is opaque without a type registry)\n",
      h0, h1, h2, h3, static_cast<int>(r), static_cast<double>(x));
    // Honest-limitation guard: the read must fail rather than yield a bogus value.
    phase13_ok = (r != INT2DDS_RET_OK);
    if (obj) {int2dds_type_object_destroy(obj);}
    if (ti) {int2dds_type_info_destroy(ti);}
  }
#else
  std::printf("[phase13 edge]   SKIP: geometry_msgs/Polygon unavailable\n");
#endif
  *p13 = phase13_ok;
}

int main()
{
  // Serialize std_msgs/Int32{data = 42} with the real (int2dds) serializer.
  std_msgs::msg::Int32 msg;
  msg.data = 42;
  rclcpp::Serialization<std_msgs::msg::Int32> serializer;
  rclcpp::SerializedMessage serialized;
  serializer.serialize_message(&msg, &serialized);
  const rcl_serialized_message_t & cdr = serialized.get_rcl_serialized_message();
  std::printf("serialized %zu bytes\n", cdr.buffer_length);

  bool phase1_ok = false;
  bool phase2_ok = false;

  // ---- Phase 1: int2dds FFI directly --------------------------------------
  {
    Int2DdsTypeInfo * info = nullptr;
    Int2DdsTypeObject * obj = nullptr;
    if (int2dds_type_info_create("std_msgs::msg::Int32", 0, &info) == INT2DDS_RET_OK &&
      int2dds_type_info_add_field(info, "data", FIELD_INT32, 0) == INT2DDS_RET_OK &&
      int2dds_type_info_to_type_object(info, &obj) == INT2DDS_RET_OK)
    {
      int32_t v = 0;
      Int2DdsRet r = int2dds_dynamic_sample_get_i32(cdr.buffer, cdr.buffer_length, obj, "data", &v);
      std::printf("[phase1 FFI]     ret=%d val=%d (expect 42)\n", static_cast<int>(r), v);
      phase1_ok = (r == INT2DDS_RET_OK && v == 42);
    } else {
      std::printf("[phase1 FFI]     FAIL: type build\n");
    }
    if (obj) {int2dds_type_object_destroy(obj);}
    if (info) {int2dds_type_info_destroy(info);}
  }

  // ---- Phase 2: through this rmw's serialization-support vtable ------------
  {
    rcutils_allocator_t alloc = rcutils_get_default_allocator();
    rosidl_dynamic_typesupport_serialization_support_t ss =
      rosidl_dynamic_typesupport_get_zero_initialized_serialization_support();
    rmw_ret_t rr = rmw_serialization_support_init("", &alloc, &ss);
    if (rr != RMW_RET_OK) {
      std::printf("[phase2 vtable]  FAIL: rmw_serialization_support_init ret=%d\n",
        static_cast<int>(rr));
    } else {
      rosidl_dynamic_typesupport_dynamic_type_builder_t builder =
        rosidl_dynamic_typesupport_get_zero_initialized_dynamic_type_builder();
      rosidl_dynamic_typesupport_dynamic_type_t type =
        rosidl_dynamic_typesupport_get_zero_initialized_dynamic_type();
      rosidl_dynamic_typesupport_dynamic_data_t data =
        rosidl_dynamic_typesupport_get_zero_initialized_dynamic_data();

      rcutils_ret_t rc = rosidl_dynamic_typesupport_dynamic_type_builder_init(
        &ss, "std_msgs::msg::Int32", 20, &alloc, &builder);
      if (rc == RCUTILS_RET_OK) {
        rc = rosidl_dynamic_typesupport_dynamic_type_builder_add_int32_member(
          &builder, 0, "data", 4, "", 0);
      }
      if (rc == RCUTILS_RET_OK) {
        rc = rosidl_dynamic_typesupport_dynamic_type_init_from_dynamic_type_builder(
          &builder, &alloc, &type);
      }
      if (rc == RCUTILS_RET_OK) {
        rc = rosidl_dynamic_typesupport_dynamic_data_init_from_dynamic_type(&type, &alloc, &data);
      }
      if (rc == RCUTILS_RET_OK) {
        rcutils_uint8_array_t buf = cdr;  // shares buffer; deserialize only reads it
        rc = rosidl_dynamic_typesupport_dynamic_data_deserialize(&data, &buf);
      }
      int32_t out = 0;
      if (rc == RCUTILS_RET_OK) {
        rc = rosidl_dynamic_typesupport_dynamic_data_get_int32_value(&data, 0, &out);
      }
      std::printf("[phase2 vtable]  rc=%d val=%d (expect 42)\n", static_cast<int>(rc), out);
      phase2_ok = (rc == RCUTILS_RET_OK && out == 42);
    }
  }

  // ---- Phase 3: multi-field / float64 through the vtable (member-id mapping) ----
  bool phase3_ok = true;  // stays true if the message type is unavailable
#ifdef HAVE_VECTOR3
  phase3_ok = false;
  {
    geometry_msgs::msg::Vector3 vec;
    vec.x = 1.5;
    vec.y = 2.5;
    vec.z = 3.5;
    rclcpp::Serialization<geometry_msgs::msg::Vector3> vser;
    rclcpp::SerializedMessage vserialized;
    vser.serialize_message(&vec, &vserialized);
    const rcl_serialized_message_t & vcdr = vserialized.get_rcl_serialized_message();
    std::printf("vec3 serialized %zu bytes\n", vcdr.buffer_length);

    // Isolation: direct FFI read on the same vec3 CDR. If this yields 1.5 but the
    // vtable path below fails, the bug is in our vtable (member mapping), not the FFI.
    {
      Int2DdsTypeInfo * di = nullptr;
      Int2DdsTypeObject * dobj = nullptr;
      int2dds_type_info_create("geometry_msgs::msg::Vector3", 0, &di);
      int2dds_type_info_add_field(di, "x", FIELD_FLOAT64, 0);
      int2dds_type_info_add_field(di, "y", FIELD_FLOAT64, 0);
      int2dds_type_info_add_field(di, "z", FIELD_FLOAT64, 0);
      int2dds_type_info_to_type_object(di, &dobj);
      double dx = 0.0;
      double dz = 0.0;
      Int2DdsRet drx =
        int2dds_dynamic_sample_get_f64(vcdr.buffer, vcdr.buffer_length, dobj, "x", &dx);
      Int2DdsRet drz =
        int2dds_dynamic_sample_get_f64(vcdr.buffer, vcdr.buffer_length, dobj, "z", &dz);
      std::printf(
        "[phase3 direct]  retx=%d x=%g | retz=%d z=%g (expect x=1.5 z=3.5)\n",
        static_cast<int>(drx), dx, static_cast<int>(drz), dz);
      if (dobj) {int2dds_type_object_destroy(dobj);}
      if (di) {int2dds_type_info_destroy(di);}
    }

    rcutils_allocator_t alloc = rcutils_get_default_allocator();
    rosidl_dynamic_typesupport_serialization_support_t ss =
      rosidl_dynamic_typesupport_get_zero_initialized_serialization_support();
    rmw_ret_t rr = rmw_serialization_support_init("", &alloc, &ss);
    if (rr == RMW_RET_OK) {
      rosidl_dynamic_typesupport_dynamic_type_builder_t builder =
        rosidl_dynamic_typesupport_get_zero_initialized_dynamic_type_builder();
      rosidl_dynamic_typesupport_dynamic_type_t type =
        rosidl_dynamic_typesupport_get_zero_initialized_dynamic_type();
      rosidl_dynamic_typesupport_dynamic_data_t data =
        rosidl_dynamic_typesupport_get_zero_initialized_dynamic_data();

      const char * step = "none";
      rcutils_ret_t rc = RCUTILS_RET_OK;
      double x = 0.0;
      double y = 0.0;
      double z = 0.0;
#define P3STEP(lbl, call) \
  do {if (rc == RCUTILS_RET_OK) {step = lbl; rc = (call);}} while (0)
      P3STEP("builder_init", rosidl_dynamic_typesupport_dynamic_type_builder_init(
          &ss, "geometry_msgs::msg::Vector3", 26, &alloc, &builder));
      P3STEP("add_x", rosidl_dynamic_typesupport_dynamic_type_builder_add_float64_member(
          &builder, 0, "x", 1, "", 0));
      P3STEP("add_y", rosidl_dynamic_typesupport_dynamic_type_builder_add_float64_member(
          &builder, 1, "y", 1, "", 0));
      P3STEP("add_z", rosidl_dynamic_typesupport_dynamic_type_builder_add_float64_member(
          &builder, 2, "z", 1, "", 0));
      P3STEP("type_init", rosidl_dynamic_typesupport_dynamic_type_init_from_dynamic_type_builder(
          &builder, &alloc, &type));
      P3STEP("data_init", rosidl_dynamic_typesupport_dynamic_data_init_from_dynamic_type(
          &type, &alloc, &data));
      {
        rcutils_uint8_array_t buf = vcdr;
        P3STEP("deserialize", rosidl_dynamic_typesupport_dynamic_data_deserialize(&data, &buf));
      }
      P3STEP("get_x", rosidl_dynamic_typesupport_dynamic_data_get_float64_value(&data, 0, &x));
      P3STEP("get_y", rosidl_dynamic_typesupport_dynamic_data_get_float64_value(&data, 1, &y));
      P3STEP("get_z", rosidl_dynamic_typesupport_dynamic_data_get_float64_value(&data, 2, &z));
#undef P3STEP
      if (rc != RCUTILS_RET_OK) {
        std::printf("[phase3 vec3]    FAILED at step '%s' rc=%d\n", step, static_cast<int>(rc));
      }
      std::printf(
        "[phase3 vec3]    rc=%d x=%g y=%g z=%g (expect 1.5 2.5 3.5)\n",
        static_cast<int>(rc), x, y, z);
      phase3_ok = (rc == RCUTILS_RET_OK && x == 1.5 && y == 2.5 && z == 3.5);
    } else {
      std::printf(
        "[phase3 vec3]    FAIL: rmw_serialization_support_init ret=%d\n", static_cast<int>(rr));
    }
  }
#else
  std::printf("[phase3 vec3]    SKIP: geometry_msgs unavailable\n");
#endif

  // ---- Phase 4: every primitive type through the vtable (test_msgs/BasicTypes) ----
  bool phase4_ok = true;
#ifdef HAVE_BASICTYPES
  phase4_ok = false;
  {
    test_msgs::msg::BasicTypes bt;
    bt.bool_value = true;
    bt.byte_value = 7;
    bt.char_value = 65;
    bt.float32_value = 1.5f;
    bt.float64_value = 2.5;
    bt.int8_value = -8;
    bt.uint8_value = 8;
    bt.int16_value = -16;
    bt.uint16_value = 16;
    bt.int32_value = -32;
    bt.uint32_value = 32;
    bt.int64_value = -64;
    bt.uint64_value = 64;
    rclcpp::Serialization<test_msgs::msg::BasicTypes> bser;
    rclcpp::SerializedMessage bserialized;
    bser.serialize_message(&bt, &bserialized);
    const rcl_serialized_message_t & bcdr = bserialized.get_rcl_serialized_message();
    std::printf("basictypes serialized %zu bytes\n", bcdr.buffer_length);

    rcutils_allocator_t alloc = rcutils_get_default_allocator();
    rosidl_dynamic_typesupport_serialization_support_t ss =
      rosidl_dynamic_typesupport_get_zero_initialized_serialization_support();
    rmw_ret_t rr = rmw_serialization_support_init("", &alloc, &ss);
    if (rr == RMW_RET_OK) {
      rosidl_dynamic_typesupport_dynamic_type_builder_t bld =
        rosidl_dynamic_typesupport_get_zero_initialized_dynamic_type_builder();
      rosidl_dynamic_typesupport_dynamic_type_t type =
        rosidl_dynamic_typesupport_get_zero_initialized_dynamic_type();
      rosidl_dynamic_typesupport_dynamic_data_t data =
        rosidl_dynamic_typesupport_get_zero_initialized_dynamic_data();
      const char * step = "none";
      rcutils_ret_t rc = RCUTILS_RET_OK;
#define P4STEP(lbl, call) do {if (rc == RCUTILS_RET_OK) {step = lbl; rc = (call);}} while (0)
      P4STEP("builder", rosidl_dynamic_typesupport_dynamic_type_builder_init(
          &ss, "test_msgs::msg::BasicTypes", 25, &alloc, &bld));
      P4STEP("add_bool", rosidl_dynamic_typesupport_dynamic_type_builder_add_bool_member(
          &bld, 0, "bool_value", 10, "", 0));
      P4STEP("add_byte", rosidl_dynamic_typesupport_dynamic_type_builder_add_byte_member(
          &bld, 1, "byte_value", 10, "", 0));
      P4STEP("add_char", rosidl_dynamic_typesupport_dynamic_type_builder_add_char_member(
          &bld, 2, "char_value", 10, "", 0));
      P4STEP("add_f32", rosidl_dynamic_typesupport_dynamic_type_builder_add_float32_member(
          &bld, 3, "float32_value", 13, "", 0));
      P4STEP("add_f64", rosidl_dynamic_typesupport_dynamic_type_builder_add_float64_member(
          &bld, 4, "float64_value", 13, "", 0));
      P4STEP("add_i8", rosidl_dynamic_typesupport_dynamic_type_builder_add_int8_member(
          &bld, 5, "int8_value", 10, "", 0));
      P4STEP("add_u8", rosidl_dynamic_typesupport_dynamic_type_builder_add_uint8_member(
          &bld, 6, "uint8_value", 11, "", 0));
      P4STEP("add_i16", rosidl_dynamic_typesupport_dynamic_type_builder_add_int16_member(
          &bld, 7, "int16_value", 11, "", 0));
      P4STEP("add_u16", rosidl_dynamic_typesupport_dynamic_type_builder_add_uint16_member(
          &bld, 8, "uint16_value", 12, "", 0));
      P4STEP("add_i32", rosidl_dynamic_typesupport_dynamic_type_builder_add_int32_member(
          &bld, 9, "int32_value", 11, "", 0));
      P4STEP("add_u32", rosidl_dynamic_typesupport_dynamic_type_builder_add_uint32_member(
          &bld, 10, "uint32_value", 12, "", 0));
      P4STEP("add_i64", rosidl_dynamic_typesupport_dynamic_type_builder_add_int64_member(
          &bld, 11, "int64_value", 11, "", 0));
      P4STEP("add_u64", rosidl_dynamic_typesupport_dynamic_type_builder_add_uint64_member(
          &bld, 12, "uint64_value", 12, "", 0));
      P4STEP("type_init", rosidl_dynamic_typesupport_dynamic_type_init_from_dynamic_type_builder(
          &bld, &alloc, &type));
      P4STEP("data_init", rosidl_dynamic_typesupport_dynamic_data_init_from_dynamic_type(
          &type, &alloc, &data));
      {
        rcutils_uint8_array_t buf = bcdr;
        P4STEP("deserialize", rosidl_dynamic_typesupport_dynamic_data_deserialize(&data, &buf));
      }
      bool b = false;
      uint8_t by = 0;
      char c = 0;
      float f32 = 0.0f;
      double f64 = 0.0;
      int8_t i8 = 0;
      uint8_t u8 = 0;
      int16_t i16 = 0;
      uint16_t u16 = 0;
      int32_t i32 = 0;
      uint32_t u32 = 0;
      int64_t i64 = 0;
      uint64_t u64 = 0;
      P4STEP("get_bool", rosidl_dynamic_typesupport_dynamic_data_get_bool_value(&data, 0, &b));
      P4STEP("get_byte", rosidl_dynamic_typesupport_dynamic_data_get_byte_value(&data, 1, &by));
      P4STEP("get_char", rosidl_dynamic_typesupport_dynamic_data_get_char_value(&data, 2, &c));
      P4STEP("get_f32", rosidl_dynamic_typesupport_dynamic_data_get_float32_value(&data, 3, &f32));
      P4STEP("get_f64", rosidl_dynamic_typesupport_dynamic_data_get_float64_value(&data, 4, &f64));
      P4STEP("get_i8", rosidl_dynamic_typesupport_dynamic_data_get_int8_value(&data, 5, &i8));
      P4STEP("get_u8", rosidl_dynamic_typesupport_dynamic_data_get_uint8_value(&data, 6, &u8));
      P4STEP("get_i16", rosidl_dynamic_typesupport_dynamic_data_get_int16_value(&data, 7, &i16));
      P4STEP("get_u16", rosidl_dynamic_typesupport_dynamic_data_get_uint16_value(&data, 8, &u16));
      P4STEP("get_i32", rosidl_dynamic_typesupport_dynamic_data_get_int32_value(&data, 9, &i32));
      P4STEP("get_u32", rosidl_dynamic_typesupport_dynamic_data_get_uint32_value(&data, 10, &u32));
      P4STEP("get_i64", rosidl_dynamic_typesupport_dynamic_data_get_int64_value(&data, 11, &i64));
      P4STEP("get_u64", rosidl_dynamic_typesupport_dynamic_data_get_uint64_value(&data, 12, &u64));
#undef P4STEP
      if (rc != RCUTILS_RET_OK) {
        std::printf("[phase4 basic]   FAILED at step '%s' rc=%d\n", step, static_cast<int>(rc));
      } else {
        const bool match = b && by == 7 && c == 65 && f32 == 1.5f && f64 == 2.5 &&
          i8 == -8 && u8 == 8 && i16 == -16 && u16 == 16 && i32 == -32 && u32 == 32u &&
          i64 == -64 && u64 == 64u;
        std::printf(
          "[phase4 basic]   bool=%d byte=%d char=%d f32=%g f64=%g i8=%d u8=%d "
          "i16=%d u16=%d i32=%d u32=%u i64=%" PRId64 " u64=%" PRIu64 " => %s\n",
          static_cast<int>(b), static_cast<int>(by), static_cast<int>(c),
          static_cast<double>(f32), f64, static_cast<int>(i8), static_cast<int>(u8),
          static_cast<int>(i16), static_cast<int>(u16), i32, u32,
          i64, u64,
          match ? "MATCH" : "MISMATCH");
        phase4_ok = match;
      }
    } else {
      std::printf(
        "[phase4 basic]   FAIL: rmw_serialization_support_init ret=%d\n", static_cast<int>(rr));
    }
  }
#else
  std::printf("[phase4 basic]   SKIP: test_msgs unavailable\n");
#endif

  // ---- Phase 5: string members through the vtable (diagnostic_msgs/KeyValue) ----
  // KeyValue = {string key; string value}. Reading `value` (id 1) also verifies the
  // decoder tracks the offset correctly past the variable-length `key`.
  bool phase5_ok = true;
#ifdef HAVE_KEYVALUE
  phase5_ok = false;
  {
    auto run_keyvalue = [](
      const std::string & key_in, const std::string & value_in,
      std::string & key_out, std::string & value_out) -> rcutils_ret_t {
        diagnostic_msgs::msg::KeyValue kv;
        kv.key = key_in;
        kv.value = value_in;
        rclcpp::Serialization<diagnostic_msgs::msg::KeyValue> kser;
        rclcpp::SerializedMessage kserialized;
        kser.serialize_message(&kv, &kserialized);
        const rcl_serialized_message_t & kcdr = kserialized.get_rcl_serialized_message();

        rcutils_allocator_t alloc = rcutils_get_default_allocator();
        rosidl_dynamic_typesupport_serialization_support_t ss =
          rosidl_dynamic_typesupport_get_zero_initialized_serialization_support();
        if (rmw_serialization_support_init("", &alloc, &ss) != RMW_RET_OK) {
          return RCUTILS_RET_ERROR;
        }
        rosidl_dynamic_typesupport_dynamic_type_builder_t bld =
          rosidl_dynamic_typesupport_get_zero_initialized_dynamic_type_builder();
        rosidl_dynamic_typesupport_dynamic_type_t type =
          rosidl_dynamic_typesupport_get_zero_initialized_dynamic_type();
        rosidl_dynamic_typesupport_dynamic_data_t data =
          rosidl_dynamic_typesupport_get_zero_initialized_dynamic_data();
        rcutils_ret_t rc = RCUTILS_RET_OK;
#define P5STEP(call) do {if (rc == RCUTILS_RET_OK) {rc = (call);}} while (0)
        P5STEP(rosidl_dynamic_typesupport_dynamic_type_builder_init(
            &ss, "diagnostic_msgs::msg::KeyValue", 30, &alloc, &bld));
        P5STEP(rosidl_dynamic_typesupport_dynamic_type_builder_add_string_member(
            &bld, 0, "key", 3, "", 0));
        P5STEP(rosidl_dynamic_typesupport_dynamic_type_builder_add_string_member(
            &bld, 1, "value", 5, "", 0));
        P5STEP(rosidl_dynamic_typesupport_dynamic_type_init_from_dynamic_type_builder(
            &bld, &alloc, &type));
        P5STEP(rosidl_dynamic_typesupport_dynamic_data_init_from_dynamic_type(
            &type, &alloc, &data));
        {
          rcutils_uint8_array_t buf = kcdr;
          P5STEP(rosidl_dynamic_typesupport_dynamic_data_deserialize(&data, &buf));
        }
        char * ks = nullptr;
        size_t klen = 0;
        char * vs = nullptr;
        size_t vlen = 0;
        P5STEP(rosidl_dynamic_typesupport_dynamic_data_get_string_value(&data, 0, &ks, &klen));
        P5STEP(rosidl_dynamic_typesupport_dynamic_data_get_string_value(&data, 1, &vs, &vlen));
#undef P5STEP
        if (rc == RCUTILS_RET_OK) {
          key_out.assign(ks, klen);
          value_out.assign(vs, vlen);
        }
        delete[] ks;  // owned by caller (new char[] in the vtable getter)
        delete[] vs;
        return rc;
      };

    std::string k1;
    std::string v1;
    rcutils_ret_t rc1 = run_keyvalue("dyn-key", "hello-value-123", k1, v1);
    std::string k2;
    std::string v2;
    rcutils_ret_t rc2 = run_keyvalue("", "tail", k2, v2);  // empty key -> probe-OK path
    std::printf(
      "[phase5 string]  rc1=%d key='%s' value='%s' | rc2=%d key='%s' value='%s' (expect "
      "'dyn-key'/'hello-value-123' and ''/'tail')\n",
      static_cast<int>(rc1), k1.c_str(), v1.c_str(),
      static_cast<int>(rc2), k2.c_str(), v2.c_str());
    phase5_ok = (rc1 == RCUTILS_RET_OK && k1 == "dyn-key" && v1 == "hello-value-123" &&
      rc2 == RCUTILS_RET_OK && k2.empty() && v2 == "tail");
  }
#else
  std::printf("[phase5 string]  SKIP: diagnostic_msgs unavailable\n");
#endif

  // ---- Phase 6: wstring builds but reads are rejected gracefully (test_msgs/WStrings) --
  // The int2dds FFI has no flat-sample wstring reader, so get_wstring_value must fail
  // (not crash, not return a bogus value). Passing = the type builds and the read errors.
  bool phase6_ok = true;
#ifdef HAVE_WSTRINGS
  phase6_ok = false;
  {
    test_msgs::msg::WStrings ws;
    ws.wstring_value = u"hello";
    rclcpp::Serialization<test_msgs::msg::WStrings> wser;
    rclcpp::SerializedMessage wserialized;
    wser.serialize_message(&ws, &wserialized);
    const rcl_serialized_message_t & wcdr = wserialized.get_rcl_serialized_message();

    rcutils_allocator_t alloc = rcutils_get_default_allocator();
    rosidl_dynamic_typesupport_serialization_support_t ss =
      rosidl_dynamic_typesupport_get_zero_initialized_serialization_support();
    if (rmw_serialization_support_init("", &alloc, &ss) == RMW_RET_OK) {
      rosidl_dynamic_typesupport_dynamic_type_builder_t bld =
        rosidl_dynamic_typesupport_get_zero_initialized_dynamic_type_builder();
      rosidl_dynamic_typesupport_dynamic_type_t type =
        rosidl_dynamic_typesupport_get_zero_initialized_dynamic_type();
      rosidl_dynamic_typesupport_dynamic_data_t data =
        rosidl_dynamic_typesupport_get_zero_initialized_dynamic_data();
      rcutils_ret_t rc = RCUTILS_RET_OK;
#define P6STEP(call) do {if (rc == RCUTILS_RET_OK) {rc = (call);}} while (0)
      P6STEP(rosidl_dynamic_typesupport_dynamic_type_builder_init(
          &ss, "test_msgs::msg::WStrings", 24, &alloc, &bld));
      P6STEP(rosidl_dynamic_typesupport_dynamic_type_builder_add_wstring_member(
          &bld, 0, "wstring_value", 13, "", 0));
      P6STEP(rosidl_dynamic_typesupport_dynamic_type_init_from_dynamic_type_builder(
          &bld, &alloc, &type));
      P6STEP(rosidl_dynamic_typesupport_dynamic_data_init_from_dynamic_type(
          &type, &alloc, &data));
      {
        rcutils_uint8_array_t buf = wcdr;
        P6STEP(rosidl_dynamic_typesupport_dynamic_data_deserialize(&data, &buf));
      }
#undef P6STEP
      const bool built = (rc == RCUTILS_RET_OK);
      char16_t * out = nullptr;
      size_t outlen = 0;
      rcutils_ret_t rc_get =
        rosidl_dynamic_typesupport_dynamic_data_get_wstring_value(&data, 0, &out, &outlen);
      std::printf(
        "[phase6 wstring] built=%d get_wstring rc=%d (expect build ok, read rejected)\n",
        static_cast<int>(built), static_cast<int>(rc_get));
      phase6_ok = (built && rc_get != RCUTILS_RET_OK);
    } else {
      std::printf("[phase6 wstring] FAIL: rmw_serialization_support_init\n");
    }
  }
#else
  std::printf("[phase6 wstring] SKIP: test_msgs WStrings unavailable\n");
#endif

  bool phase7_ok = true;
  bool phase8_ok = true;
  bool phase9_ok = true;
  bool phase10_ok = true;
  run_collection_phases(&phase7_ok, &phase8_ok, &phase9_ok, &phase10_ok);

  bool phase11_ok = true;
  run_nested_phase(&phase11_ok);

  bool phase12_ok = true;
  run_nested_vtable_phase(&phase12_ok);

  bool phase13_ok = true;
  run_edge_phase(&phase13_ok);

  const bool ok =
    phase1_ok && phase2_ok && phase3_ok && phase4_ok && phase5_ok && phase6_ok &&
    phase7_ok && phase8_ok && phase9_ok && phase10_ok && phase11_ok && phase12_ok &&
    phase13_ok;
  std::printf(
    "RESULT: phase1(FFI)=%s  phase2(int32)=%s  phase3(vec3)=%s  phase4(all-prims)=%s  "
    "phase5(string)=%s  phase6(wstring)=%s  phase7(array)=%s  phase8(seq)=%s  "
    "phase9(arr-vt)=%s  phase10(seq-vt)=%s  phase11(nested)=%s  phase12(nst-vt)=%s  "
    "phase13(edge)=%s  => %s\n",
    phase1_ok ? "PASS" : "FAIL",
    phase2_ok ? "PASS" : "FAIL",
    phase3_ok ? "PASS" : "FAIL",
    phase4_ok ? "PASS" : "FAIL",
    phase5_ok ? "PASS" : "FAIL",
    phase6_ok ? "PASS" : "FAIL",
    phase7_ok ? "PASS" : "FAIL",
    phase8_ok ? "PASS" : "FAIL",
    phase9_ok ? "PASS" : "FAIL",
    phase10_ok ? "PASS" : "FAIL",
    phase11_ok ? "PASS" : "FAIL",
    phase12_ok ? "PASS" : "FAIL",
    phase13_ok ? "PASS" : "FAIL",
    ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
