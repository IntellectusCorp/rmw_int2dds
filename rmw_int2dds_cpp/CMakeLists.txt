cmake_minimum_required(VERSION 3.14)
project(rmw_int2dds_cpp)

# C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The matched-event smoke executable exercises APIs that are not present in
# ROS 2 Humble's rmw headers. Keep that target opt-in on Humble while building
# the Humble-compatible event validation target normally.
if("$ENV{ROS_DISTRO}" STREQUAL "humble")
  set(RMW_INT2DDS_BUILD_MATCHED_EVENT_TESTS_DEFAULT OFF)
else()
  set(RMW_INT2DDS_BUILD_MATCHED_EVENT_TESTS_DEFAULT ON)
endif()
option(
  RMW_INT2DDS_BUILD_EVENT_VALIDATION
  "Build RMW event validation executable"
  ON)
option(
  RMW_INT2DDS_BUILD_MATCHED_EVENT_TESTS
  "Build matched-event smoke executable that depends on newer RMW matched-event APIs"
  ${RMW_INT2DDS_BUILD_MATCHED_EVENT_TESTS_DEFAULT})

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
# Optional: dynamic message type support (C3). Present on Iron+; absent on Humble.
find_package(rosidl_dynamic_typesupport QUIET)
find_package(rmw_dds_common REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)

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

target_link_libraries(rmw_int2dds_cpp
  int2dds_ffi::int2dds_ffi
  ${PLATFORM_LIBS}
)

ament_target_dependencies(rmw_int2dds_cpp
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

# Configure as RMW library
# Dynamic message type support (C3): link when available (Iron+).
if(rosidl_dynamic_typesupport_FOUND)
  target_link_libraries(rmw_int2dds_cpp
    rosidl_dynamic_typesupport::rosidl_dynamic_typesupport)
endif()

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
# C3 foundation check: int2dds dynamic type build + primitive sample read.
add_executable(test_dynamic_message test/test_dynamic_message.cpp)
target_include_directories(test_dynamic_message PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(test_dynamic_message
  rclcpp::rclcpp
  ${std_msgs_TARGETS}
  int2dds_ffi::int2dds_ffi
  rmw_int2dds_cpp
)
if(rosidl_dynamic_typesupport_FOUND)
  target_link_libraries(test_dynamic_message
    rosidl_dynamic_typesupport::rosidl_dynamic_typesupport)
endif()
find_package(geometry_msgs QUIET)
if(geometry_msgs_FOUND)
  target_link_libraries(test_dynamic_message ${geometry_msgs_TARGETS})
endif()
find_package(test_msgs QUIET)
if(test_msgs_FOUND)
  target_link_libraries(test_dynamic_message ${test_msgs_TARGETS})
endif()
find_package(diagnostic_msgs QUIET)
if(diagnostic_msgs_FOUND)
  target_link_libraries(test_dynamic_message ${diagnostic_msgs_TARGETS})
endif()

# C3 full-flow check: build the dynamic type from a generated TypeDescription (the ROS
# runtime path) then deserialize and read.
if(rosidl_dynamic_typesupport_FOUND AND test_msgs_FOUND)
  add_executable(test_dynamic_fullflow test/test_dynamic_fullflow.cpp)
  target_include_directories(test_dynamic_fullflow PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
  target_link_libraries(test_dynamic_fullflow
    rclcpp::rclcpp
    int2dds_ffi::int2dds_ffi
    rmw_int2dds_cpp
    rosidl_dynamic_typesupport::rosidl_dynamic_typesupport
    ${test_msgs_TARGETS}
  )
endif()

# C3 receive-path check: publish then pull back via rmw_take_dynamic_message.
find_package(rcl QUIET)
if(rosidl_dynamic_typesupport_FOUND AND diagnostic_msgs_FOUND AND rcl_FOUND)
  add_executable(test_dynamic_take test/test_dynamic_take.cpp)
  target_include_directories(test_dynamic_take PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
  target_link_libraries(test_dynamic_take
    rclcpp::rclcpp
    rcl::rcl
    int2dds_ffi::int2dds_ffi
    rmw_int2dds_cpp
    rosidl_dynamic_typesupport::rosidl_dynamic_typesupport
    ${diagnostic_msgs_TARGETS}
  )
endif()


# D4: batch take_sequence functional check.
if(rcl_FOUND)
  add_executable(test_take_sequence test/test_take_sequence.cpp)
  target_include_directories(test_take_sequence PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
  target_link_libraries(test_take_sequence
    rclcpp::rclcpp
    rcl::rcl
    ${std_msgs_TARGETS}
    rmw_int2dds_cpp
  )
endif()

if(RMW_INT2DDS_BUILD_MATCHED_EVENT_TESTS)
  add_executable(test_event_smoke test/test_event_smoke.cpp)
  target_include_directories(test_event_smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
  target_link_libraries(test_event_smoke rmw_int2dds_cpp int2dds_ffi::int2dds_ffi ${PLATFORM_LIBS})
  ament_target_dependencies(test_event_smoke
    rcutils
    rmw
    rosidl_runtime_cpp
    rosidl_typesupport_cpp
    std_msgs
  )
endif()

if(RMW_INT2DDS_BUILD_EVENT_VALIDATION)
  add_executable(test_event_validation test/test_event_validation.cpp)
  target_include_directories(test_event_validation PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
  target_link_libraries(test_event_validation rmw_int2dds_cpp int2dds_ffi::int2dds_ffi ${PLATFORM_LIBS})
  ament_target_dependencies(test_event_validation
    rcutils
    rmw
    rclcpp
    rosidl_runtime_cpp
    rosidl_typesupport_cpp
    std_msgs
  )
endif()

add_executable(qos_durability_sub_cpp validation/qos/durability/rclcpp/qos_durability_sub.cpp)
target_include_directories(qos_durability_sub_cpp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
ament_target_dependencies(qos_durability_sub_cpp
  rclcpp
  std_msgs
)

add_executable(qos_durability_pub_cpp validation/qos/durability/rclcpp/qos_durability_pub.cpp)
target_include_directories(qos_durability_pub_cpp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
ament_target_dependencies(qos_durability_pub_cpp
  rclcpp
  std_msgs
)

add_executable(qos_history_depth_sub_cpp validation/qos/history_depth/rclcpp/qos_history_depth_sub.cpp)
target_include_directories(qos_history_depth_sub_cpp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
ament_target_dependencies(qos_history_depth_sub_cpp
  rclcpp
  std_msgs
)

add_executable(qos_history_depth_pub_cpp validation/qos/history_depth/rclcpp/qos_history_depth_pub.cpp)
target_include_directories(qos_history_depth_pub_cpp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
ament_target_dependencies(qos_history_depth_pub_cpp
  rclcpp
  std_msgs
)

add_executable(qos_deadline_sub_cpp validation/qos/deadline/rclcpp/qos_deadline_sub.cpp)
target_include_directories(qos_deadline_sub_cpp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
ament_target_dependencies(qos_deadline_sub_cpp
  rclcpp
  std_msgs
)

add_executable(qos_deadline_pub_cpp validation/qos/deadline/rclcpp/qos_deadline_pub.cpp)
target_include_directories(qos_deadline_pub_cpp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
ament_target_dependencies(qos_deadline_pub_cpp
  rclcpp
  std_msgs
)

add_executable(qos_liveliness_sub_cpp validation/qos/liveliness/rclcpp/qos_liveliness_sub.cpp)
target_include_directories(qos_liveliness_sub_cpp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
ament_target_dependencies(qos_liveliness_sub_cpp
  rclcpp
  std_msgs
)

add_executable(qos_liveliness_pub_cpp validation/qos/liveliness/rclcpp/qos_liveliness_pub.cpp)
target_include_directories(qos_liveliness_pub_cpp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
ament_target_dependencies(qos_liveliness_pub_cpp
  rclcpp
  std_msgs
)

add_executable(qos_lifespan_sub_cpp validation/qos/lifespan/rclcpp/qos_lifespan_sub.cpp)
target_include_directories(qos_lifespan_sub_cpp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
ament_target_dependencies(qos_lifespan_sub_cpp
  rclcpp
  std_msgs
)

add_executable(qos_lifespan_pub_cpp validation/qos/lifespan/rclcpp/qos_lifespan_pub.cpp)
target_include_directories(qos_lifespan_pub_cpp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
ament_target_dependencies(qos_lifespan_pub_cpp
  rclcpp
  std_msgs
)

add_executable(content_filter_lifecycle_check_cpp
  validation/cft/rclcpp/content_filter_lifecycle_check.cpp)
target_include_directories(content_filter_lifecycle_check_cpp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
ament_target_dependencies(content_filter_lifecycle_check_cpp
  rclcpp
  std_msgs
)

add_executable(callback_executor_smoke_cpp
  validation/callback/rclcpp/callback_executor_smoke.cpp)
target_include_directories(callback_executor_smoke_cpp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
ament_target_dependencies(callback_executor_smoke_cpp
  rclcpp
)

add_executable(perf_latency_pub_cpp validation/perf/rclcpp/perf_latency_pub_cpp.cpp)
target_include_directories(perf_latency_pub_cpp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
ament_target_dependencies(perf_latency_pub_cpp
  rclcpp
  std_msgs
)

add_executable(perf_latency_sub_cpp validation/perf/rclcpp/perf_latency_sub_cpp.cpp)
target_include_directories(perf_latency_sub_cpp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
ament_target_dependencies(perf_latency_sub_cpp
  rclcpp
  std_msgs
)

add_executable(perf_throughput_pub_cpp validation/perf/rclcpp/perf_throughput_pub_cpp.cpp)
target_include_directories(perf_throughput_pub_cpp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
ament_target_dependencies(perf_throughput_pub_cpp
  rclcpp
  std_msgs
)

add_executable(perf_throughput_sub_cpp validation/perf/rclcpp/perf_throughput_sub_cpp.cpp)
target_include_directories(perf_throughput_sub_cpp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
ament_target_dependencies(perf_throughput_sub_cpp
  rclcpp
  std_msgs
)

add_executable(perf_readiness_pub_cpp validation/perf/rclcpp/perf_readiness_pub_cpp.cpp)
target_include_directories(perf_readiness_pub_cpp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
ament_target_dependencies(perf_readiness_pub_cpp
  rclcpp
  std_msgs
)

add_executable(perf_readiness_sub_cpp validation/perf/rclcpp/perf_readiness_sub_cpp.cpp)
target_include_directories(perf_readiness_sub_cpp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
ament_target_dependencies(perf_readiness_sub_cpp
  rclcpp
  std_msgs
)

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

install(TARGETS qos_durability_sub_cpp
  RUNTIME DESTINATION lib/${PROJECT_NAME}
)

install(TARGETS qos_durability_pub_cpp
  RUNTIME DESTINATION lib/${PROJECT_NAME}
)

install(TARGETS qos_history_depth_sub_cpp
  RUNTIME DESTINATION lib/${PROJECT_NAME}
)

install(TARGETS qos_history_depth_pub_cpp
  RUNTIME DESTINATION lib/${PROJECT_NAME}
)

install(TARGETS qos_deadline_sub_cpp
  RUNTIME DESTINATION lib/${PROJECT_NAME}
)

install(TARGETS qos_deadline_pub_cpp
  RUNTIME DESTINATION lib/${PROJECT_NAME}
)

install(TARGETS qos_liveliness_sub_cpp
  RUNTIME DESTINATION lib/${PROJECT_NAME}
)

install(TARGETS qos_liveliness_pub_cpp
  RUNTIME DESTINATION lib/${PROJECT_NAME}
)

install(TARGETS qos_lifespan_sub_cpp
  RUNTIME DESTINATION lib/${PROJECT_NAME}
)

install(TARGETS qos_lifespan_pub_cpp
  RUNTIME DESTINATION lib/${PROJECT_NAME}
)

install(TARGETS content_filter_lifecycle_check_cpp
  RUNTIME DESTINATION lib/${PROJECT_NAME}
)

install(TARGETS callback_executor_smoke_cpp
  RUNTIME DESTINATION lib/${PROJECT_NAME}
)

install(TARGETS perf_latency_pub_cpp
  RUNTIME DESTINATION lib/${PROJECT_NAME}
)

install(TARGETS perf_latency_sub_cpp
  RUNTIME DESTINATION lib/${PROJECT_NAME}
)

install(TARGETS perf_throughput_pub_cpp
  RUNTIME DESTINATION lib/${PROJECT_NAME}
)

install(TARGETS perf_throughput_sub_cpp
  RUNTIME DESTINATION lib/${PROJECT_NAME}
)

install(TARGETS perf_readiness_pub_cpp
  RUNTIME DESTINATION lib/${PROJECT_NAME}
)

install(TARGETS perf_readiness_sub_cpp
  RUNTIME DESTINATION lib/${PROJECT_NAME}
)

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
    "${CMAKE_CURRENT_SOURCE_DIR}/validation/*"
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

  # ament_lint_auto forwards AMENT_LINT_AUTO_FILE_EXCLUDE to cpplint,
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

  # The checks under test/ are plain executables rather than gtest binaries: each
  # drives the rmw C API (or the FFI directly) and reports through its exit
  # status. Registering them is what gives `colcon test` something to run - until
  # now every one of them was only ever compiled, so nothing but the linters could
  # fail and no runtime regression could be caught.
  #
  # GENERATE_RESULT_FOR_RETURN_CODE_ZERO: these write no JUnit XML of their own,
  # so ament has to synthesise the passing result from the exit status.
  #
  # ROS_DOMAIN_ID separates only the two checks that reach DDS through rclcpp -
  # test_take_sequence and test_dynamic_take - because nothing under src/ reads
  # the variable: test_connection hardcodes domain 0, and the raw rmw_init path
  # folds RMW_DEFAULT_DOMAIN_ID to 0. The remaining checks therefore share domain
  # 0 with each other and with any other domain-0 participant on the network.
  # colcon runs ctest sequentially (it passes no -j), so they do not currently
  # collide - but do not read the setting below as isolation. 71 belongs to the
  # wait-set guard tests on the humble branch; this range stays clear of it.
  find_package(ament_cmake_test REQUIRED)

  function(rmw_int2dds_add_check name domain_id timeout)
    # Several checks are built only when an optional dependency was found.
    if(NOT TARGET ${name})
      message(STATUS "rmw_int2dds_cpp: ${name} not built; skipping test registration")
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
  rmw_int2dds_add_check(test_take_sequence     75 120)
  rmw_int2dds_add_check(test_dynamic_message   76 120)
  rmw_int2dds_add_check(test_dynamic_fullflow  77 120)
  rmw_int2dds_add_check(test_dynamic_take      78 120)
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
  rclcpp
)

ament_package()
