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

#include <tuple>
#include "rmw/rmw.h"
#include "rmw/error_handling.h"
#include "rmw/init.h"
#include "rmw/init_options.h"

#include "rcutils/allocator.h"
#include "rcutils/strdup.h"

#include "int2dds-ffi.h"  // NOLINT(build/include_subdir): vendored FFI header
#include "rmw_int2dds_cpp/identifier.hpp"
#include "rmw_int2dds_cpp/types.hpp"
#include "../graph/discovery.hpp"

extern "C"
{
rmw_ret_t
rmw_init_options_init(
  rmw_init_options_t * init_options,
  rcutils_allocator_t allocator)
{
  RMW_CHECK_ARGUMENT_FOR_NULL(init_options, RMW_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_ALLOCATOR(&allocator, return RMW_RET_INVALID_ARGUMENT);

  if (init_options->implementation_identifier != nullptr) {
    RMW_SET_ERROR_MSG("init_options already initialized");
    return RMW_RET_INVALID_ARGUMENT;
  }

  init_options->instance_id = 0;
  init_options->implementation_identifier = rmw_int2dds_cpp::implementation_identifier;
  init_options->allocator = allocator;
  init_options->impl = nullptr;
  init_options->enclave = nullptr;
  init_options->domain_id = RMW_DEFAULT_DOMAIN_ID;
  init_options->security_options = rmw_get_default_security_options();
  init_options->localhost_only = RMW_LOCALHOST_ONLY_DEFAULT;

  return RMW_RET_OK;
}

rmw_ret_t
rmw_init_options_copy(
  const rmw_init_options_t * src,
  rmw_init_options_t * dst)
{
  RMW_CHECK_ARGUMENT_FOR_NULL(src, RMW_RET_INVALID_ARGUMENT);
  RMW_CHECK_ARGUMENT_FOR_NULL(dst, RMW_RET_INVALID_ARGUMENT);

  if (src->implementation_identifier == nullptr) {
    RMW_SET_ERROR_MSG("src init_options is not initialized");
    return RMW_RET_INVALID_ARGUMENT;
  }

  if (src->implementation_identifier != rmw_int2dds_cpp::implementation_identifier) {
    RMW_SET_ERROR_MSG("src init_options not from this implementation");
    return RMW_RET_INCORRECT_RMW_IMPLEMENTATION;
  }

  if (dst->implementation_identifier != nullptr) {
    RMW_SET_ERROR_MSG("dst init_options already initialized");
    return RMW_RET_INVALID_ARGUMENT;
  }

  *dst = *src;

  if (src->enclave != nullptr) {
    dst->enclave = rcutils_strdup(src->enclave, src->allocator);
    if (dst->enclave == nullptr) {
      return RMW_RET_BAD_ALLOC;
    }
  }

  return RMW_RET_OK;
}

rmw_ret_t
rmw_init_options_fini(rmw_init_options_t * init_options)
{
  RMW_CHECK_ARGUMENT_FOR_NULL(init_options, RMW_RET_INVALID_ARGUMENT);

  if (init_options->implementation_identifier == nullptr) {
    RMW_SET_ERROR_MSG("init_options is not initialized");
    return RMW_RET_INVALID_ARGUMENT;
  }

  if (init_options->implementation_identifier != rmw_int2dds_cpp::implementation_identifier) {
    RMW_SET_ERROR_MSG("init_options not from this implementation");
    return RMW_RET_INCORRECT_RMW_IMPLEMENTATION;
  }

  if (init_options->enclave != nullptr) {
    init_options->allocator.deallocate(init_options->enclave, init_options->allocator.state);
    init_options->enclave = nullptr;
  }

  *init_options = rmw_get_zero_initialized_init_options();
  return RMW_RET_OK;
}

rmw_ret_t
rmw_init(const rmw_init_options_t * options, rmw_context_t * context)
{
  RMW_CHECK_ARGUMENT_FOR_NULL(options, RMW_RET_INVALID_ARGUMENT);
  RMW_CHECK_ARGUMENT_FOR_NULL(context, RMW_RET_INVALID_ARGUMENT);

  if (options->implementation_identifier == nullptr) {
    RMW_SET_ERROR_MSG("options is not initialized");
    return RMW_RET_INVALID_ARGUMENT;
  }

  if (options->implementation_identifier != rmw_int2dds_cpp::implementation_identifier) {
    RMW_SET_ERROR_MSG("options not from this implementation");
    return RMW_RET_INCORRECT_RMW_IMPLEMENTATION;
  }

  if (options->enclave == nullptr) {
    RMW_SET_ERROR_MSG("options enclave is null");
    return RMW_RET_INVALID_ARGUMENT;
  }

  if (context->implementation_identifier != nullptr) {
    RMW_SET_ERROR_MSG("context already initialized");
    return RMW_RET_INVALID_ARGUMENT;
  }

  // Create context data
  auto * context_data = new (std::nothrow) rmw_int2dds_cpp::ContextData();
  if (context_data == nullptr) {
    RMW_SET_ERROR_MSG("failed to allocate context data");
    return RMW_RET_BAD_ALLOC;
  }

  // Get domain participant factory
  Int2DdsRet ret = int2dds_domain_participant_factory_get_instance(&context_data->factory);
  if (ret != INT2DDS_RET_OK) {
    delete context_data;
    RMW_SET_ERROR_MSG("failed to get participant factory");
    return RMW_RET_ERROR;
  }

  // Determine domain ID before narrowing to the DDS API's integer type.
  size_t actual_domain_id = options->domain_id;
  if (actual_domain_id == RMW_DEFAULT_DOMAIN_ID) {
    actual_domain_id = 0;  // Default to domain 0
  }
  context_data->domain_id = actual_domain_id;

  // Create participant
  ret = int2dds_create_participant(
    context_data->factory,
    "rmw_int2dds_participant",
    static_cast<int32_t>(actual_domain_id),
    &context_data->participant);
  if (ret != INT2DDS_RET_OK) {
    int2dds_domain_participant_factory_finalize(context_data->factory);
    delete context_data;
    RMW_SET_ERROR_MSG("failed to create participant");
    return RMW_RET_ERROR;
  }

  // Create default publisher
  ret = int2dds_create_publisher(
    context_data->participant,
    &context_data->default_publisher);
  if (ret != INT2DDS_RET_OK) {
    int2dds_delete_participant(context_data->participant);
    int2dds_domain_participant_factory_finalize(context_data->factory);
    delete context_data;
    RMW_SET_ERROR_MSG("failed to create default publisher");
    return RMW_RET_ERROR;
  }

  // Create default subscriber
  ret = int2dds_create_subscriber(
    context_data->participant,
    &context_data->default_subscriber);
  if (ret != INT2DDS_RET_OK) {
    int2dds_delete_publisher(context_data->default_publisher);
    int2dds_delete_participant(context_data->participant);
    int2dds_domain_participant_factory_finalize(context_data->factory);
    delete context_data;
    RMW_SET_ERROR_MSG("failed to create default subscriber");
    return RMW_RET_ERROR;
  }

  // Create graph guard condition
  ret = int2dds_guard_condition_new(&context_data->graph_guard_condition);
  if (ret != INT2DDS_RET_OK) {
    int2dds_delete_subscriber(context_data->default_subscriber);
    int2dds_delete_publisher(context_data->default_publisher);
    int2dds_delete_participant(context_data->participant);
    int2dds_domain_participant_factory_finalize(context_data->factory);
    delete context_data;
    RMW_SET_ERROR_MSG("failed to create graph guard condition");
    return RMW_RET_ERROR;
  }

  // Stand up standard rmw_dds_common node-graph discovery (ros_discovery_info).
  if (rmw_int2dds_cpp::init_discovery(context_data, options->enclave) != RMW_RET_OK) {
    int2dds_guard_condition_delete(context_data->graph_guard_condition);
    int2dds_delete_subscriber(context_data->default_subscriber);
    int2dds_delete_publisher(context_data->default_publisher);
    int2dds_delete_participant(context_data->participant);
    int2dds_domain_participant_factory_finalize(context_data->factory);
    delete context_data;
    RMW_SET_ERROR_MSG("failed to initialize node-graph discovery");
    return RMW_RET_ERROR;
  }

  context_data->is_shutdown = false;
  context_data->ref_count = 1;

  // Set up context
  context->instance_id = options->instance_id;
  context->actual_domain_id = actual_domain_id;
  context->implementation_identifier = rmw_int2dds_cpp::implementation_identifier;
  context->impl = reinterpret_cast<rmw_context_impl_t *>(context_data);

  // Copy options
  rmw_ret_t rmw_ret = rmw_init_options_copy(options, &context->options);
  if (rmw_ret != RMW_RET_OK) {
    // Clean up on failure
    int2dds_guard_condition_delete(context_data->graph_guard_condition);
    int2dds_delete_subscriber(context_data->default_subscriber);
    int2dds_delete_publisher(context_data->default_publisher);
    int2dds_delete_participant(context_data->participant);
    int2dds_domain_participant_factory_finalize(context_data->factory);
    delete context_data;
    context->impl = nullptr;
    return rmw_ret;
  }

  return RMW_RET_OK;
}

rmw_ret_t
rmw_shutdown(rmw_context_t * context)
{
  RMW_CHECK_ARGUMENT_FOR_NULL(context, RMW_RET_INVALID_ARGUMENT);

  if (context->implementation_identifier == nullptr) {
    RMW_SET_ERROR_MSG("context is not initialized");
    return RMW_RET_INVALID_ARGUMENT;
  }

  if (context->implementation_identifier != rmw_int2dds_cpp::implementation_identifier) {
    RMW_SET_ERROR_MSG("context not from this implementation");
    return RMW_RET_INCORRECT_RMW_IMPLEMENTATION;
  }

  auto * context_data = reinterpret_cast<rmw_int2dds_cpp::ContextData *>(context->impl);
  if (context_data == nullptr) {
    RMW_SET_ERROR_MSG("context not initialized");
    return RMW_RET_INVALID_ARGUMENT;
  }

  context_data->is_shutdown = true;

  return RMW_RET_OK;
}

rmw_ret_t
rmw_context_fini(rmw_context_t * context)
{
  RMW_CHECK_ARGUMENT_FOR_NULL(context, RMW_RET_INVALID_ARGUMENT);

  if (context->implementation_identifier == nullptr) {
    RMW_SET_ERROR_MSG("context is not initialized");
    return RMW_RET_INVALID_ARGUMENT;
  }

  if (context->implementation_identifier != rmw_int2dds_cpp::implementation_identifier) {
    RMW_SET_ERROR_MSG("context not from this implementation");
    return RMW_RET_INCORRECT_RMW_IMPLEMENTATION;
  }

  auto * context_data = reinterpret_cast<rmw_int2dds_cpp::ContextData *>(context->impl);
  if (context_data == nullptr) {
    RMW_SET_ERROR_MSG("context not initialized");
    return RMW_RET_INVALID_ARGUMENT;
  }

  if (!context_data->is_shutdown) {
    RMW_SET_ERROR_MSG("context not shut down");
    return RMW_RET_INVALID_ARGUMENT;
  }

  // Tear down node-graph discovery first (joins listener thread, releases the
  // ros_discovery_info entities) while the participant is still alive.
  rmw_int2dds_cpp::fini_discovery(context_data);

  // Clean up DDS entities
  if (context_data->graph_guard_condition != nullptr) {
    int2dds_guard_condition_delete(context_data->graph_guard_condition);
  }

  if (context_data->default_subscriber != nullptr) {
    int2dds_delete_subscriber(context_data->default_subscriber);
  }

  if (context_data->default_publisher != nullptr) {
    int2dds_delete_publisher(context_data->default_publisher);
  }

  if (context_data->participant != nullptr) {
    // Delete any entities still attached to the participant (e.g. a node's
    // built-in parameter services / rosout publisher) before tearing it down.
    // int2dds_delete_participant refuses with PRECONDITION_NOT_MET and orphans
    // the participant if non-builtin entities remain, leaking that participant's
    // sockets, eventfds and epoll instances. A context reclaimed by the client
    // library's garbage collector (rather than an explicit destroy) can reach
    // here with those entities still attached.
    int2dds_participant_delete_contained_entities(context_data->participant);
    int2dds_delete_participant(context_data->participant);
  }

  if (context_data->factory != nullptr) {
    int2dds_domain_participant_factory_finalize(context_data->factory);
  }

  delete context_data;

  std::ignore = rmw_init_options_fini(&context->options);

  *context = rmw_get_zero_initialized_context();

  return RMW_RET_OK;
}
}  // extern "C"
