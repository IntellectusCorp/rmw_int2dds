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
| Foxy Fitzroy (LTS)     | Supported (verified) |
| Humble Hawksbill (LTS) | Supported (verified) |
| Jazzy Jalisco (LTS)    | Supported (verified) |
| Lyrical Luth (LTS)     | Supported (verified, except cross-vendor `WStrings` - see Known issues) |
| Rolling Ridley         | Supported (verified, except cross-vendor `WStrings` - see Known issues) |

### Supported platforms

| Platform | amd64 | arm64 |
|----------|-------|-------|
| Ubuntu 20.04 (Focal) | Verified | Verified |

- Windows, macOS: not supported. The `int2dds_ffi_vendor` releases carry Linux only.
- arm32: not supported. ROS 2 publishes no arm32 binaries.

## Quick Start

```bash
# 1) Get the sources into your ROS 2 workspace
mkdir -p ~/ros2_ws/src && cd ~/ros2_ws/src
git clone -b foxy https://github.com/IntellectusCorp/rmw_int2dds.git

# 2) Build
cd ~/ros2_ws
source /opt/ros/foxy/setup.bash
colcon build --packages-up-to rmw_int2dds_cpp
source install/setup.bash

# 3) Select int2DDS as the middleware
export RMW_IMPLEMENTATION=rmw_int2dds_cpp

# 4) Run any ROS 2 demo
ros2 run demo_nodes_cpp talker
# in another terminal (same RMW_IMPLEMENTATION):
ros2 run demo_nodes_cpp listener
```

## Binary install (.deb)

Foxy is not released to the ROS 2 apt repository, so build the two `.deb`s for
your architecture yourself with `packaging/build-deb.sh foxy <arch>` (needs
Docker; the packages are written to `dist/foxy/<arch>/`), then install them:

```bash
cd dist/foxy/amd64
sudo apt install ./ros-foxy-int2dds-ffi-vendor_*_amd64.deb \
                 ./ros-foxy-rmw-int2dds-cpp_*_amd64.deb
source /opt/ros/foxy/setup.bash
export RMW_IMPLEMENTATION=rmw_int2dds_cpp
ros2 run demo_nodes_cpp talker
```

`apt install ./file.deb` installs the file and resolves its dependencies (the rmw
package pulls in the vendor package automatically). The RMW library and its
ament-index marker install into `/opt/ros/foxy/`, so once the environment is
sourced only `RMW_IMPLEMENTATION` needs to be set.

Supported: **foxy / humble / jazzy / lyrical / rolling** × **amd64 / arm64**.

See `packaging/` for the build and verification scripts.

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

## Repository layout

This repository holds three ROS 2 packages:

| Package | Role |
|---------|------|
| [`rmw_int2dds_cpp/`](rmw_int2dds_cpp/) | The RMW implementation itself |
| [`int2dds_ffi_vendor/`](int2dds_ffi_vendor/) | Fetches the prebuilt int2DDS FFI library and exports it to CMake |
| [`rmw_int2dds_validation/`](rmw_int2dds_validation/) | In-repo validation scripts |

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

| Suite | Rolling | Lyrical | Jazzy | Humble | Foxy |
|---|---|---|---|---|---|
| `test_rmw_implementation` (RMW conformance gate) | 16/16 | 16/16 | 16/16 | 15/15 | 13/13 |
| `test_communication` same-RMW | 34/34 | 34/34 | 30/30 | 29/29 | 28/28 |
| `test_quality_of_service` | 4/4 | 4/4 | 4/4 | 3/3 | n/a (not registered upstream) |
| `test_rclcpp` | 25/25 | 25/25 | 25/25 | 25/25 | 23/23 |
| Cross-vendor vs `rmw_fastrtps_cpp` | 8/8 | 8/8 | 8/8 | 8/8 | 8/8 |
| Cross-vendor vs `rmw_cyclonedds_cpp` | 8/8 excluding `WStrings` (see Known issues) | 8/8 excluding `WStrings` (see Known issues) | 8/8 | 8/8 | 8/8 |
| `test_cli_remapping` | 1/1 | 1/1 | 1/1 | 1/1 | 1/1 |
| `test_security` | 6/6 | 6/6 | 6/6 | 6/6 | 6/6 |
| In-repo QoS check scripts | 6/6 | 6/6 | 6/6 | 6/6 | 6/6 |
| `rosdoc2 build` | pass | pass | pass | pass | pass |
| `ament_lint` suite | 162 tests, 0 failures, 42 skipped | 162 tests, 0 failures, 42 skipped | 162 tests, 0 failures, 42 skipped | 154 tests, 0 failures, 40 skipped | 154 tests, 0 failures, 0 skipped |

Cross-vendor scope: the upstream suite skips service/action combinations for
**all** vendor pairs on every distro, so the cross-vendor rows cover the 8
pub/sub direction/language combinations only.

Counting notes (the full run/skip/fail decomposition of every cell was
verified against the per-test xunit/gtest XML results):

- Totals differ across columns only where the upstream suite itself differs by
  distro version (keyed-type tests, `test_event`, `best_available` QoS, and
  Foxy's older suites), never because a test was dropped. Foxy's
  `test_quality_of_service` only builds its test programs
  (`ament_add_gtest_executable`) and registers no ctest tests, hence n/a; QoS
  itself is supported on Foxy and is covered there by the in-repo QoS check
  scripts.
- `test_rclcpp`: 25 (23 on Foxy) is the actually-run count; the raw
  ctest entry count also includes upstream-skipped cross-RMW `node_name`
  variants, so it varies by environment.
- Upstream-skipped cases inside otherwise-run suites (6 loaned-message /
  allocator cases in the Rolling and Lyrical gates, 5 in Jazzy and Humble) are counted in ctest's
  headline totals even though they do not run.
- `ament_lint`: the count is the eight linters' combined xunit testcase total
  (162 on Rolling, Lyrical and Jazzy, 154 on Humble and Foxy); running `colcon test-result`
  over the whole build directory prints 8 more (170 / 162) because it also sums the
  ctest summary file that wraps those same eight linters. The skips (42 on
  Rolling, Lyrical and Jazzy, 40 on Humble) are ament_cppcheck's performance guard for
  cppcheck 2.x (set `AMENT_CPPCHECK_ALLOW_SLOW_VERSIONS=1` to run it). The same
  guard skips the cppcheck entry in `test_security`. Foxy's ament_cppcheck has no
  such guard (Ubuntu 20.04 ships cppcheck 1.90), so nothing is skipped there.
- Foxy's `rosdoc2 build` ran with rosdoc2 in a Python 3.10 environment and
  doxygen 1.9.8.

## Known issues

- `spin_all_fail_wait_set_clear` (upstream `rclcpp` unit test, not part of the
  `test_rclcpp` suite above; not present on Humble or Foxy): tracked as a known limitation.
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
