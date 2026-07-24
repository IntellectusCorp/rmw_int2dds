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

#ifndef RMW_INT2DDS_CPP__DYNAMIC_SERIALIZATION_SUPPORT_HPP_
#define RMW_INT2DDS_CPP__DYNAMIC_SERIALIZATION_SUPPORT_HPP_

#include "rcutils/allocator.h"
#include "rcutils/types/rcutils_ret.h"

#include "rosidl_dynamic_typesupport/api/serialization_support_interface.h"

namespace rmw_int2dds_cpp
{

// Populate the serialization support impl handle (int2dds backend marker + allocator).
rcutils_ret_t
init_dynamic_serialization_support_impl(
  rcutils_allocator_t * allocator,
  rosidl_dynamic_typesupport_serialization_support_impl_t * impl);

// Populate the serialization support method table (vtable) with int2dds functions.
rcutils_ret_t
init_dynamic_serialization_support_interface(
  rcutils_allocator_t * allocator,
  rosidl_dynamic_typesupport_serialization_support_interface_t * iface);

}  // namespace rmw_int2dds_cpp

#endif  // RMW_INT2DDS_CPP__DYNAMIC_SERIALIZATION_SUPPORT_HPP_
