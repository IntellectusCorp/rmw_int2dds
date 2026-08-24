cmake_minimum_required(VERSION 3.14)
project(rmw_int2dds_cpp)

# C++20: platform minimum since ROS 2 Kilted (REP 2000); Jazzy toolchains accept it too.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(
  RMW_INT2DDS_BUILD_EVENT_VALIDATION
  "Build RMW event validation executable"
  ON)
# RMW_INT2DDS_BUILD_MATCHED_EVENT_TESTS is declared further down, after
# find_package(rmw): its default is probed from the installed rmw headers rather
# than guessed from the environment.

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Find ROS2 dependencies
find_package(ament_cmake REQUIRED)
find_package(rcutils REQUIRED)
find_package(rmw REQUIRED)
find_package(rosidl_runtime_c REQUIRED)
find_package(rosidl_runtime_cpp REQUIRED)
find_package(rosidl_typesupport_introspection_c REQUIRED)
find_package(rosidl_typesupport_introspection_cpp REQUIRED)
find_package(rosidl_typesupport_c REQUIRED)
find_package(rosidl_typesupport_cpp REQUIRED)
# Dynamic message type support. Present from Iron on, so REQUIRED here: it used
# to be probed QUIET and undeclared, which let a build-farm job drop it without a
# word and ship a .deb with no dynamic message support at all.
find_package(rosidl_dynamic_typesupport REQUIRED)
find_package(rmw_dds_common REQUIRED)
find_package(std_msgs REQUIRED)

# NOTE: rclcpp and rcl are deliberately absent from this package. They sit ABOVE
# it in the ROS 2 stack:
#
#   rclcpp -> rcl -> rmw_implementation -> rmw_int2dds_cpp
#                    (via the rmw_implementation_packages group)
#
# Depending on either would close a build dependency cycle and drag the whole
# client library stack into an RMW plugin. No upstream RMW implementation does
# it (rmw_fastrtps, rmw_cyclonedds, rmw_connextdds, rmw_zenoh: zero references).
# The rclcpp-based QoS/perf probes live in the sibling rmw_int2dds_validation
# package instead, mirroring test_rmw_zenoh_cpp in ros2/rmw_zenoh.

# The matched-event smoke executable is built entirely on rmw matched-event APIs
# that only exist in newer rmw headers, so ask the headers whether they are there
# - the same question test_event_validation.cpp asks with __has_include.
#
# It must NOT be inferred from $ENV{ROS_DISTRO}: ros_environment is the only
# package that exports that variable, so anywhere it is not installed the
# variable is empty and a distro comparison silently picks whichever branch that
# happens to select. The rmw headers are the thing the code actually depends on,
# so probe those instead.
#
# NO_DEFAULT_PATH keeps the search inside the rmw package that was just found; a
# stray matched.h reachable through CMAKE_PREFIX_PATH must not answer for it.
find_file(RMW_INT2DDS_MATCHED_EVENT_HEADER
  NAMES rmw/events_statuses/matched.h
  HINTS ${rmw_INCLUDE_DIRS}
  NO_DEFAULT_PATH)
mark_as_advanced(RMW_INT2DDS_MATCHED_EVENT_HEADER)
if(RMW_INT2DDS_MATCHED_EVENT_HEADER)
  set(RMW_INT2DDS_BUILD_MATCHED_EVENT_TESTS_DEFAULT ON)
  message(STATUS "rmw matched-event API found: ${RMW_INT2DDS_MATCHED_EVENT_HEADER}")
else()
  set(RMW_INT2DDS_BUILD_MATCHED_EVENT_TESTS_DEFAULT OFF)
  message(STATUS
    "rmw matched-event API not available (searched: ${rmw_INCLUDE_DIRS}) - "
    "the matched-event smoke target is disabled for this rmw version.")
endif()
option(
  RMW_INT2DDS_BUILD_MATCHED_EVENT_TESTS
  "Build matched-event smoke executable that depends on newer RMW matched-event APIs"
  ${RMW_INT2DDS_BUILD_MATCHED_EVENT_TESTS_DEFAULT})

# ============================================================================
# int2dds-ffi library
# ----------------------------------------------------------------------------
# The prebuilt int2DDS FFI library is provided by the int2dds_ffi_vendor
# package, which downloads the artifact for this platform and exposes the
# int2dds_ffi::int2dds_ffi imported target (include dir + shared library).
# ============================================================================
find_package(int2dds_ffi_vendor REQUIRED)

if(WIN32)
  set(PLATFORM_LIBS ws2_32 ntdll userenv iphlpapi ole32)
elseif(APPLE)
  set(PLATFORM_LIBS pthread)
else()  # Linux
  set(PLATFORM_LIBS pthread dl)
endif()

# ============================================================================
# RMW Library
# ============================================================================
add_library(rmw_int2dds_cpp SHARED
  # Main entry point
  src/rmw_int2dds.cpp

  # Initialization
  src/init/rmw_init.cpp

  # Node management
  src/node/rmw_node.cpp

  # Publisher
  src/publisher/rmw_publisher.cpp
  src/publisher/rmw_publish.cpp

  # Subscription
  src/subscription/rmw_subscription.cpp
  src/subscription/rmw_take.cpp

  # Guard condition
  src/guard_condition/rmw_guard_condition.cpp

  # Wait set
  src/wait/rmw_wait.cpp
  src/wait/rmw_wait_set.cpp
  src/wait/waitset_registry.cpp

  # Service
  src/service/rmw_service.cpp
  src/service/rmw_service_request.cpp

  # Client
  src/client/rmw_client.cpp
  src/client/rmw_client_request.cpp

  # Event
  src/event/rmw_event.cpp

  # Graph
  src/graph/rmw_graph.cpp
  src/graph/discovery.cpp

  # QoS
  src/qos/rmw_qos.cpp

  # Serialization
  src/serialization/rmw_serialize.cpp
  src/serialization/dynamic_serialization_support.cpp
  src/serialization/cdr_serializer.cpp

  # Common utilities
  src/common/error_handling.cpp
  src/common/gid.cpp
  src/common/listeners.cpp
  src/common/take_with_info.cpp
  src/common/topic_names.cpp
)

target_compile_definitions(rmw_int2dds_cpp PRIVATE "RMW_INT2DDS_CPP_BUILDING_DLL")

target_include_directories(rmw_int2dds_cpp PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

# ament_target_dependencies() was removed from ament_cmake in ROS 2 Lyrical
# (ament_cmake 2.8.x no longer defines it), so link the namespaced targets
# directly - they carry their own include directories and transitive deps.
target_link_libraries(rmw_int2dds_cpp PUBLIC
  int2dds_ffi::int2dds_ffi
  rosidl_dynamic_typesupport::rosidl_dynamic_typesupport
  ${PLATFORM_LIBS}
  rcutils::rcutils
  rmw::rmw
  rosidl_runtime_c::rosidl_runtime_c
  rosidl_runtime_cpp::rosidl_runtime_cpp
  rosidl_typesupport_introspection_c::rosidl_typesupport_introspection_c
  rosidl_typesupport_introspection_cpp::rosidl_typesupport_introspection_cpp
  rosidl_typesupport_c::rosidl_typesupport_c
  rosidl_typesupport_cpp::rosidl_typesupport_cpp
  rmw_dds_common::rmw_dds_common_library
)

# Configure as RMW library
configure_rmw_library(rmw_int2dds_cpp)

# Register RMW implementation
register_rmw_implementation(
  "c:rosidl_typesupport_c:rosidl_typesupport_introspection_c"
  "cpp:rosidl_typesupport_cpp:rosidl_typesupport_introspection_cpp"
)

# ============================================================================
# Test connection executable (for development testing)
# ============================================================================
add_executable(test_connection test/test_connection.cpp)
target_include_directories(test_connection PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(test_connection int2dds_ffi::int2dds_ffi ${PLATFORM_LIBS})

# D4: the batch take_sequence functional check needs rclcpp and rcl, so it lives
# in the rmw_int2dds_validation package.

if(RMW_INT2DDS_BUILD_MATCHED_EVENT_TESTS)
  add_executable(test_event_smoke test/test_event_smoke.cpp)
  target_include_directories(test_event_smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
  target_link_libraries(test_event_smoke
    rmw_int2dds_cpp
    int2dds_ffi::int2dds_ffi
    ${PLATFORM_LIBS}
    rcutils::rcutils
    rmw::rmw
    rosidl_runtime_cpp::rosidl_runtime_cpp
    rosidl_typesupport_cpp::rosidl_typesupport_cpp
    ${std_msgs_TARGETS}
  )
endif()

if(RMW_INT2DDS_BUILD_EVENT_VALIDATION)
  add_executable(test_event_validation test/test_event_validation.cpp)
  target_include_directories(test_event_validation PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
  # test_event_validation.cpp drives the rmw C API directly - it includes no
  # rclcpp header, so rclcpp was never a real dependency of this target.
  target_link_libraries(test_event_validation
    rmw_int2dds_cpp
    int2dds_ffi::int2dds_ffi
    ${PLATFORM_LIBS}
    rcutils::rcutils
    rmw::rmw
    rosidl_runtime_cpp::rosidl_runtime_cpp
    rosidl_typesupport_cpp::rosidl_typesupport_cpp
    ${std_msgs_TARGETS}
  )
endif()

# ============================================================================
# Installation
# ============================================================================
install(TARGETS rmw_int2dds_cpp
  EXPORT export_rmw_int2dds_cpp
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

install(DIRECTORY include/
  DESTINATION include
)

install(TARGETS test_connection
  RUNTIME DESTINATION lib/${PROJECT_NAME}
)

if(RMW_INT2DDS_BUILD_MATCHED_EVENT_TESTS)
  install(TARGETS test_event_smoke
    RUNTIME DESTINATION lib/${PROJECT_NAME}
  )
endif()

if(RMW_INT2DDS_BUILD_EVENT_VALIDATION)
  install(TARGETS test_event_validation
    RUNTIME DESTINATION lib/${PROJECT_NAME}
  )
endif()

# The int2dds FFI runtime library is installed by the int2dds_ffi_vendor
# package. On Windows, copy the DLL next to the freshly built binaries so the
# development/test executables can locate it at runtime.
if(WIN32)
  add_custom_command(TARGET rmw_int2dds_cpp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:int2dds_ffi::int2dds_ffi> $<TARGET_FILE_DIR:rmw_int2dds_cpp>
    COMMENT "Copying int2dds_ffi.dll to build directory")

  add_custom_command(TARGET test_connection POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:int2dds_ffi::int2dds_ffi> $<TARGET_FILE_DIR:test_connection>
    COMMENT "Copying int2dds_ffi.dll for test_connection")
endif()

# ============================================================================
# Testing
# ============================================================================
if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  find_package(ament_cmake_cpplint REQUIRED)
  find_package(ament_cmake_flake8 REQUIRED)
  file(GLOB_RECURSE RMW_INT2DDS_LINT_EXCLUDE_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/doc/*"
    "${CMAKE_CURRENT_SOURCE_DIR}/examples/*"
    "${CMAKE_CURRENT_SOURCE_DIR}/ros2_documentation_pr/*"
  )
  list(APPEND RMW_INT2DDS_LINT_EXCLUDE_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/CHANGELOG.rst"
    "${CMAKE_CURRENT_SOURCE_DIR}/CLA-Corporate.md"
    "${CMAKE_CURRENT_SOURCE_DIR}/CLA-Individual.md"
    "${CMAKE_CURRENT_SOURCE_DIR}/CODE_OF_CONDUCT.md"
    "${CMAKE_CURRENT_SOURCE_DIR}/CONTRIBUTING.md"
    "${CMAKE_CURRENT_SOURCE_DIR}/DOCUMENTATION.md"
    "${CMAKE_CURRENT_SOURCE_DIR}/NOTICE"
    "${CMAKE_CURRENT_SOURCE_DIR}/QUALITY_DECLARATION.md"
    "${CMAKE_CURRENT_SOURCE_DIR}/README.md"
    "${CMAKE_CURRENT_SOURCE_DIR}/SECURITY.md"
    "${CMAKE_CURRENT_SOURCE_DIR}/STRUCTURE.md"
    "${CMAKE_CURRENT_SOURCE_DIR}/THIRD_PARTY_LICENSES.md"
    "${CMAKE_CURRENT_SOURCE_DIR}/TRADEMARK_POLICY.md"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/int2dds-ffi.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/test/test_connection.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/test/test_event_smoke.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/test/test_event_validation.cpp"
  )
  set(AMENT_LINT_AUTO_FILE_EXCLUDE
    ${RMW_INT2DDS_LINT_EXCLUDE_FILES}
  )
  set(AMENT_LINT_AUTO_EXCLUDE
    ament_cmake_cpplint
    ament_cmake_flake8
  )
  ament_lint_auto_find_test_dependencies()

  # Humble's ament_lint_auto forwards AMENT_LINT_AUTO_FILE_EXCLUDE to cpplint,
  # copyright and uncrustify, but not to flake8. Keep the package lint targets
  # explicit so generated/runtime validation assets do not fail release lint.
  ament_flake8(--exclude "${RMW_INT2DDS_LINT_EXCLUDE_FILES}")
  # Lint only the paths we own. Passing explicit paths (before FILTERS) works
  # identically on every ament_cmake_cpplint version, unlike EXCLUDE which is
  # single-value on Humble/Jazzy and silently turns the exclude list into
  # positional lint targets. include/ is narrowed to our namespace so a
  # vendored include/int2dds-ffi.h can never enter the lint scope.
  ament_cpplint(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/rmw_int2dds_cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    FILTERS
      -build/include_order
      -build/include_subdir
      -legal/copyright
      -readability/braces
      -runtime/int
      -whitespace/blank_line
      -whitespace/comments
      -whitespace/line_length
  )

  # Wait set unit tests. rmw_create_wait_set and rmw_create_guard_condition
  # only validate the context implementation identifier, so these run without a
  # DDS participant: no discovery, no sockets, no traffic, and no flakiness
  # from either.
  find_package(ament_cmake_gtest REQUIRED)
  ament_add_gtest(test_rmw_wait_guards test/test_rmw_wait_guards.cpp
    TIMEOUT 120
    ENV RMW_IMPLEMENTATION=rmw_int2dds_cpp ROS_DOMAIN_ID=71
  )
  if(TARGET test_rmw_wait_guards)
    target_include_directories(test_rmw_wait_guards PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(test_rmw_wait_guards
      rmw_int2dds_cpp
      int2dds_ffi::int2dds_ffi
      ${PLATFORM_LIBS}
      rmw::rmw
      rcutils::rcutils
    )
    if(WIN32)
      add_custom_command(TARGET test_rmw_wait_guards POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:int2dds_ffi::int2dds_ffi> $<TARGET_FILE_DIR:test_rmw_wait_guards>
        COMMENT "Copying int2dds_ffi.dll for test_rmw_wait_guards")
    endif()
  endif()

  # The remaining checks under test/ are plain executables rather than gtest
  # binaries: each drives the rmw C API (or the FFI directly) and reports through
  # its exit status. Registering them is what gives `colcon test` something to
  # run beyond the linters and test_rmw_wait_guards.
  #
  # GENERATE_RESULT_FOR_RETURN_CODE_ZERO: these write no JUnit XML of their own,
  # so ament has to synthesise the passing result from the exit status.
  #
  # ROS_DOMAIN_ID does not actually separate them - nothing under src/ reads the
  # variable, test_connection hardcodes domain 0 and the raw rmw_init path folds
  # RMW_DEFAULT_DOMAIN_ID to 0 - so do not read it as isolation. It is set for
  # the day those paths honour it. 71 belongs to test_rmw_wait_guards above and
  # 81..84 to rmw_int2dds_validation.
  find_package(ament_cmake_test REQUIRED)

  function(rmw_int2dds_add_check name domain_id timeout)
    # test_event_smoke is built only when the matched-event API is present.
    if(NOT TARGET ${name})
      message(WARNING "rmw_int2dds_cpp: ${name} not built; skipping test registration")
      return()
    endif()
    ament_add_test(${name}
      COMMAND "$<TARGET_FILE:${name}>"
      TIMEOUT ${timeout}
      GENERATE_RESULT_FOR_RETURN_CODE_ZERO
      ENV RMW_IMPLEMENTATION=rmw_int2dds_cpp ROS_DOMAIN_ID=${domain_id}
    )
  endfunction()

  rmw_int2dds_add_check(test_connection        72 60)
  rmw_int2dds_add_check(test_event_validation  73 180)
  rmw_int2dds_add_check(test_event_smoke       74 120)
endif()

# ============================================================================
# Export
# ============================================================================
ament_export_include_directories(include)
ament_export_libraries(rmw_int2dds_cpp)
ament_export_targets(export_rmw_int2dds_cpp)
ament_export_dependencies(
  int2dds_ffi_vendor
  rcutils
  rmw
  rosidl_runtime_c
  rosidl_runtime_cpp
  rosidl_typesupport_introspection_c
  rosidl_typesupport_introspection_cpp
  rosidl_typesupport_c
  rosidl_typesupport_cpp
  rmw_dds_common
)

ament_package()
