# ROS 2 RMW for int2DDS

<div align="center">

A **ROS 2 RMW implementation** that binds the **int2DDS**
DDS/RTPS middleware to the ROS 2 middleware (RMW) interface.

</div>

## Overview

**rmw_int2dds_cpp** lets ROS 2 applications run on top of
[**int2DDS**](https://github.com/IntellectusCorp/int2DDS), a Rust
implementation of the OMG DDS standard (RTPS 2.5). It implements the ROS 2 `rmw`
C interface so that any ROS 2 stack (rclcpp, rclpy, ros2 CLI, tools) can use
int2DDS as its middleware via `RMW_IMPLEMENTATION=rmw_int2dds_cpp`.

> Status: **work in progress** — APIs and test results are being stabilized
> ahead of a request for Tier 3 status in [REP 2000](https://ros.org/reps/rep-2000.html).

### Supported ROS 2 distributions

| Distribution | Status |
|--------------|--------|
| Humble Hawksbill (LTS) | Supported (verified) |
| Jazzy Jalisco (LTS)    | Supported (verified) |
| Lyrical Luth (LTS)     | Supported (verified, except cross-vendor `WStrings` - see Known issues) |
| Rolling Ridley         | Supported (verified, except cross-vendor `WStrings` - see Known issues) |

### Supported platforms

| Platform | amd64 | arm64 |
|----------|-------|-------|
| Ubuntu 24.04 (Noble) | Verified | Verified |

- Windows, macOS: not supported. The `int2dds_ffi_vendor` releases carry Linux only.
- arm32: not supported. ROS 2 publishes no arm32 binaries.

## Quick Start

```bash
# 1) Get the sources into your ROS 2 workspace
# This repository carries every package you need: rmw_int2dds_cpp, its
# int2dds_ffi_vendor dependency, and the rmw_int2dds_validation probes.
mkdir -p ~/ros2_ws/src && cd ~/ros2_ws/src
git clone -b jazzy https://github.com/IntellectusCorp/rmw_int2dds.git

# 2) Build
cd ~/ros2_ws
source /opt/ros/jazzy/setup.bash
colcon build --packages-up-to rmw_int2dds_cpp
source install/setup.bash

# 3) Select int2DDS as the middleware
export RMW_IMPLEMENTATION=rmw_int2dds_cpp

# 4) Run any ROS 2 demo
ros2 run demo_nodes_cpp talker
# in another terminal (same RMW_IMPLEMENTATION):
ros2 run demo_nodes_cpp listener
```

## Installation

### From the ROS 2 apt repository

```bash
sudo apt update
sudo apt install ros-jazzy-rmw-int2dds-cpp
source /opt/ros/jazzy/setup.bash
export RMW_IMPLEMENTATION=rmw_int2dds_cpp
ros2 run demo_nodes_cpp talker
# in another terminal (same RMW_IMPLEMENTATION):
ros2 run demo_nodes_cpp listener
```

`apt install` pulls in the `int2dds_ffi_vendor` package automatically. The RMW library
and its ament-index marker install into `/opt/ros/jazzy/`, so once the environment is
sourced only `RMW_IMPLEMENTATION` needs to be set.

Supported: **humble / jazzy / lyrical / rolling** × **amd64 / arm64**.

### Latest version (testing repository or source build)

The newest release is available from the ROS 2 testing repository:

```bash
sudo apt install -y ros2-testing-apt-source
sudo apt update
sudo apt install ros-jazzy-rmw-int2dds-cpp
```

To build from source instead, follow [Quick Start](#quick-start).

To build the Debian packages yourself: `packaging/build-deb.sh <distro> <arch>` (needs
Docker; see `packaging/` for the build and verification scripts).

## Running examples

```bash
# C++ (rclcpp)
RMW_IMPLEMENTATION=rmw_int2dds_cpp ros2 run demo_nodes_cpp talker
RMW_IMPLEMENTATION=rmw_int2dds_cpp ros2 run demo_nodes_cpp listener

# Python (rclpy)
RMW_IMPLEMENTATION=rmw_int2dds_cpp ros2 run demo_nodes_py talker
RMW_IMPLEMENTATION=rmw_int2dds_cpp ros2 run demo_nodes_py listener
```

> These are the minimal per-language talker/listener smoke examples; run each
> command in its own terminal.

### More examples

```bash
# Service / client
RMW_IMPLEMENTATION=rmw_int2dds_cpp ros2 run demo_nodes_cpp add_two_ints_server
RMW_IMPLEMENTATION=rmw_int2dds_cpp ros2 run demo_nodes_cpp add_two_ints_client

# Cross-vendor: int2DDS publisher, Fast DDS subscriber
RMW_IMPLEMENTATION=rmw_int2dds_cpp ros2 run demo_nodes_cpp talker
RMW_IMPLEMENTATION=rmw_fastrtps_cpp ros2 run demo_nodes_cpp listener

# QoS: best-effort subscriber on a reliable publisher
RMW_IMPLEMENTATION=rmw_int2dds_cpp ros2 run demo_nodes_cpp talker
RMW_IMPLEMENTATION=rmw_int2dds_cpp ros2 run demo_nodes_cpp listener_best_effort
```

## Middleware library dependency

This package links against the **int2DDS FFI library** (`libint2dds_ffi.so*`
and `int2dds-ffi.h`), which is provided by the `int2dds_ffi_vendor` package in
this repository. At CMake configure time the vendor package downloads the
per-OS tarball from the
[int2dds_ffi_vendor releases](https://github.com/IntellectusCorp/int2dds_ffi_vendor/releases),
selects the artifact matching the host architecture and libc, verifies it
against the `sha256` recorded in the bundled manifest, and exports the
`int2dds_ffi::int2dds_ffi` CMake target used by this RMW package.

Building therefore needs outbound network access to `github.com`. The FFI
version is pinned in one place: `INT2DDS_FFI_VERSION` in
[int2dds_ffi_vendor/CMakeLists.txt](int2dds_ffi_vendor/CMakeLists.txt).

## Test status

All results below were produced by running the listed suites directly.
Same-vendor and cross-vendor integration tests use the official ROS 2
repositories (`rmw_implementation`, `system_tests`).

| Suite | Rolling | Lyrical | Jazzy | Humble |
|---|---|---|---|---|
| `test_rmw_implementation` (RMW conformance gate) | 16/16 | 16/16 | 16/16 | 15/15 |
| `test_communication` same-RMW | 34/34 | 34/34 | 30/30 | 29/29 |
| `test_quality_of_service` | 4/4 | 4/4 | 4/4 | 3/3 |
| `test_rclcpp` | 25/25 | 25/25 | 25/25 | 25/25 |
| Cross-vendor vs `rmw_fastrtps_cpp` | 8/8 | 8/8 | 8/8 | 8/8 |
| Cross-vendor vs `rmw_cyclonedds_cpp` | 8/8 excluding `WStrings` (see Known issues) | 8/8 excluding `WStrings` (see Known issues) | 8/8 | 8/8 |
| `test_cli_remapping` | 1/1 | 1/1 | 1/1 | 1/1 |
| `test_security` | 6/6 | 6/6 | 6/6 | 6/6 |
| In-repo QoS check scripts | 6/6 | 6/6 | 6/6 | 6/6 |
| `rosdoc2 build` | pass | pass | pass | pass |
| `ament_lint` suite | 162 tests, 0 failures, 42 skipped | 162 tests, 0 failures, 42 skipped | 162 tests, 0 failures, 42 skipped | 154 tests, 0 failures, 40 skipped |

Cross-vendor scope: the upstream suite skips service/action combinations for
**all** vendor pairs on every distro, so the cross-vendor rows cover the 8
pub/sub direction/language combinations only.

Counting notes (the full run/skip/fail decomposition of every cell was
verified against the per-test xunit/gtest XML results):

- Totals differ across columns only where the upstream suite itself differs by
  distro version (keyed-type tests, `test_event`, `best_available` QoS), never
  because a test was dropped.
- `test_rclcpp`: 25 is the actually-run count on all four distros; the raw
  ctest entry count also includes upstream-skipped cross-RMW `node_name`
  variants, so it varies by environment.
- Upstream-skipped cases inside otherwise-run suites (6 loaned-message /
  allocator cases in the Rolling and Lyrical gates, 5 in Jazzy and Humble) are counted in ctest's
  headline totals even though they do not run.
- `ament_lint`: the count is the eight linters' combined xunit testcase total
  (162 on Rolling, Lyrical and Jazzy, 154 on Humble); running `colcon test-result` over
  the whole build directory prints 8 more (170 / 162) because it also sums the
  ctest summary file that wraps those same eight linters. The skips (42 on
  Rolling, Lyrical and Jazzy, 40 on Humble) are ament_cppcheck's performance guard for
  cppcheck 2.x (set `AMENT_CPPCHECK_ALLOW_SLOW_VERSIONS=1` to run it). The same
  guard skips the cppcheck entry in `test_security`.

## Known issues

- `spin_all_fail_wait_set_clear` (upstream `rclcpp` unit test, not part of the
  `test_rclcpp` suite above; not present on Humble): tracked as a known limitation.
- DDS-Security (SROS 2) is not supported yet (see `doc/security.rst`).
- Rolling and Lyrical cross-vendor vs `rmw_cyclonedds_cpp`: the `WStrings` message type is
  not interoperable in either direction. This is a vendor-level wstring
  wire-format mismatch, not an int2DDS defect: on Rolling and Lyrical, CycloneDDS
  serializes wstring as UTF-16 (2 bytes/char, byte-length prefix) while
  FastDDS and int2DDS use 4 bytes/char with a character-count prefix
  (verified by serializing the same message under the int2DDS, FastDDS, and
  CycloneDDS RMWs — int2DDS output is byte-identical to FastDDS; RTI Connext
  is in the same 4-byte camp because `rmw_connextdds` serializes ROS messages
  through `rosidl_typesupport_fastrtps`). Upstream `test_communication`
  acknowledges the same incompatibility by excluding `WStrings` from the
  FastDDS×CycloneDDS and Connext×CycloneDDS pairs ("CycloneDDS don't FastRTPS
  interoperate for WString"); int2DDS is simply not on that vendor-name
  exclusion list, so the case runs and fails. All other 13 message types pass
  in both directions.

## Documentation

- Installation: [doc/installation.rst](rmw_int2dds_cpp/doc/installation.rst)
- Usage: [doc/usage.rst](rmw_int2dds_cpp/doc/usage.rst)
- QoS mapping: [doc/qos_mapping.rst](rmw_int2dds_cpp/doc/qos_mapping.rst)
- Security: [doc/security.rst](rmw_int2dds_cpp/doc/security.rst) — **note: DDS-Security / SROS 2 is not supported yet**
- Examples: [examples/](rmw_int2dds_cpp/examples/)
- API documentation:
  [Humble](https://docs.ros.org/en/humble/p/rmw_int2dds_cpp/) ·
  [Jazzy](https://docs.ros.org/en/jazzy/p/rmw_int2dds_cpp/) ·
  [Rolling](https://docs.ros.org/en/rolling/p/rmw_int2dds_cpp/)

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). All contributions are subject to the
[Code of Conduct](CODE_OF_CONDUCT.md) and the project CLA
([individual](CLA-Individual.md) / [corporate](CLA-Corporate.md)).

## License

Licensed under the [Apache License 2.0](LICENSE). See [NOTICE](NOTICE) and
[THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md).
"int2DDS" and related marks are trademarks of Intellectus Corp.; see
[TRADEMARK_POLICY.md](TRADEMARK_POLICY.md).

## Contact

Intellectus Corp. — int2dds@int2.us
