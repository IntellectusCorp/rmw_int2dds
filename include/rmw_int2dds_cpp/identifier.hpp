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

#ifndef RMW_INT2DDS_CPP__IDENTIFIER_HPP_
#define RMW_INT2DDS_CPP__IDENTIFIER_HPP_

#include "rmw_int2dds_cpp/visibility_control.h"

namespace rmw_int2dds_cpp
{

/// RMW implementation identifier
RMW_INT2DDS_CPP_PUBLIC
extern const char * const implementation_identifier;

/// Serialization format used by this RMW implementation
RMW_INT2DDS_CPP_PUBLIC
extern const char * const serialization_format;

}  // namespace rmw_int2dds_cpp

#endif  // RMW_INT2DDS_CPP__IDENTIFIER_HPP_
