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
// Dynamic message serialization support (C3). Bridges the rosidl_dynamic_typesupport
// interface onto the int2dds FFI dynamic type/data API.
//
// Scope (receive path): primitive- and string-typed struct members, arrays and sequences
// of those, and nested struct members (recursively). Reads use the int2dds flat-sample
// accessors, which resolve dotted/indexed paths against a decoded sample.
//
// Nested structs are handled by flattening: for FINAL extensibility a nested struct is
// encoded inline with no member header, so add_complex_member re-adds the nested type's
// leaf fields into the parent type builder under generated names, in CDR order. The read
// side keeps a member tree; loan_value(id) yields a child view over the nested members (or
// the collection element index). The alternative -- int2dds_type_info_add_named_type_field
// -- cannot be used here: it stores only a type hash, and the participant-free flat decoder
// resolves that hash to an opaque ExternalType (dynamic_type.rs resolve_nested, ctx None),
// which loses the nested layout.
//
// Not supported: arrays/sequences of nested structs (cannot be flattened at a static
// count), wstring reads (no flat-sample wstring reader in the FFI), and non-FINAL nested
// structs (their DHEADERs break the inline-flatten assumption). Those slots fail gracefully
// or are left null.

#if __has_include("rosidl_dynamic_typesupport/api/serialization_support_interface.h")

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <new>
#include <string>
#include <vector>

#include "rcutils/allocator.h"
#include "rcutils/error_handling.h"
#include "rcutils/types/rcutils_ret.h"
#include "rcutils/types/uint8_array.h"

// The vendored FFI header declares its macro-generated dynamic accessors (the
// int2dds_dynamic_sample_get_* / int2dds_dynamic_data_get_* families) after the
// close of its own extern "C" block, so under C++ they would receive C++ linkage
// and fail to resolve against the C symbols in libint2dds_ffi.so. Wrap the include
// so every declaration gets C linkage.
extern "C"
{
#include "int2dds-ffi.h"  // NOLINT(build/include_subdir): vendored FFI header
}

#include "rosidl_dynamic_typesupport/api/serialization_support.h"
#include "rosidl_dynamic_typesupport/api/serialization_support_interface.h"
#include "rosidl_dynamic_typesupport/types.h"

#include "rmw_int2dds_cpp/dynamic_serialization_support.hpp"

namespace
{

// Field type codes are the INT2DDS_FIELD_* constants from int2dds-ffi.h (consumed by
// int2dds_type_info_add_field -> field_type_to_type_identifier); NOT the XTypes TypeKind
// values, which differ and would build the wrong field type.

const char * const kLibraryIdentifier = "rmw_int2dds_dynamic";

enum MemberKind
{
  MEMBER_LEAF,        // scalar or string
  MEMBER_COLLECTION,  // array or sequence of a primitive/string element
  MEMBER_NESTED       // nested struct (flattened)
};

// A struct member. LEAF/COLLECTION members carry the int2dds field name they were flattened
// to (flat_name); NESTED members carry their sub-member subtree. The build fields
// (type_code/is_array/extent) let a nested member's leaves be re-added to a parent builder.
struct Member
{
  std::string name;              // ROS member name
  MemberKind kind = MEMBER_LEAF;
  int type_code = 0;             // LEAF: scalar/string INT2DDS_FIELD_*; COLLECTION: element code
  bool is_array = false;         // COLLECTION: array vs sequence
  uint32_t extent = 0;           // COLLECTION: array size, or sequence bound (0 = unbounded)
  std::string flat_name;         // LEAF/COLLECTION: field name in the (flat) int2dds type
  std::vector<Member> sub;       // NESTED: sub-members
};

struct BuilderHandle
{
  Int2DdsTypeInfo * info = nullptr;
  std::string name;
  std::vector<Member> members;
  uint32_t flat_counter = 0;     // generates unique flattened field names
};

struct TypeHandle
{
  Int2DdsTypeObject * obj = nullptr;
  std::string name;
  std::vector<Member> members;
};

// A view over a decoded sample. The root/nested-struct view exposes `members`; a view
// loaned for a collection member exposes `flat_name` + `element_code`. bytes and the member
// tree are shared/borrowed so loaned children stay valid while the parent lives.
struct DataHandle
{
  std::shared_ptr<std::vector<uint8_t>> bytes;
  const Int2DdsTypeObject * obj = nullptr;
  std::vector<Member> members_owned;           // populated only for the root view
  // Struct view members: root points to members_owned; a nested child borrows the parent tree.
  const std::vector<Member> * members = nullptr;
  bool is_collection = false;
  std::string flat_name;                       // collection view: the collection's field name
  int element_code = 0;                        // collection view: element INT2DDS_FIELD_*
};

BuilderHandle * builder_of(rosidl_dynamic_typesupport_dynamic_type_builder_impl_t * h)
{
  return static_cast<BuilderHandle *>(h->handle);
}
TypeHandle * type_of(rosidl_dynamic_typesupport_dynamic_type_impl_t * h)
{
  return static_cast<TypeHandle *>(h->handle);
}
DataHandle * data_of(rosidl_dynamic_typesupport_dynamic_data_impl_t * h)
{
  return static_cast<DataHandle *>(h->handle);
}
const DataHandle * data_of(const rosidl_dynamic_typesupport_dynamic_data_impl_t * h)
{
  return static_cast<const DataHandle *>(h->handle);
}

// Build the flat-sample path for member/element `id` in view `d`. Returns false when a
// struct id is out of range or names a member that must be loaned (nested/collection).
bool resolve_path(
  const DataHandle * d, rosidl_dynamic_typesupport_member_id_t id, std::string & out)
{
  if (d->is_collection) {
    out = d->flat_name + "[" + std::to_string(id) + "]";
    return true;
  }
  if (!d->members || static_cast<size_t>(id) >= d->members->size()) {
    return false;
  }
  const Member & m = (*d->members)[id];
  if (m.kind != MEMBER_LEAF) {
    return false;
  }
  out = m.flat_name;
  return true;
}

// Return code of reading the element at `path` as `code`, discarding the value. Used to
// probe a collection's length: a valid index returns OK, past-the-end returns
// DYNAMIC_FIELD_NOT_FOUND.
Int2DdsRet probe_path(const DataHandle * d, const std::string & path, int code)
{
  const uint8_t * b = d->bytes->data();
  size_t n = d->bytes->size();
  const char * p = path.c_str();
#define PROBE_CASE(CODE, CTYPE, GETTER) \
  case CODE: {CTYPE v; return GETTER(b, n, d->obj, p, &v);}
  switch (code) {
    PROBE_CASE(INT2DDS_FIELD_BOOL, bool, int2dds_dynamic_sample_get_bool)
    PROBE_CASE(INT2DDS_FIELD_BYTE, uint8_t, int2dds_dynamic_sample_get_byte)
    PROBE_CASE(INT2DDS_FIELD_CHAR8, uint8_t, int2dds_dynamic_sample_get_char8)
    PROBE_CASE(INT2DDS_FIELD_INT8, int8_t, int2dds_dynamic_sample_get_i8)
    PROBE_CASE(INT2DDS_FIELD_UINT8, uint8_t, int2dds_dynamic_sample_get_u8)
    PROBE_CASE(INT2DDS_FIELD_INT16, int16_t, int2dds_dynamic_sample_get_i16)
    PROBE_CASE(INT2DDS_FIELD_UINT16, uint16_t, int2dds_dynamic_sample_get_u16)
    PROBE_CASE(INT2DDS_FIELD_INT32, int32_t, int2dds_dynamic_sample_get_i32)
    PROBE_CASE(INT2DDS_FIELD_UINT32, uint32_t, int2dds_dynamic_sample_get_u32)
    PROBE_CASE(INT2DDS_FIELD_INT64, int64_t, int2dds_dynamic_sample_get_i64)
    PROBE_CASE(INT2DDS_FIELD_UINT64, uint64_t, int2dds_dynamic_sample_get_u64)
    PROBE_CASE(INT2DDS_FIELD_FLOAT32, float, int2dds_dynamic_sample_get_f32)
    PROBE_CASE(INT2DDS_FIELD_FLOAT64, double, int2dds_dynamic_sample_get_f64)
    case INT2DDS_FIELD_STRING: {
        char probe[1] = {0};
        size_t needed = 0;
        Int2DdsRet r = int2dds_dynamic_sample_get_string(b, n, d->obj, p, probe, sizeof(probe),
            &needed);
        // A non-empty string overflows the 1-byte probe (DECODE_ERROR) but still exists.
        if (r == INT2DDS_RET_OK || needed > 0) {return INT2DDS_RET_OK;}
        return r;
      }
    default:
      return INT2DDS_RET_DYNAMIC_UNSUPPORTED_TYPE;
  }
#undef PROBE_CASE
}

// ===== Management =============================================================
rcutils_ret_t
ss_impl_fini(rosidl_dynamic_typesupport_serialization_support_impl_t * impl)
{
  (void)impl;
  return RCUTILS_RET_OK;
}

rcutils_ret_t
ss_interface_fini(rosidl_dynamic_typesupport_serialization_support_interface_t * methods)
{
  (void)methods;
  return RCUTILS_RET_OK;
}

// ===== Dynamic type builder ==================================================
rcutils_ret_t
dtb_init(
  rosidl_dynamic_typesupport_serialization_support_impl_t * ss,
  const char * name, size_t name_length,
  rcutils_allocator_t * allocator,
  rosidl_dynamic_typesupport_dynamic_type_builder_impl_t * out)
{
  (void)ss;
  (void)allocator;
  auto * b = new (std::nothrow) BuilderHandle();
  if (!b) {
    RCUTILS_SET_ERROR_MSG("failed to allocate int2dds dynamic type builder");
    return RCUTILS_RET_BAD_ALLOC;
  }
  b->name.assign(name, name_length);
  // extensibility 0 = FINAL (standard .msg types).
  if (int2dds_type_info_create(b->name.c_str(), 0, &b->info) != INT2DDS_RET_OK || !b->info) {
    delete b;
    RCUTILS_SET_ERROR_MSG("int2dds_type_info_create failed");
    return RCUTILS_RET_ERROR;
  }
  out->handle = b;
  return RCUTILS_RET_OK;
}

rcutils_ret_t
dtb_fini(
  rosidl_dynamic_typesupport_serialization_support_impl_t * ss,
  rosidl_dynamic_typesupport_dynamic_type_builder_impl_t * builder)
{
  (void)ss;
  auto * b = builder_of(builder);
  if (b) {
    if (b->info) {
      int2dds_type_info_destroy(b->info);
    }
    delete b;
    builder->handle = nullptr;
  }
  return RCUTILS_RET_OK;
}

rcutils_ret_t
dt_init_from_builder(
  rosidl_dynamic_typesupport_serialization_support_impl_t * ss,
  rosidl_dynamic_typesupport_dynamic_type_builder_impl_t * builder,
  rcutils_allocator_t * allocator,
  rosidl_dynamic_typesupport_dynamic_type_impl_t * out)
{
  (void)ss;
  (void)allocator;
  auto * b = builder_of(builder);
  if (!b || !b->info) {
    RCUTILS_SET_ERROR_MSG("invalid int2dds dynamic type builder");
    return RCUTILS_RET_ERROR;
  }
  auto * t = new (std::nothrow) TypeHandle();
  if (!t) {
    RCUTILS_SET_ERROR_MSG("failed to allocate int2dds dynamic type");
    return RCUTILS_RET_BAD_ALLOC;
  }
  if (int2dds_type_info_to_type_object(b->info, &t->obj) != INT2DDS_RET_OK || !t->obj) {
    delete t;
    RCUTILS_SET_ERROR_MSG("int2dds_type_info_to_type_object failed");
    return RCUTILS_RET_ERROR;
  }
  t->name = b->name;
  t->members = b->members;
  out->handle = t;
  return RCUTILS_RET_OK;
}

rcutils_ret_t
dt_fini(
  rosidl_dynamic_typesupport_serialization_support_impl_t * ss,
  rosidl_dynamic_typesupport_dynamic_type_impl_t * type)
{
  (void)ss;
  auto * t = type_of(type);
  if (t) {
    if (t->obj) {
      int2dds_type_object_destroy(t->obj);
    }
    delete t;
    type->handle = nullptr;
  }
  return RCUTILS_RET_OK;
}

// Type/builder introspection. A description-driven reader walks its own field list, so the
// vtable exposes only the top-level member count and the type name (there is no per-member
// kind slot in the interface). These read straight from the handles.
rcutils_ret_t
dt_get_member_count(
  rosidl_dynamic_typesupport_serialization_support_impl_t * ss,
  const rosidl_dynamic_typesupport_dynamic_type_impl_t * type,
  size_t * member_count)
{
  (void)ss;
  const auto * t = static_cast<const TypeHandle *>(type->handle);
  if (!t || !member_count) {
    RCUTILS_SET_ERROR_MSG("invalid int2dds dynamic type");
    return RCUTILS_RET_ERROR;
  }
  *member_count = t->members.size();
  return RCUTILS_RET_OK;
}

rcutils_ret_t
dt_get_name(
  rosidl_dynamic_typesupport_serialization_support_impl_t * ss,
  const rosidl_dynamic_typesupport_dynamic_type_impl_t * type,
  const char ** name, size_t * name_length)
{
  (void)ss;
  const auto * t = static_cast<const TypeHandle *>(type->handle);
  if (!t || !name || !name_length) {
    RCUTILS_SET_ERROR_MSG("invalid int2dds dynamic type");
    return RCUTILS_RET_ERROR;
  }
  *name = t->name.c_str();
  *name_length = t->name.size();
  return RCUTILS_RET_OK;
}

rcutils_ret_t
dtb_get_name(
  rosidl_dynamic_typesupport_serialization_support_impl_t * ss,
  const rosidl_dynamic_typesupport_dynamic_type_builder_impl_t * builder,
  const char ** name, size_t * name_length)
{
  (void)ss;
  const auto * b = static_cast<const BuilderHandle *>(builder->handle);
  if (!b || !name || !name_length) {
    RCUTILS_SET_ERROR_MSG("invalid int2dds dynamic type builder");
    return RCUTILS_RET_ERROR;
  }
  *name = b->name.c_str();
  *name_length = b->name.size();
  return RCUTILS_RET_OK;
}

rcutils_ret_t
dtb_set_name(
  rosidl_dynamic_typesupport_serialization_support_impl_t * ss,
  rosidl_dynamic_typesupport_dynamic_type_builder_impl_t * builder,
  const char * name, size_t name_length)
{
  (void)ss;
  auto * b = builder_of(builder);
  if (!b || !name) {
    RCUTILS_SET_ERROR_MSG("invalid int2dds dynamic type builder");
    return RCUTILS_RET_ERROR;
  }
  b->name.assign(name, name_length);
  return RCUTILS_RET_OK;
}

// Scalar/string member adders share a signature; only the field code differs. At the top
// level the flat name is the ROS name; when flattened into a parent it is regenerated.
#define DEFINE_ADD_SCALAR(FN, TK) \
  rcutils_ret_t FN( \
    rosidl_dynamic_typesupport_serialization_support_impl_t * ss, \
    rosidl_dynamic_typesupport_dynamic_type_builder_impl_t * builder, \
    rosidl_dynamic_typesupport_member_id_t id, \
    const char * name, size_t name_length, \
    const char * default_value, size_t default_value_length) \
  { \
    (void)ss; \
    (void)id; \
    (void)default_value; \
    (void)default_value_length; \
    auto * b = builder_of(builder); \
    if (!b || !b->info) { \
      RCUTILS_SET_ERROR_MSG("invalid int2dds dynamic type builder"); \
      return RCUTILS_RET_ERROR; \
    } \
    std::string field(name, name_length); \
    if (int2dds_type_info_add_field(b->info, field.c_str(), (TK), 0) != INT2DDS_RET_OK) { \
      RCUTILS_SET_ERROR_MSG("int2dds_type_info_add_field failed"); \
      return RCUTILS_RET_ERROR; \
    } \
    Member m; \
    m.name = field; \
    m.kind = MEMBER_LEAF; \
    m.type_code = (TK); \
    m.flat_name = field; \
    b->members.push_back(std::move(m)); \
    return RCUTILS_RET_OK; \
  }

DEFINE_ADD_SCALAR(dtb_add_bool, INT2DDS_FIELD_BOOL)
DEFINE_ADD_SCALAR(dtb_add_byte, INT2DDS_FIELD_BYTE)
DEFINE_ADD_SCALAR(dtb_add_char, INT2DDS_FIELD_CHAR8)
DEFINE_ADD_SCALAR(dtb_add_float32, INT2DDS_FIELD_FLOAT32)
DEFINE_ADD_SCALAR(dtb_add_float64, INT2DDS_FIELD_FLOAT64)
DEFINE_ADD_SCALAR(dtb_add_int8, INT2DDS_FIELD_INT8)
DEFINE_ADD_SCALAR(dtb_add_uint8, INT2DDS_FIELD_UINT8)
DEFINE_ADD_SCALAR(dtb_add_int16, INT2DDS_FIELD_INT16)
DEFINE_ADD_SCALAR(dtb_add_uint16, INT2DDS_FIELD_UINT16)
DEFINE_ADD_SCALAR(dtb_add_int32, INT2DDS_FIELD_INT32)
DEFINE_ADD_SCALAR(dtb_add_uint32, INT2DDS_FIELD_UINT32)
DEFINE_ADD_SCALAR(dtb_add_int64, INT2DDS_FIELD_INT64)
DEFINE_ADD_SCALAR(dtb_add_uint64, INT2DDS_FIELD_UINT64)

// String/wstring members build the same way (only the field code differs);
// STRING -> String8, WSTRING -> String16 in field_type_to_type_identifier.
DEFINE_ADD_SCALAR(dtb_add_string, INT2DDS_FIELD_STRING)
DEFINE_ADD_SCALAR(dtb_add_wstring, INT2DDS_FIELD_WSTRING)

// Array and sequence member adders. Arrays and bounded sequences take an extra length.
#define DEFINE_ADD_ARRAY(FN, ELEM) \
  rcutils_ret_t FN( \
    rosidl_dynamic_typesupport_serialization_support_impl_t * ss, \
    rosidl_dynamic_typesupport_dynamic_type_builder_impl_t * builder, \
    rosidl_dynamic_typesupport_member_id_t id, \
    const char * name, size_t name_length, \
    const char * default_value, size_t default_value_length, \
    size_t array_length) \
  { \
    (void)ss; \
    (void)id; \
    (void)default_value; \
    (void)default_value_length; \
    auto * b = builder_of(builder); \
    if (!b || !b->info) { \
      RCUTILS_SET_ERROR_MSG("invalid int2dds dynamic type builder"); \
      return RCUTILS_RET_ERROR; \
    } \
    std::string field(name, name_length); \
    Int2DdsRet r = int2dds_type_info_add_array_field( \
      b->info, field.c_str(), (ELEM), static_cast<uint32_t>(array_length), 0); \
    if (r != INT2DDS_RET_OK) { \
      RCUTILS_SET_ERROR_MSG("int2dds_type_info_add_array_field failed"); \
      return RCUTILS_RET_ERROR; \
    } \
    Member m; \
    m.name = field; \
    m.kind = MEMBER_COLLECTION; \
    m.type_code = (ELEM); \
    m.is_array = true; \
    m.extent = static_cast<uint32_t>(array_length); \
    m.flat_name = field; \
    b->members.push_back(std::move(m)); \
    return RCUTILS_RET_OK; \
  }

#define DEFINE_ADD_USEQ(FN, ELEM) \
  rcutils_ret_t FN( \
    rosidl_dynamic_typesupport_serialization_support_impl_t * ss, \
    rosidl_dynamic_typesupport_dynamic_type_builder_impl_t * builder, \
    rosidl_dynamic_typesupport_member_id_t id, \
    const char * name, size_t name_length, \
    const char * default_value, size_t default_value_length) \
  { \
    (void)ss; \
    (void)id; \
    (void)default_value; \
    (void)default_value_length; \
    auto * b = builder_of(builder); \
    if (!b || !b->info) { \
      RCUTILS_SET_ERROR_MSG("invalid int2dds dynamic type builder"); \
      return RCUTILS_RET_ERROR; \
    } \
    std::string field(name, name_length); \
    Int2DdsRet r = int2dds_type_info_add_sequence_field(b->info, field.c_str(), (ELEM), 0u, 0); \
    if (r != INT2DDS_RET_OK) { \
      RCUTILS_SET_ERROR_MSG("int2dds_type_info_add_sequence_field failed"); \
      return RCUTILS_RET_ERROR; \
    } \
    Member m; \
    m.name = field; \
    m.kind = MEMBER_COLLECTION; \
    m.type_code = (ELEM); \
    m.is_array = false; \
    m.extent = 0u; \
    m.flat_name = field; \
    b->members.push_back(std::move(m)); \
    return RCUTILS_RET_OK; \
  }

#define DEFINE_ADD_BSEQ(FN, ELEM) \
  rcutils_ret_t FN( \
    rosidl_dynamic_typesupport_serialization_support_impl_t * ss, \
    rosidl_dynamic_typesupport_dynamic_type_builder_impl_t * builder, \
    rosidl_dynamic_typesupport_member_id_t id, \
    const char * name, size_t name_length, \
    const char * default_value, size_t default_value_length, \
    size_t sequence_bound) \
  { \
    (void)ss; \
    (void)id; \
    (void)default_value; \
    (void)default_value_length; \
    auto * b = builder_of(builder); \
    if (!b || !b->info) { \
      RCUTILS_SET_ERROR_MSG("invalid int2dds dynamic type builder"); \
      return RCUTILS_RET_ERROR; \
    } \
    std::string field(name, name_length); \
    Int2DdsRet r = int2dds_type_info_add_sequence_field( \
      b->info, field.c_str(), (ELEM), static_cast<uint32_t>(sequence_bound), 0); \
    if (r != INT2DDS_RET_OK) { \
      RCUTILS_SET_ERROR_MSG("int2dds_type_info_add_sequence_field failed"); \
      return RCUTILS_RET_ERROR; \
    } \
    Member m; \
    m.name = field; \
    m.kind = MEMBER_COLLECTION; \
    m.type_code = (ELEM); \
    m.is_array = false; \
    m.extent = static_cast<uint32_t>(sequence_bound); \
    m.flat_name = field; \
    b->members.push_back(std::move(m)); \
    return RCUTILS_RET_OK; \
  }

#define DEFINE_ADD_COLLECTIONS(PREFIX, ELEM) \
  DEFINE_ADD_ARRAY(dtb_add_ ## PREFIX ## _array, ELEM) \
  DEFINE_ADD_USEQ(dtb_add_ ## PREFIX ## _useq, ELEM) \
  DEFINE_ADD_BSEQ(dtb_add_ ## PREFIX ## _bseq, ELEM)

DEFINE_ADD_COLLECTIONS(bool, INT2DDS_FIELD_BOOL)
DEFINE_ADD_COLLECTIONS(byte, INT2DDS_FIELD_BYTE)
DEFINE_ADD_COLLECTIONS(char, INT2DDS_FIELD_CHAR8)
DEFINE_ADD_COLLECTIONS(float32, INT2DDS_FIELD_FLOAT32)
DEFINE_ADD_COLLECTIONS(float64, INT2DDS_FIELD_FLOAT64)
DEFINE_ADD_COLLECTIONS(int8, INT2DDS_FIELD_INT8)
DEFINE_ADD_COLLECTIONS(uint8, INT2DDS_FIELD_UINT8)
DEFINE_ADD_COLLECTIONS(int16, INT2DDS_FIELD_INT16)
DEFINE_ADD_COLLECTIONS(uint16, INT2DDS_FIELD_UINT16)
DEFINE_ADD_COLLECTIONS(int32, INT2DDS_FIELD_INT32)
DEFINE_ADD_COLLECTIONS(uint32, INT2DDS_FIELD_UINT32)
DEFINE_ADD_COLLECTIONS(int64, INT2DDS_FIELD_INT64)
DEFINE_ADD_COLLECTIONS(uint64, INT2DDS_FIELD_UINT64)
DEFINE_ADD_COLLECTIONS(string, INT2DDS_FIELD_STRING)
DEFINE_ADD_COLLECTIONS(wstring, INT2DDS_FIELD_WSTRING)

// Recursively flatten one member of a nested type into the parent builder. LEAF/COLLECTION
// members are re-added to the parent's flat int2dds type under a generated name; NESTED
// members recurse. Returns false if any re-add fails.
bool flatten_member(BuilderHandle * b, const Member & src, std::vector<Member> & out)
{
  if (src.kind == MEMBER_NESTED) {
    Member nm;
    nm.name = src.name;
    nm.kind = MEMBER_NESTED;
    for (const Member & c : src.sub) {
      if (!flatten_member(b, c, nm.sub)) {
        return false;
      }
    }
    out.push_back(std::move(nm));
    return true;
  }
  std::string flat = "_int2dds_f" + std::to_string(b->flat_counter++);
  Int2DdsRet r;
  if (src.kind == MEMBER_COLLECTION) {
    if (src.is_array) {
      r = int2dds_type_info_add_array_field(b->info, flat.c_str(), src.type_code, src.extent, 0);
    } else {
      r = int2dds_type_info_add_sequence_field(b->info, flat.c_str(), src.type_code, src.extent, 0);
    }
  } else {
    r = int2dds_type_info_add_field(b->info, flat.c_str(), src.type_code, 0);
  }
  if (r != INT2DDS_RET_OK) {
    RCUTILS_SET_ERROR_MSG("failed to flatten nested member into parent type");
    return false;
  }
  Member lm = src;
  lm.flat_name = flat;
  lm.sub.clear();
  out.push_back(std::move(lm));
  return true;
}

rcutils_ret_t
add_complex_from_members(
  BuilderHandle * b, const char * name, size_t name_length,
  const std::vector<Member> & nested_members)
{
  if (!b || !b->info) {
    RCUTILS_SET_ERROR_MSG("invalid int2dds dynamic type builder");
    return RCUTILS_RET_ERROR;
  }
  Member nm;
  nm.name.assign(name, name_length);
  nm.kind = MEMBER_NESTED;
  for (const Member & c : nested_members) {
    if (!flatten_member(b, c, nm.sub)) {
      return RCUTILS_RET_ERROR;
    }
  }
  b->members.push_back(std::move(nm));
  return RCUTILS_RET_OK;
}

rcutils_ret_t
dtb_add_complex_member(
  rosidl_dynamic_typesupport_serialization_support_impl_t * ss,
  rosidl_dynamic_typesupport_dynamic_type_builder_impl_t * builder,
  rosidl_dynamic_typesupport_member_id_t id,
  const char * name, size_t name_length,
  const char * default_value, size_t default_value_length,
  rosidl_dynamic_typesupport_dynamic_type_impl_t * nested_struct)
{
  (void)ss;
  (void)id;
  (void)default_value;
  (void)default_value_length;
  auto * nt = type_of(nested_struct);
  if (!nt) {
    RCUTILS_SET_ERROR_MSG("invalid nested dynamic type");
    return RCUTILS_RET_ERROR;
  }
  return add_complex_from_members(builder_of(builder), name, name_length, nt->members);
}

rcutils_ret_t
dtb_add_complex_member_builder(
  rosidl_dynamic_typesupport_serialization_support_impl_t * ss,
  rosidl_dynamic_typesupport_dynamic_type_builder_impl_t * builder,
  rosidl_dynamic_typesupport_member_id_t id,
  const char * name, size_t name_length,
  const char * default_value, size_t default_value_length,
  rosidl_dynamic_typesupport_dynamic_type_builder_impl_t * nested_struct_builder)
{
  (void)ss;
  (void)id;
  (void)default_value;
  (void)default_value_length;
  auto * nb = builder_of(nested_struct_builder);
  if (!nb) {
    RCUTILS_SET_ERROR_MSG("invalid nested dynamic type builder");
    return RCUTILS_RET_ERROR;
  }
  return add_complex_from_members(builder_of(builder), name, name_length, nb->members);
}

// Arrays/sequences of nested structs cannot be flattened at a static element count; reject
// them rather than build a type that would misdecode. The array/bounded-sequence slots pass
// a trailing length, the unbounded-sequence slot does not.
rcutils_ret_t
dtb_add_complex_sized_unsupported(
  rosidl_dynamic_typesupport_serialization_support_impl_t * ss,
  rosidl_dynamic_typesupport_dynamic_type_builder_impl_t * builder,
  rosidl_dynamic_typesupport_member_id_t id,
  const char * name, size_t name_length,
  const char * default_value, size_t default_value_length,
  rosidl_dynamic_typesupport_dynamic_type_impl_t * nested_struct,
  size_t bound)
{
  (void)ss;
  (void)builder;
  (void)id;
  (void)name;
  (void)name_length;
  (void)default_value;
  (void)default_value_length;
  (void)nested_struct;
  (void)bound;
  RCUTILS_SET_ERROR_MSG("arrays/sequences of nested structs are not supported");
  return RCUTILS_RET_ERROR;
}

rcutils_ret_t
dtb_add_complex_unbounded_unsupported(
  rosidl_dynamic_typesupport_serialization_support_impl_t * ss,
  rosidl_dynamic_typesupport_dynamic_type_builder_impl_t * builder,
  rosidl_dynamic_typesupport_member_id_t id,
  const char * name, size_t name_length,
  const char * default_value, size_t default_value_length,
  rosidl_dynamic_typesupport_dynamic_type_impl_t * nested_struct)
{
  (void)ss;
  (void)builder;
  (void)id;
  (void)name;
  (void)name_length;
  (void)default_value;
  (void)default_value_length;
  (void)nested_struct;
  RCUTILS_SET_ERROR_MSG("sequences of nested structs are not supported");
  return RCUTILS_RET_ERROR;
}

// ===== Dynamic data ==========================================================
rcutils_ret_t
dd_init_from_type(
  rosidl_dynamic_typesupport_serialization_support_impl_t * ss,
  rosidl_dynamic_typesupport_dynamic_type_impl_t * type,
  rcutils_allocator_t * allocator,
  rosidl_dynamic_typesupport_dynamic_data_impl_t * out)
{
  (void)ss;
  (void)allocator;
  auto * t = type_of(type);
  if (!t || !t->obj) {
    RCUTILS_SET_ERROR_MSG("invalid int2dds dynamic type");
    return RCUTILS_RET_ERROR;
  }
  auto * d = new (std::nothrow) DataHandle();
  if (!d) {
    RCUTILS_SET_ERROR_MSG("failed to allocate int2dds dynamic data");
    return RCUTILS_RET_BAD_ALLOC;
  }
  d->bytes = std::make_shared<std::vector<uint8_t>>();
  if (!d->bytes) {
    delete d;
    RCUTILS_SET_ERROR_MSG("failed to allocate int2dds dynamic data buffer");
    return RCUTILS_RET_BAD_ALLOC;
  }
  d->obj = t->obj;
  d->members_owned = t->members;
  d->members = &d->members_owned;
  out->handle = d;
  return RCUTILS_RET_OK;
}

rcutils_ret_t
dd_fini(
  rosidl_dynamic_typesupport_serialization_support_impl_t * ss,
  rosidl_dynamic_typesupport_dynamic_data_impl_t * data)
{
  (void)ss;
  auto * d = data_of(data);
  if (d) {
    delete d;
    data->handle = nullptr;
  }
  return RCUTILS_RET_OK;
}

rcutils_ret_t
dd_deserialize(
  rosidl_dynamic_typesupport_serialization_support_impl_t * ss,
  rosidl_dynamic_typesupport_dynamic_data_impl_t * data,
  rcutils_uint8_array_t * buffer)
{
  (void)ss;
  auto * d = data_of(data);
  if (!d || !d->bytes || !buffer) {
    RCUTILS_SET_ERROR_MSG("invalid int2dds dynamic data");
    return RCUTILS_RET_ERROR;
  }
  d->bytes->assign(buffer->buffer, buffer->buffer + buffer->buffer_length);
  return RCUTILS_RET_OK;
}

// Primitive getters resolve member id -> flat-sample path, then read the CDR buffer.
#define DEFINE_GET_PRIMITIVE(FN, CTYPE, SAMPLE_GET) \
  rcutils_ret_t FN( \
    rosidl_dynamic_typesupport_serialization_support_impl_t * ss, \
    const rosidl_dynamic_typesupport_dynamic_data_impl_t * data, \
    rosidl_dynamic_typesupport_member_id_t id, \
    CTYPE * value) \
  { \
    (void)ss; \
    const auto * d = data_of(data); \
    std::string path; \
    if (!d || !d->obj || !d->bytes || !resolve_path(d, id, path)) { \
      RCUTILS_SET_ERROR_MSG("invalid int2dds dynamic data or member id"); \
      return RCUTILS_RET_ERROR; \
    } \
    Int2DdsRet r = SAMPLE_GET(d->bytes->data(), d->bytes->size(), d->obj, path.c_str(), value); \
    if (r != INT2DDS_RET_OK) { \
      RCUTILS_SET_ERROR_MSG("int2dds_dynamic_sample_get failed"); \
      return RCUTILS_RET_ERROR; \
    } \
    return RCUTILS_RET_OK; \
  }

DEFINE_GET_PRIMITIVE(dd_get_bool, bool, int2dds_dynamic_sample_get_bool)
DEFINE_GET_PRIMITIVE(dd_get_byte, uint8_t, int2dds_dynamic_sample_get_byte)
DEFINE_GET_PRIMITIVE(dd_get_int8, int8_t, int2dds_dynamic_sample_get_i8)
DEFINE_GET_PRIMITIVE(dd_get_uint8, uint8_t, int2dds_dynamic_sample_get_u8)
DEFINE_GET_PRIMITIVE(dd_get_int16, int16_t, int2dds_dynamic_sample_get_i16)
DEFINE_GET_PRIMITIVE(dd_get_uint16, uint16_t, int2dds_dynamic_sample_get_u16)
DEFINE_GET_PRIMITIVE(dd_get_int32, int32_t, int2dds_dynamic_sample_get_i32)
DEFINE_GET_PRIMITIVE(dd_get_uint32, uint32_t, int2dds_dynamic_sample_get_u32)
DEFINE_GET_PRIMITIVE(dd_get_int64, int64_t, int2dds_dynamic_sample_get_i64)
DEFINE_GET_PRIMITIVE(dd_get_uint64, uint64_t, int2dds_dynamic_sample_get_u64)
DEFINE_GET_PRIMITIVE(dd_get_float32, float, int2dds_dynamic_sample_get_f32)
DEFINE_GET_PRIMITIVE(dd_get_float64, double, int2dds_dynamic_sample_get_f64)

// char maps to int2dds CHAR8 (uint8 wire); rosidl's getter takes char*.
rcutils_ret_t
dd_get_char(
  rosidl_dynamic_typesupport_serialization_support_impl_t * ss,
  const rosidl_dynamic_typesupport_dynamic_data_impl_t * data,
  rosidl_dynamic_typesupport_member_id_t id,
  char * value)
{
  (void)ss;
  const auto * d = data_of(data);
  std::string path;
  if (!d || !d->obj || !d->bytes || !resolve_path(d, id, path)) {
    RCUTILS_SET_ERROR_MSG("invalid int2dds dynamic data or member id");
    return RCUTILS_RET_ERROR;
  }
  uint8_t tmp = 0;
  if (int2dds_dynamic_sample_get_char8(
      d->bytes->data(), d->bytes->size(), d->obj, path.c_str(), &tmp) != INT2DDS_RET_OK)
  {
    RCUTILS_SET_ERROR_MSG("int2dds_dynamic_sample_get_char8 failed");
    return RCUTILS_RET_ERROR;
  }
  *value = static_cast<char>(tmp);
  return RCUTILS_RET_OK;
}

// String getter. int2dds writes into a caller buffer and always reports the needed length
// first (copy_str_to_c sets *out_len before the capacity check), so probe once with a
// 1-byte buffer to learn the length, then allocate exactly and read. A genuine
// decode/field failure leaves needed at 0; a too-small buffer sets needed to the real
// length (> 0) and returns DYNAMIC_DECODE_ERROR. The returned buffer is owned by the
// caller, matching the fastrtps reference (new char[]).
rcutils_ret_t
dd_get_string(
  rosidl_dynamic_typesupport_serialization_support_impl_t * ss,
  const rosidl_dynamic_typesupport_dynamic_data_impl_t * data,
  rosidl_dynamic_typesupport_member_id_t id,
  char ** value, size_t * value_length)
{
  (void)ss;
  const auto * d = data_of(data);
  std::string path;
  if (!d || !d->obj || !d->bytes || !value || !value_length || !resolve_path(d, id, path)) {
    RCUTILS_SET_ERROR_MSG("invalid int2dds dynamic data or member id");
    return RCUTILS_RET_ERROR;
  }
  size_t needed = 0;
  char probe[1] = {0};
  Int2DdsRet r = int2dds_dynamic_sample_get_string(
    d->bytes->data(), d->bytes->size(), d->obj, path.c_str(), probe, sizeof(probe), &needed);
  if (r != INT2DDS_RET_OK && needed == 0) {
    RCUTILS_SET_ERROR_MSG("int2dds_dynamic_sample_get_string failed");
    return RCUTILS_RET_ERROR;
  }
  char * out = new (std::nothrow) char[needed + 1];
  if (!out) {
    RCUTILS_SET_ERROR_MSG("failed to allocate string value");
    return RCUTILS_RET_BAD_ALLOC;
  }
  if (int2dds_dynamic_sample_get_string(
      d->bytes->data(), d->bytes->size(), d->obj, path.c_str(), out, needed + 1, &needed) !=
    INT2DDS_RET_OK)
  {
    delete[] out;
    RCUTILS_SET_ERROR_MSG("int2dds_dynamic_sample_get_string failed");
    return RCUTILS_RET_ERROR;
  }
  out[needed] = '\0';
  *value = out;
  *value_length = needed;
  return RCUTILS_RET_OK;
}

// wstring/char16: the type can be built, but the int2dds FFI exposes no flat-sample
// wstring reader (only a write-side value constructor), so the receive path fails
// gracefully instead of crashing. Adding int2dds_dynamic_sample_get_wstring to the FFI
// (mirroring get_string) would close this gap.
rcutils_ret_t
dd_get_wstring(
  rosidl_dynamic_typesupport_serialization_support_impl_t * ss,
  const rosidl_dynamic_typesupport_dynamic_data_impl_t * data,
  rosidl_dynamic_typesupport_member_id_t id,
  char16_t ** value, size_t * value_length)
{
  (void)ss;
  (void)data;
  (void)id;
  (void)value;
  (void)value_length;
  RCUTILS_SET_ERROR_MSG("wstring read unsupported: no int2dds flat-sample wstring reader");
  return RCUTILS_RET_ERROR;
}

// ===== Collection / nested navigation (loan model) ===========================
// Number of items: struct member count, or a collection's element count (probed since the
// flat API has no participant-free length query; an out-of-range index reads FIELD_NOT_FOUND).
rcutils_ret_t
dd_get_item_count(
  rosidl_dynamic_typesupport_serialization_support_impl_t * ss,
  const rosidl_dynamic_typesupport_dynamic_data_impl_t * data,
  size_t * item_count)
{
  (void)ss;
  const auto * d = data_of(data);
  if (!d || !item_count) {
    RCUTILS_SET_ERROR_MSG("invalid int2dds dynamic data");
    return RCUTILS_RET_ERROR;
  }
  if (!d->is_collection) {
    *item_count = d->members ? d->members->size() : 0;
    return RCUTILS_RET_OK;
  }
  if (!d->bytes || !d->obj) {
    RCUTILS_SET_ERROR_MSG("invalid int2dds dynamic data");
    return RCUTILS_RET_ERROR;
  }
  size_t count = 0;
  const size_t kMaxItems = 1u << 24;  // guard against a probe that never terminates
  while (count < kMaxItems) {
    std::string p = d->flat_name + "[" + std::to_string(count) + "]";
    if (probe_path(d, p, d->element_code) != INT2DDS_RET_OK) {
      break;
    }
    ++count;
  }
  *item_count = count;
  return RCUTILS_RET_OK;
}

// Member id lookups. Member ids are the declaration index at the struct level and the
// element index within a collection, so at-index is the identity.
rcutils_ret_t
dd_get_member_id_by_name(
  rosidl_dynamic_typesupport_serialization_support_impl_t * ss,
  const rosidl_dynamic_typesupport_dynamic_data_impl_t * data,
  const char * name, size_t name_length,
  rosidl_dynamic_typesupport_member_id_t * member_id)
{
  (void)ss;
  const auto * d = data_of(data);
  if (!d || !name || !member_id || d->is_collection || !d->members) {
    RCUTILS_SET_ERROR_MSG("invalid int2dds dynamic data for member lookup");
    return RCUTILS_RET_ERROR;
  }
  std::string n(name, name_length);
  for (size_t i = 0; i < d->members->size(); ++i) {
    if ((*d->members)[i].name == n) {
      *member_id = static_cast<rosidl_dynamic_typesupport_member_id_t>(i);
      return RCUTILS_RET_OK;
    }
  }
  RCUTILS_SET_ERROR_MSG("member name not found");
  return RCUTILS_RET_ERROR;
}

rcutils_ret_t
dd_get_member_id_at_index(
  rosidl_dynamic_typesupport_serialization_support_impl_t * ss,
  const rosidl_dynamic_typesupport_dynamic_data_impl_t * data,
  size_t index,
  rosidl_dynamic_typesupport_member_id_t * member_id)
{
  (void)ss;
  (void)data;
  if (!member_id) {
    RCUTILS_SET_ERROR_MSG("null member id output");
    return RCUTILS_RET_ERROR;
  }
  *member_id = static_cast<rosidl_dynamic_typesupport_member_id_t>(index);
  return RCUTILS_RET_OK;
}

// Loan a child view for a nested-struct or collection member. The child shares the sample
// bytes and type object; a nested child borrows the sub-member tree, a collection child
// carries the collection's flat field name and element code.
rcutils_ret_t
dd_loan_value(
  rosidl_dynamic_typesupport_serialization_support_impl_t * ss,
  rosidl_dynamic_typesupport_dynamic_data_impl_t * data,
  rosidl_dynamic_typesupport_member_id_t id,
  rcutils_allocator_t * allocator,
  rosidl_dynamic_typesupport_dynamic_data_impl_t * loaned_dynamic_data)
{
  (void)ss;
  (void)allocator;
  auto * d = data_of(data);
  if (!d || d->is_collection || !d->members ||
    static_cast<size_t>(id) >= d->members->size() || !loaned_dynamic_data)
  {
    RCUTILS_SET_ERROR_MSG("invalid int2dds dynamic data for loan");
    return RCUTILS_RET_ERROR;
  }
  const Member & m = (*d->members)[id];
  if (m.kind == MEMBER_LEAF) {
    RCUTILS_SET_ERROR_MSG("loan_value supports only nested or collection members");
    return RCUTILS_RET_ERROR;
  }
  auto * child = new (std::nothrow) DataHandle();
  if (!child) {
    RCUTILS_SET_ERROR_MSG("failed to allocate loaned dynamic data");
    return RCUTILS_RET_BAD_ALLOC;
  }
  child->bytes = d->bytes;  // shared with the parent view
  child->obj = d->obj;
  if (m.kind == MEMBER_COLLECTION) {
    child->is_collection = true;
    child->element_code = m.type_code;
    child->flat_name = m.flat_name;
  } else {  // MEMBER_NESTED
    child->is_collection = false;
    child->members = &m.sub;  // borrows the parent's member tree (stable for the loan)
  }
  loaned_dynamic_data->handle = child;
  return RCUTILS_RET_OK;
}

rcutils_ret_t
dd_return_loaned_value(
  rosidl_dynamic_typesupport_serialization_support_impl_t * ss,
  rosidl_dynamic_typesupport_dynamic_data_impl_t * data,
  const rosidl_dynamic_typesupport_dynamic_data_impl_t * inner_data)
{
  (void)ss;
  (void)data;
  if (inner_data) {
    delete static_cast<DataHandle *>(inner_data->handle);
  }
  return RCUTILS_RET_OK;
}

}  // namespace

namespace rmw_int2dds_cpp
{

rcutils_ret_t
init_dynamic_serialization_support_impl(
  rcutils_allocator_t * allocator,
  rosidl_dynamic_typesupport_serialization_support_impl_t * impl)
{
  impl->allocator = *allocator;
  impl->serialization_library_identifier = kLibraryIdentifier;
  // int2dds type building is stateless; a non-null marker handle is enough.
  impl->handle = const_cast<char *>(kLibraryIdentifier);
  return RCUTILS_RET_OK;
}

rcutils_ret_t
init_dynamic_serialization_support_interface(
  rcutils_allocator_t * allocator,
  rosidl_dynamic_typesupport_serialization_support_interface_t * iface)
{
  // Start from a clean slate; only implemented slots are wired.
  std::memset(iface, 0, sizeof(*iface));
  iface->allocator = *allocator;
  iface->serialization_library_identifier = kLibraryIdentifier;

  iface->serialization_support_impl_fini = ss_impl_fini;
  iface->serialization_support_interface_fini = ss_interface_fini;

  iface->dynamic_type_builder_init = dtb_init;
  iface->dynamic_type_builder_fini = dtb_fini;
  iface->dynamic_type_init_from_dynamic_type_builder = dt_init_from_builder;
  iface->dynamic_type_fini = dt_fini;
  iface->dynamic_type_get_member_count = dt_get_member_count;
  iface->dynamic_type_get_name = dt_get_name;
  iface->dynamic_type_builder_get_name = dtb_get_name;
  iface->dynamic_type_builder_set_name = dtb_set_name;

  iface->dynamic_type_builder_add_bool_member = dtb_add_bool;
  iface->dynamic_type_builder_add_byte_member = dtb_add_byte;
  iface->dynamic_type_builder_add_char_member = dtb_add_char;
  iface->dynamic_type_builder_add_float32_member = dtb_add_float32;
  iface->dynamic_type_builder_add_float64_member = dtb_add_float64;
  iface->dynamic_type_builder_add_int8_member = dtb_add_int8;
  iface->dynamic_type_builder_add_uint8_member = dtb_add_uint8;
  iface->dynamic_type_builder_add_int16_member = dtb_add_int16;
  iface->dynamic_type_builder_add_uint16_member = dtb_add_uint16;
  iface->dynamic_type_builder_add_int32_member = dtb_add_int32;
  iface->dynamic_type_builder_add_uint32_member = dtb_add_uint32;
  iface->dynamic_type_builder_add_int64_member = dtb_add_int64;
  iface->dynamic_type_builder_add_uint64_member = dtb_add_uint64;
  iface->dynamic_type_builder_add_string_member = dtb_add_string;
  iface->dynamic_type_builder_add_wstring_member = dtb_add_wstring;

  // Array and sequence builders for every element type.
#define WIRE_COLLECTIONS(T) \
  iface->dynamic_type_builder_add_ ## T ## _array_member = dtb_add_ ## T ## _array; \
  iface->dynamic_type_builder_add_ ## T ## _unbounded_sequence_member = dtb_add_ ## T ## _useq; \
  iface->dynamic_type_builder_add_ ## T ## _bounded_sequence_member = dtb_add_ ## T ## _bseq;
  WIRE_COLLECTIONS(bool)
  WIRE_COLLECTIONS(byte)
  WIRE_COLLECTIONS(char)
  WIRE_COLLECTIONS(float32)
  WIRE_COLLECTIONS(float64)
  WIRE_COLLECTIONS(int8)
  WIRE_COLLECTIONS(uint8)
  WIRE_COLLECTIONS(int16)
  WIRE_COLLECTIONS(uint16)
  WIRE_COLLECTIONS(int32)
  WIRE_COLLECTIONS(uint32)
  WIRE_COLLECTIONS(int64)
  WIRE_COLLECTIONS(uint64)
  WIRE_COLLECTIONS(string)
  WIRE_COLLECTIONS(wstring)
#undef WIRE_COLLECTIONS

  // Nested struct members (flattened). Arrays/sequences of nested structs are rejected.
  iface->dynamic_type_builder_add_complex_member = dtb_add_complex_member;
  iface->dynamic_type_builder_add_complex_member_builder = dtb_add_complex_member_builder;
  iface->dynamic_type_builder_add_complex_array_member = dtb_add_complex_sized_unsupported;
  iface->dynamic_type_builder_add_complex_unbounded_sequence_member =
    dtb_add_complex_unbounded_unsupported;
  iface->dynamic_type_builder_add_complex_bounded_sequence_member =
    dtb_add_complex_sized_unsupported;

  iface->dynamic_data_init_from_dynamic_type = dd_init_from_type;
  iface->dynamic_data_fini = dd_fini;
  iface->dynamic_data_deserialize = dd_deserialize;

  iface->dynamic_data_get_bool_value = dd_get_bool;
  iface->dynamic_data_get_byte_value = dd_get_byte;
  iface->dynamic_data_get_int8_value = dd_get_int8;
  iface->dynamic_data_get_uint8_value = dd_get_uint8;
  iface->dynamic_data_get_int16_value = dd_get_int16;
  iface->dynamic_data_get_uint16_value = dd_get_uint16;
  iface->dynamic_data_get_int32_value = dd_get_int32;
  iface->dynamic_data_get_uint32_value = dd_get_uint32;
  iface->dynamic_data_get_int64_value = dd_get_int64;
  iface->dynamic_data_get_uint64_value = dd_get_uint64;
  iface->dynamic_data_get_float32_value = dd_get_float32;
  iface->dynamic_data_get_float64_value = dd_get_float64;
  iface->dynamic_data_get_char_value = dd_get_char;
  iface->dynamic_data_get_string_value = dd_get_string;
  iface->dynamic_data_get_wstring_value = dd_get_wstring;

  iface->dynamic_data_get_item_count = dd_get_item_count;
  iface->dynamic_data_get_member_id_by_name = dd_get_member_id_by_name;
  iface->dynamic_data_get_member_id_at_index = dd_get_member_id_at_index;
  iface->dynamic_data_get_array_index = dd_get_member_id_at_index;
  iface->dynamic_data_loan_value = dd_loan_value;
  iface->dynamic_data_return_loaned_value = dd_return_loaned_value;

  return RCUTILS_RET_OK;
}

}  // namespace rmw_int2dds_cpp

#endif  // __has_include rosidl_dynamic_typesupport serialization_support_interface.h
