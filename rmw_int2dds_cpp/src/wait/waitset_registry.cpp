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

#include "waitset_registry.hpp"  // NOLINT(build/include_subdir)

#include <algorithm>
#include <mutex>
#include <vector>

#include "int2dds-ffi.h"  // NOLINT(build/include_subdir): vendored FFI header

namespace rmw_int2dds_cpp
{

namespace
{
std::mutex & registry_mutex()
{
  static std::mutex * m = new std::mutex();  // leaked on purpose: valid during static destruction
  return *m;
}
std::vector<WaitSetData *> & wait_sets()
{
  static std::vector<WaitSetData *> * v = new std::vector<WaitSetData *>();
  return *v;
}
}  // namespace

void
waitset_registry_add(WaitSetData * ws_data)
{
  std::lock_guard<std::mutex> lock(registry_mutex());
  wait_sets().push_back(ws_data);
}

void
waitset_registry_remove(WaitSetData * ws_data)
{
  std::lock_guard<std::mutex> lock(registry_mutex());
  wait_sets().erase(
    std::remove(wait_sets().begin(), wait_sets().end(), ws_data), wait_sets().end());
}

void
waitset_registry_clean_caches()
{
  std::lock_guard<std::mutex> lock(registry_mutex());
  for (auto * ws_data : wait_sets()) {
    std::unique_lock<std::mutex> ws_lock(ws_data->lock);
    // cache_busy never spans the blocking FFI wait, so this wait is short.
    ws_data->cache_cv.wait(ws_lock, [ws_data] {return !ws_data->cache_busy;});
    if (!ws_data->inuse) {
      waitset_detach_all(ws_data);
    }
    // inuse without cache_busy means the wait set is blocked inside
    // int2dds_waitset_wait_ex_ns with attachments and cache mirroring its
    // current entity set. The entity being destroyed cannot be in that set
    // (the rmw contract forbids destroying entities being waited on), so
    // there is nothing of it here to clean and skipping is safe.
  }
}

void
waitset_detach_all(WaitSetData * ws_data)
{
  for (const auto & entry : ws_data->attached_conditions) {
    int2dds_waitset_detach_statuscondition(ws_data->waitset, entry.first);
  }
  for (const auto & entry : ws_data->attached_guards) {
    int2dds_waitset_detach_guardcondition(ws_data->waitset, entry.first);
  }
  ws_data->attached_conditions.clear();
  ws_data->attached_guards.clear();

  ws_data->cached_subscriptions.clear();
  ws_data->cached_guard_conditions.clear();
  ws_data->cached_services.clear();
  ws_data->cached_clients.clear();
  ws_data->cached_events.clear();
}

}  // namespace rmw_int2dds_cpp
