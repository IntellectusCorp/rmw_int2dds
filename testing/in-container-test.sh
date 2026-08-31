#!/usr/bin/env bash
# Runs inside ros:<distro>-ros-base. Builds the int2DDS RMW, then builds and
# runs the upstream RMW/communication suites against it.
#
# Two colcon workspaces on purpose: test_rmw_implementation and test_communication
# enumerate the available RMW implementations at CMake *configure* time
# (call_for_each_rmw_implementation / get_available_rmw_implementations), so
# rmw_int2dds_cpp has to be installed and on AMENT_PREFIX_PATH before the test
# workspace is configured. One combined build would configure them too early and
# silently produce a matrix without int2DDS in it.
set -euo pipefail

: "${ROS_DISTRO:?ROS_DISTRO must be set}"
export DEBIAN_FRONTEND=noninteractive
export LANG=C.UTF-8
export CTEST_OUTPUT_ON_FAILURE=1

OUT=/ws/out
step() { echo; echo "########## $* ##########"; date -Is; }
# ROS setup scripts touch unbound variables.
ros_source() { set +u; . "$1"; set -u; }

trap 'chown -R "${HOST_UID:-0}:${HOST_GID:-0}" /ws 2>/dev/null || true' EXIT

step "apt dependencies"
apt-get update -qq
apt-get install -y -qq --no-install-recommends \
  git wget ca-certificates \
  build-essential cmake \
  python3-colcon-common-extensions python3-rosdep python3-vcstool \
  "ros-${ROS_DISTRO}-rmw-fastrtps-cpp" \
  "ros-${ROS_DISTRO}-rmw-cyclonedds-cpp" \
  "ros-${ROS_DISTRO}-demo-nodes-cpp" \
  "ros-${ROS_DISTRO}-demo-nodes-py"

ros_source "/opt/ros/${ROS_DISTRO}/setup.bash"
echo "rmw version: $(dpkg-query -W -f='${Version}' "ros-${ROS_DISTRO}-rmw" 2>/dev/null || echo unknown)" | tee "${OUT}/env.txt"
{
  echo "ros_distro: ${ROS_DISTRO}"
  echo "image_os:   $(. /etc/os-release && echo "${PRETTY_NAME}")"
  echo "arch:       $(uname -m)"
  echo "date:       $(date -Is)"
  dpkg-query -W -f='${Package} ${Version}\n' "ros-${ROS_DISTRO}-rmw" \
    "ros-${ROS_DISTRO}-rmw-implementation" "ros-${ROS_DISTRO}-rmw-fastrtps-cpp" \
    "ros-${ROS_DISTRO}-rmw-cyclonedds-cpp" 2>/dev/null || true
} >> "${OUT}/env.txt"

step "clone upstream test suites @ ${ROS_DISTRO}"
cd /ws/test_ws/src
[ -d rmw_implementation ] || git clone -q --depth 1 -b "${ROS_DISTRO}" https://github.com/ros2/rmw_implementation.git
[ -d system_tests ]       || git clone -q --depth 1 -b "${ROS_DISTRO}" https://github.com/ros2/system_tests.git
# rmw_implementation itself is the apt one; only its test package is wanted here.
rm -rf rmw_implementation/rmw_implementation
for d in rmw_implementation/* system_tests/*; do [ -d "$d" ] && echo "  src: $d"; done

step "rosdep"
rosdep update --rosdistro "${ROS_DISTRO}" -q 2>&1 | tail -3 || true
# connext/zenoh keys have no apt package here; their test rows simply do not
# get generated. Unresolved keys are reported, not fatal.
rosdep install --from-paths /ws/rmw_ws/src /ws/test_ws/src --ignore-src -y -r \
  --rosdistro "${ROS_DISTRO}" \
  --skip-keys "rti-connext-dds-6.0.1 rti-connext-dds-7.3.0 rmw_connextdds rmw_zenoh_cpp" \
  2>&1 | tail -20 || echo "!! rosdep reported unresolved keys (continuing)"

step "stage 1: build rmw_int2dds_cpp"
cd /ws/rmw_ws
colcon build \
  --packages-up-to rmw_int2dds_cpp rmw_int2dds_validation \
  --cmake-args -DCMAKE_BUILD_TYPE=Release \
  --event-handlers console_cohesion+
ros_source /ws/rmw_ws/install/setup.bash

step "stage 1: in-repo tests (gtest + ament_lint)"
colcon test --packages-select rmw_int2dds_cpp rmw_int2dds_validation \
  --executor sequential --event-handlers console_cohesion+ || true
colcon test-result --all --verbose > "${OUT}/results-rmw_ws.txt" 2>&1 || true
tail -20 "${OUT}/results-rmw_ws.txt"

step "smoke: talker / listener over rmw_int2dds_cpp"
(
  export RMW_IMPLEMENTATION=rmw_int2dds_cpp
  export ROS_DOMAIN_ID=42
  timeout 25 ros2 run demo_nodes_cpp talker > "${OUT}/smoke-talker.log" 2>&1 &
  tpid=$!
  timeout 20 ros2 run demo_nodes_cpp listener > "${OUT}/smoke-listener.log" 2>&1 || true
  wait "$tpid" 2>/dev/null || true
)
if grep -q "I heard" "${OUT}/smoke-listener.log"; then
  echo "SMOKE PASS: $(grep -c 'I heard' "${OUT}/smoke-listener.log") messages received"
else
  echo "!! SMOKE FAIL: listener received nothing"; tail -20 "${OUT}/smoke-listener.log"
fi

step "RMW implementations visible to CMake"
# get_available_rmw_implementations() reads the ament index "rmw_typesupport"
# resource, not a resource named after rmw_implementation. Listing the wrong
# name prints nothing and looks like "no RMWs found".
for prefix in "/opt/ros/${ROS_DISTRO}" /ws/rmw_ws/install/*; do
  d="${prefix}/share/ament_index/resource_index/rmw_typesupport"
  [ -d "$d" ] && ls "$d" | sed "s|^|  ${prefix##*/}: |"
done

step "stage 2: build upstream test workspace"
cd /ws/test_ws
colcon build --cmake-args -DCMAKE_BUILD_TYPE=Release --event-handlers console_cohesion+

step "stage 2: run upstream suites (sequential — DDS tests share the network)"
ros_source /ws/test_ws/install/setup.bash
colcon test --executor sequential --event-handlers console_cohesion+ || true
colcon test-result --all --verbose > "${OUT}/results-test_ws.txt" 2>&1 || true
tail -40 "${OUT}/results-test_ws.txt"

step "collect xunit XML"
mkdir -p "${OUT}/xunit"
for ws in rmw_ws test_ws; do
  for d in /ws/${ws}/build/*/test_results; do
    [ -d "$d" ] || continue
    pkg="$(basename "$(dirname "$d")")"
    mkdir -p "${OUT}/xunit/${ws}/${pkg}"
    cp -r "$d/." "${OUT}/xunit/${ws}/${pkg}/" 2>/dev/null || true
  done
done
find "${OUT}/xunit" -name '*.xml' | wc -l | xargs echo "xunit files collected:"

step "done"
