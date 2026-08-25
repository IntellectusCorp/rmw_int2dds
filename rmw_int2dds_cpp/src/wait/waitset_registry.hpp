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

#ifndef WAIT__WAITSET_REGISTRY_HPP_
#define WAIT__WAITSET_REGISTRY_HPP_

#include "rmw_int2dds_cpp/types.hpp"

namespace rmw_int2dds_cpp
{

bool waitset_registry_add(WaitSetData * ws_data);
void waitset_registry_remove(WaitSetData * ws_data);

// Detaches every cached condition from every registered wait set. Waits out
// the short sections where rmw_wait is reading or rebuilding a cache; a wait
// set blocked in the FFI wait is skipped, which is safe because its
// attachments then mirror the current entity set, which cannot contain an
// entity being destroyed. Entity destroy paths call this before freeing
// anything a wait set caches - condition handles as well as the entity data
// pointers the cache keys on - so no dangling entry survives a destroy.
void waitset_registry_clean_caches();

// Detaches all cached conditions of one wait set. Caller must have exclusive
// access to ws_data, either by holding ws_data->lock or by having set inuse.
void waitset_detach_all(WaitSetData * ws_data);

// Takes over a destroyed guard condition instead of freeing it: rclcpp
// destroys them while another executor thread is still scanning that set.
void waitset_registry_retire_guard(GuardConditionData * gc_data);

}  // namespace rmw_int2dds_cpp

#endif  // WAIT__WAITSET_REGISTRY_HPP_
