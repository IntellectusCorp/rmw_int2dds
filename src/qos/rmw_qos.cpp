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
#include "rmw/error_handling.h"
#include "rmw/qos_profiles.h"
#include "rmw_dds_common/qos.hpp"
#include "rmw_int2dds_cpp/identifier.hpp"

extern "C"
{
rmw_ret_t
rmw_qos_profile_check_compatible(
  const rmw_qos_profile_t publisher_profile,
  const rmw_qos_profile_t subscription_profile,
  rmw_qos_compatibility_type_t * compatibility,
  char * reason,
  size_t reason_size)
{
  // Delegate to the canonical rmw_dds_common implementation. Our previous
  // hand-rolled check only covered the reliability (best_effort->reliable) and
  // durability (volatile->transient_local) error cases and returned OK for
  // everything else. That made it disagree with the reference RMWs and with our
  // own DDS matching: e.g. for a reliable publisher + best_effort subscription
  // the reference reports WARNING while we reported OK, so
  // test_subscription.count_mismatched expected a match that (correctly) never
  // happens. rmw_dds_common is already a dependency and is the same logic used
  // by fastrtps/cyclone, keeping our verdicts consistent with the conformance
  // suite. It also performs the compatibility/reason argument validation.
  return rmw_dds_common::qos_profile_check_compatible(
    publisher_profile, subscription_profile, compatibility, reason, reason_size);
}
}  // extern "C"
