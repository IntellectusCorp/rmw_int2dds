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

#include "rmw/rmw.h"
#include "rmw/allocators.h"
#include "rmw/error_handling.h"

#include <mutex>

#include "int2dds-ffi.h"  // NOLINT(build/include_subdir): vendored FFI header
#include "rmw_int2dds_cpp/identifier.hpp"
#include "rmw_int2dds_cpp/types.hpp"
#include "waitset_registry.hpp"  // NOLINT(build/include_subdir)

extern "C"
{
rmw_wait_set_t *
rmw_create_wait_set(rmw_context_t * context, size_t max_conditions)
{
  (void)max_conditions;  // int2dds waitset doesn't need pre-sizing

  RMW_CHECK_ARGUMENT_FOR_NULL(context, nullptr);

  if (context->implementation_identifier != rmw_int2dds_cpp::implementation_identifier) {
    RMW_SET_ERROR_MSG("context not from this implementation");
    return nullptr;
  }

  // Create wait set data
  auto * ws_data = new (std::nothrow) rmw_int2dds_cpp::WaitSetData();
  if (ws_data == nullptr) {
    RMW_SET_ERROR_MSG("failed to allocate wait set data");
    return nullptr;
  }

  // Create DDS waitset
  Int2DdsRet ret = int2dds_waitset_new(&ws_data->waitset);
  if (ret != INT2DDS_RET_OK) {
    delete ws_data;
    RMW_SET_ERROR_MSG("failed to create DDS waitset");
    return nullptr;
  }

  // Allocate RMW wait set
  rmw_wait_set_t * wait_set = rmw_wait_set_allocate();
  if (wait_set == nullptr) {
    int2dds_waitset_delete(ws_data->waitset);
    delete ws_data;
    RMW_SET_ERROR_MSG("failed to allocate wait set");
    return nullptr;
  }

  wait_set->implementation_identifier = rmw_int2dds_cpp::implementation_identifier;
  wait_set->data = ws_data;

  if (!rmw_int2dds_cpp::waitset_registry_add(ws_data)) {
    // Without a registry entry a later clean_caches could not see this wait set,
    // so fail creation instead of leaking an unregistered wait set.
    int2dds_waitset_delete(ws_data->waitset);
    delete ws_data;
    rmw_wait_set_free(wait_set);
    RMW_SET_ERROR_MSG("failed to register wait set");
    return nullptr;
  }

  return wait_set;
}

rmw_ret_t
rmw_destroy_wait_set(rmw_wait_set_t * wait_set)
{
// The conformance suite's expectation for a null wait_set changed across
// distros: Jazzy expects RMW_RET_ERROR, Lyrical expects RMW_RET_INVALID_ARGUMENT
// The probe is rmw/get_service_endpoint_info.h (rmw 7.9.1), used here only as a
// "Lyrical or newer" marker - the suite's expectation is not tied to that
// header's release, and no distro ships an rmw between 7.8.2 and 7.10.1.
#if __has_include("rmw/get_service_endpoint_info.h")
  RMW_CHECK_ARGUMENT_FOR_NULL(wait_set, RMW_RET_INVALID_ARGUMENT);
#else
  RMW_CHECK_ARGUMENT_FOR_NULL(wait_set, RMW_RET_ERROR);
#endif

  if (wait_set->implementation_identifier != rmw_int2dds_cpp::implementation_identifier) {
    RMW_SET_ERROR_MSG("wait set not from this implementation");
    return RMW_RET_INCORRECT_RMW_IMPLEMENTATION;
  }

  auto * ws_data = static_cast<rmw_int2dds_cpp::WaitSetData *>(wait_set->data);
  if (ws_data != nullptr) {
    // Detach while still registered: a concurrent entity destroy's clean_caches
    // serializes on ws_data->lock and cannot free a condition mid-detach.
    {
      std::lock_guard<std::mutex> lock(ws_data->lock);
      rmw_int2dds_cpp::waitset_detach_all(ws_data);
    }
    rmw_int2dds_cpp::waitset_registry_remove(ws_data);

    if (ws_data->waitset != nullptr) {
      int2dds_waitset_delete(ws_data->waitset);
    }
    delete ws_data;
  }

  rmw_wait_set_free(wait_set);
  return RMW_RET_OK;
}
}  // extern "C"
