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

| Platform | amd64 | arm64 | arm32 |
|----------|-------|-------|-------|
| Ubuntu 22.04 (Jammy) | Verified | Verified | Verified |
| Windows 10 | Verified (on Windows 11) |  |  |
| RHEL 8 | Verified |  |  |
| Ubuntu 20.04 (Focal) | Verified | Verified | Verified |
| macOS | Verified |  |  |
| Debian 11 (Bullseye) | Verified | Verified | Verified |
| OpenEmbedded / Yocto | Verified | Verified | Verified |

- Windows: verified on Windows 11 with Visual Studio 2019. See [Windows and macOS](#windows-and-macos).
- macOS: verified on macOS 15.
- OpenEmbedded / Yocto: verified with a local layer that adds the `rmw_int2dds` recipes. arm32 needs one change in meta-ros: in `meta-ros2/classes/ros_ament_cmake.bbclass`, replace `inherit cmake python3native` with `inherit cmake python3targetconfig`. Without it, the ROS 2 message packages fail with a `LONG_BIT` error before `rmw_int2dds` is reached ([ros/meta-ros#1205](https://github.com/ros/meta-ros/issues/1205), [fix](https://github.com/ros/meta-ros/issues/1205#issuecomment-2298233842)). Boards running an older kernel also need `OLDEST_KERNEL` set to that kernel (tested on NanoPi, kernel 4.14: `OLDEST_KERNEL = "4.14"`).

## Installation

### From the ROS 2 apt repository

```bash
sudo apt update
sudo apt install ros-humble-rmw-int2dds-cpp
source /opt/ros/humble/setup.bash
export RMW_IMPLEMENTATION=rmw_int2dds_cpp
```

`apt install` pulls in the `int2dds_ffi_vendor` package automatically. The RMW library
and its ament-index marker install into `/opt/ros/humble/`, so once the environment is
sourced only `RMW_IMPLEMENTATION` needs to be set.

Supported: **humble / jazzy / lyrical / rolling** × **amd64 / arm64** (rolling: testing repository only).

### From the ROS 2 RPM repository (RHEL)

With the ROS 2 RPM repository set up as in the
[ROS 2 RHEL installation guide](https://docs.ros.org/en/humble/Installation/RHEL-Install-RPMs.html):

```bash
sudo dnf install ros-humble-rmw-int2dds-cpp
source /opt/ros/humble/setup.bash
export RMW_IMPLEMENTATION=rmw_int2dds_cpp
```

### Latest version (testing repository or source build)

The newest release is available from the ROS 2 testing repository:

```bash
sudo apt install -y ros2-testing-apt-source
sudo apt update
sudo apt install ros-humble-rmw-int2dds-cpp
```

To build from source instead, follow [Quick Start](#quick-start).

### Windows and macOS

There are no binary packages of `rmw_int2dds` for Windows or macOS: build it from
source on top of a ROS 2 installation made with the ROS 2 installation guide for
that platform. `int2dds_ffi_vendor` downloads the FFI for the host from the
[int2DDS releases](https://github.com/IntellectusCorp/int2DDS/releases).

Windows, in an x64 Native Tools Command Prompt for VS 2019. The commands below assume ROS 2 Humble in
`C:\pixi_ws\ros2-windows`; change the paths to match your installation.
The ROS 2 Windows binaries are Release builds, so build `rmw_int2dds` as Release too:

```bat
cd /d C:\pixi_ws
pixi shell
call C:\pixi_ws\ros2-windows\local_setup.bat
md C:\dev\ros2_ws\src
git clone -b humble https://github.com/IntellectusCorp/rmw_int2dds.git C:\dev\ros2_ws\src\rmw_int2dds
cd /d C:\dev\ros2_ws
colcon build --merge-install --packages-up-to rmw_int2dds_cpp --cmake-args -DCMAKE_BUILD_TYPE=Release
call C:\dev\ros2_ws\install\local_setup.bat
set RMW_IMPLEMENTATION=rmw_int2dds_cpp
ros2 run demo_nodes_cpp talker
```

macOS (in the shell where the ROS 2 environment is active):

```bash
mkdir -p ~/ros2_ws/src && cd ~/ros2_ws/src
git clone -b humble https://github.com/IntellectusCorp/rmw_int2dds.git
cd ~/ros2_ws
colcon build --packages-up-to rmw_int2dds_cpp --cmake-args -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
source install/setup.bash
export RMW_IMPLEMENTATION=rmw_int2dds_cpp
ros2 run demo_nodes_cpp talker
```

## Quick Start

```bash
# 1) Get the sources into your ROS 2 workspace
mkdir -p ~/ros2_ws/src && cd ~/ros2_ws/src
git clone -b humble https://github.com/IntellectusCorp/rmw_int2dds.git

# 2) Build
cd ~/ros2_ws
source /opt/ros/humble/setup.bash
colcon build --packages-up-to rmw_int2dds_cpp
source install/setup.bash

# 3) Select int2DDS as the middleware
export RMW_IMPLEMENTATION=rmw_int2dds_cpp

# 4) Run any ROS 2 demo
ros2 run demo_nodes_cpp talker
# in another terminal (same RMW_IMPLEMENTATION):
ros2 run demo_nodes_cpp listener
```

On Tier 3 platforms (for Humble: Ubuntu 20.04, Debian 11 and arm32) ROS 2 has no binary packages:
build ROS 2 from source first ([guide](https://docs.ros.org/en/humble/Installation/Alternatives/Ubuntu-Development-Setup.html))
and in step 2 source that workspace's `install/setup.bash` instead of `/opt/ros/humble/setup.bash`.

## Running examples

```bash
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
archive for the host OS, architecture and libc from the
[int2DDS releases](https://github.com/IntellectusCorp/int2DDS/releases),
verifies the library against the `sha256` recorded in the archive's manifest,
and exports the `int2dds_ffi::int2dds_ffi` CMake target used by this RMW package.

Building therefore needs outbound network access to `github.com`. The FFI
version is pinned in one place: `INT2DDS_FFI_VERSION` in
[int2dds_ffi_vendor/CMakeLists.txt](int2dds_ffi_vendor/CMakeLists.txt).

## Test status

All results below were produced by running the listed suites directly.
Integration tests use the official ROS 2
repositories (`rmw_implementation`, `system_tests`).

| Suite | Rolling | Lyrical | Jazzy | Humble |
|---|---|---|---|---|
| `test_rmw_implementation` (RMW conformance gate) | 16/16 | 16/16 | 16/16 | 15/15 |
| `test_communication` same-RMW | 34/34 | 34/34 | 30/30 | 29/29 |
| `test_quality_of_service` | 4/4 | 4/4 | 4/4 | 3/3 |
| `test_rclcpp` | 25/25 | 25/25 | 25/25 | 25/25 |
| `test_cli_remapping` | 1/1 | 1/1 | 1/1 | 1/1 |
| `test_security` | 6/6 | 6/6 | 6/6 | 6/6 |
| In-repo QoS check scripts | 6/6 | 6/6 | 6/6 | 6/6 |
| `rosdoc2 build` | pass | pass | pass | pass |
| `ament_lint` suite | 162 tests, 0 failures, 42 skipped | 162 tests, 0 failures, 42 skipped | 162 tests, 0 failures, 42 skipped | 154 tests, 0 failures, 40 skipped |

Notes:

- `test_rclcpp`: ctest lists more entries than the 25 that run, because the
  upstream suite also generates `node_name` variants that it skips.
- `ament_lint`: the skips are ament_cppcheck's performance guard for cppcheck 2.x
  (set `AMENT_CPPCHECK_ALLOW_SLOW_VERSIONS=1` to run it). The same guard skips the
  cppcheck entry in `test_security`.

## Known issues

- `spin_all_fail_wait_set_clear` (upstream `rclcpp` unit test, not part of the
  `test_rclcpp` suite above; not present on Humble): tracked as a known limitation.
- DDS-Security (SROS 2) is not supported yet (see `doc/security.rst`).
- Rolling and Lyrical, with `rmw_cyclonedds_cpp`: `WStrings` messages are not
  delivered in either direction; the other 13 message types are.
  `rmw_fastrtps_cpp` and `rmw_cyclonedds_cpp` also fail `WStrings` in both
  directions, and upstream `test_communication` skips `WStrings` for that pair.

## Documentation

- Installation: [doc/installation.rst](rmw_int2dds_cpp/doc/installation.rst)
- Usage: [doc/usage.rst](rmw_int2dds_cpp/doc/usage.rst)
- QoS mapping: [doc/qos_mapping.rst](rmw_int2dds_cpp/doc/qos_mapping.rst)
- Security: [doc/security.rst](rmw_int2dds_cpp/doc/security.rst) — **note: DDS-Security / SROS 2 is not supported yet**
- Examples: [examples/](rmw_int2dds_cpp/examples/)
- API documentation:
  [Humble](https://docs.ros.org/en/humble/p/rmw_int2dds_cpp/) ·
  [Jazzy](https://docs.ros.org/en/jazzy/p/rmw_int2dds_cpp/) ·
  [Lyrical](https://docs.ros.org/en/lyrical/p/rmw_int2dds_cpp/) ·
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
