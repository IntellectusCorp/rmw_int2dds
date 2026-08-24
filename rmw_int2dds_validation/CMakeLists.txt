cmake_minimum_required(VERSION 3.14)
project(rmw_int2dds_validation)

# C++20: platform minimum since ROS 2 Kilted (REP 2000); Jazzy toolchains accept it too.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# These probes are the reason this package exists separately from
# rmw_int2dds_cpp: they need rclcpp, and an RMW implementation cannot depend on
# rclcpp without closing a build dependency cycle. See the layering note in
# rmw_int2dds_cpp/CMakeLists.txt.
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rcl REQUIRED)
find_package(rmw REQUIRED)
find_package(rcutils REQUIRED)
find_package(std_msgs REQUIRED)
find_package(rmw_int2dds_cpp REQUIRED)

# ============================================================================
# Probes
# ----------------------------------------------------------------------------
# Every probe is a standalone rclcpp node that selects the middleware at run
# time through RMW_IMPLEMENTATION, so none of them link rmw_int2dds_cpp and all
# of them build and install identically.
# ============================================================================
# ament_target_dependencies() was removed from ament_cmake in ROS 2 Lyrical
# (ament_cmake 2.8.x no longer defines it), so every probe links the namespaced
# targets directly. Callers pass CMake targets, not package names.
function(add_probe target source)
  add_executable(${target} ${source})
  target_link_libraries(${target} ${ARGN})
  install(TARGETS ${target}
    RUNTIME DESTINATION lib/${PROJECT_NAME}
  )
endfunction()

# QoS: durability
add_probe(qos_durability_sub_cpp
  validation/qos/durability/rclcpp/qos_durability_sub.cpp rclcpp::rclcpp ${std_msgs_TARGETS})
add_probe(qos_durability_pub_cpp
  validation/qos/durability/rclcpp/qos_durability_pub.cpp rclcpp::rclcpp ${std_msgs_TARGETS})

# QoS: history depth
add_probe(qos_history_depth_sub_cpp
  validation/qos/history_depth/rclcpp/qos_history_depth_sub.cpp rclcpp::rclcpp ${std_msgs_TARGETS})
add_probe(qos_history_depth_pub_cpp
  validation/qos/history_depth/rclcpp/qos_history_depth_pub.cpp rclcpp::rclcpp ${std_msgs_TARGETS})

# QoS: deadline
add_probe(qos_deadline_sub_cpp
  validation/qos/deadline/rclcpp/qos_deadline_sub.cpp rclcpp::rclcpp ${std_msgs_TARGETS})
add_probe(qos_deadline_pub_cpp
  validation/qos/deadline/rclcpp/qos_deadline_pub.cpp rclcpp::rclcpp ${std_msgs_TARGETS})

# QoS: liveliness
add_probe(qos_liveliness_sub_cpp
  validation/qos/liveliness/rclcpp/qos_liveliness_sub.cpp rclcpp::rclcpp ${std_msgs_TARGETS})
add_probe(qos_liveliness_pub_cpp
  validation/qos/liveliness/rclcpp/qos_liveliness_pub.cpp rclcpp::rclcpp ${std_msgs_TARGETS})

# QoS: lifespan
add_probe(qos_lifespan_sub_cpp
  validation/qos/lifespan/rclcpp/qos_lifespan_sub.cpp rclcpp::rclcpp ${std_msgs_TARGETS})
add_probe(qos_lifespan_pub_cpp
  validation/qos/lifespan/rclcpp/qos_lifespan_pub.cpp rclcpp::rclcpp ${std_msgs_TARGETS})

# Content-filtered topics
add_probe(content_filter_lifecycle_check_cpp
  validation/cft/rclcpp/content_filter_lifecycle_check.cpp rclcpp::rclcpp ${std_msgs_TARGETS})

# Executor callbacks - publishes nothing, so no std_msgs here.
add_probe(callback_executor_smoke_cpp
  validation/callback/rclcpp/callback_executor_smoke.cpp rclcpp::rclcpp)

# Performance: latency / throughput / readiness
add_probe(perf_latency_pub_cpp
  validation/perf/rclcpp/perf_latency_pub_cpp.cpp rclcpp::rclcpp ${std_msgs_TARGETS})
add_probe(perf_latency_sub_cpp
  validation/perf/rclcpp/perf_latency_sub_cpp.cpp rclcpp::rclcpp ${std_msgs_TARGETS})
add_probe(perf_throughput_pub_cpp
  validation/perf/rclcpp/perf_throughput_pub_cpp.cpp rclcpp::rclcpp ${std_msgs_TARGETS})
add_probe(perf_throughput_sub_cpp
  validation/perf/rclcpp/perf_throughput_sub_cpp.cpp rclcpp::rclcpp ${std_msgs_TARGETS})
add_probe(perf_readiness_pub_cpp
  validation/perf/rclcpp/perf_readiness_pub_cpp.cpp rclcpp::rclcpp ${std_msgs_TARGETS})
add_probe(perf_readiness_sub_cpp
  validation/perf/rclcpp/perf_readiness_sub_cpp.cpp rclcpp::rclcpp ${std_msgs_TARGETS})

# ============================================================================
# D4: batch take_sequence functional check
# ----------------------------------------------------------------------------
# Unlike the probes above this one links the middleware directly instead of
# going through rmw_implementation, so it needs rmw_int2dds_cpp at build time.
# It was not installed while it lived in rmw_int2dds_cpp; that is preserved
# here, so run it from the build tree.
#
# It also used to be guarded by `if(rcl_FOUND)` because rcl was probed with
# find_package(rcl QUIET) and never declared in package.xml. On the ROS build
# farm, which installs only declared rosdep keys, that guard silently dropped
# the target. rcl is a declared dependency of this package, so the check now
# builds everywhere.
# ============================================================================
add_executable(test_take_sequence test/test_take_sequence.cpp)
target_link_libraries(test_take_sequence
  rclcpp::rclcpp
  rcl::rcl
  rmw::rmw
  rcutils::rcutils
  ${std_msgs_TARGETS}
  rmw_int2dds_cpp::rmw_int2dds_cpp
)

# ============================================================================
# C3: dynamic message type support checks (Jazzy and newer)
# ----------------------------------------------------------------------------
# These live here rather than in rmw_int2dds_cpp because each one includes
# rclcpp headers (serialization.hpp / serialized_message.hpp / rclcpp.hpp), and
# an RMW implementation cannot depend on rclcpp - see the layering note above.
#
# Every dependency is REQUIRED, not probed QUIET. They used to be optional in
# rmw_int2dds_cpp, which meant a build-farm job that did not install them just
# dropped these targets and still reported success - so a released .deb could
# ship with no dynamic message coverage at all and nothing would say so.
# ============================================================================
find_package(rosidl_dynamic_typesupport REQUIRED)
find_package(rosidl_runtime_c REQUIRED)
find_package(rosidl_typesupport_cpp REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(diagnostic_msgs REQUIRED)
find_package(test_msgs REQUIRED)
find_package(int2dds_ffi_vendor REQUIRED)

function(add_dynamic_check target)
  add_executable(${target} test/${target}.cpp)
  target_link_libraries(${target}
    rclcpp::rclcpp
    int2dds_ffi::int2dds_ffi
    rosidl_dynamic_typesupport::rosidl_dynamic_typesupport
    ${ARGN}
    rmw::rmw
    rcutils::rcutils
    rmw_int2dds_cpp::rmw_int2dds_cpp
  )
endfunction()

add_dynamic_check(test_dynamic_message
  ${std_msgs_TARGETS} ${geometry_msgs_TARGETS} ${test_msgs_TARGETS}
  ${diagnostic_msgs_TARGETS})
add_dynamic_check(test_dynamic_fullflow ${test_msgs_TARGETS})
add_dynamic_check(test_dynamic_take ${diagnostic_msgs_TARGETS})
target_link_libraries(test_dynamic_take rcl::rcl)
target_link_libraries(test_dynamic_fullflow
  rosidl_runtime_c::rosidl_runtime_c
  rosidl_typesupport_cpp::rosidl_typesupport_cpp)

# ============================================================================
# Register the checks so `colcon test` runs them
# ----------------------------------------------------------------------------
# These are plain executables that report through their exit status, so ament
# has to synthesise the passing result from the return code. Each gets its own
# ROS_DOMAIN_ID; 71..78 belong to the checks in rmw_int2dds_cpp.
# ============================================================================
if(BUILD_TESTING)
  find_package(ament_cmake_test REQUIRED)

  function(add_validation_check target domain_id)
    ament_add_test(${target}
      COMMAND "$<TARGET_FILE:${target}>"
      TIMEOUT 120
      GENERATE_RESULT_FOR_RETURN_CODE_ZERO
      ENV RMW_IMPLEMENTATION=rmw_int2dds_cpp ROS_DOMAIN_ID=${domain_id}
    )
  endfunction()

  # colcon runs packages in parallel, so these must not share a domain with the
  # checks in rmw_int2dds_cpp (71..78) either.
  add_validation_check(test_take_sequence    81)
  add_validation_check(test_dynamic_message  82)
  add_validation_check(test_dynamic_fullflow 83)
  add_validation_check(test_dynamic_take     84)

  # These checks print "SKIP:" and still exit 0 when a message package is absent,
  # so a missing dependency would read as a pass. Every find_package above is
  # REQUIRED, which should make that unreachable; this catches it if one is ever
  # relaxed. "BLOCKED-UPSTREAM" is deliberately not matched - that one reports a
  # real rosidl limitation, not a gap in this package.
  set_tests_properties(
    test_dynamic_message test_dynamic_fullflow test_dynamic_take
    PROPERTIES FAIL_REGULAR_EXPRESSION "SKIP:")
endif()

# NOTE: no ament_lint_auto here. validation/* was excluded from every linter in
# rmw_int2dds_cpp (AMENT_LINT_AUTO_FILE_EXCLUDE), and these are the same files;
# enabling lint now would fail on sources that have never been linted. Add it
# deliberately, in its own change, once the sources are cleaned up.

ament_package()
