# rmw_int2dds_cpp

<div align="center">

A **ROS 2 RMW implementation** that binds the [int2DDS](https://github.com/IntellectusCorp/int2DDS)
DDS/RTPS middleware to the ROS 2 middleware (RMW) interface.

</div>

## Overview

**rmw_int2dds_cpp** lets ROS 2 applications run on top of **int2DDS**, a Rust
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

### Supported platforms

| Platform | Architectures | Status |
|----------|---------------|--------|
| Ubuntu (Linux) | amd64 | Supported (verified) |
| Ubuntu (Linux) | arm64 | Target (library builds; board validation pending) |

## Quick Start

```bash
# 1) Get the sources into your ROS 2 workspace
mkdir -p ~/ros2_ws/src && cd ~/ros2_ws/src
git clone -b rolling https://github.com/IntellectusCorp/rmw_int2dds.git
git clone https://github.com/IntellectusCorp/int2dds_ffi_vendor.git

# 2) Build
cd ~/ros2_ws
source /opt/ros/rolling/setup.bash
colcon build --packages-up-to rmw_int2dds_cpp
source install/setup.bash

# 3) Select int2DDS as the middleware
export RMW_IMPLEMENTATION=rmw_int2dds_cpp

# 4) Run any ROS 2 demo
ros2 run demo_nodes_cpp talker
# in another terminal (same RMW_IMPLEMENTATION):
ros2 run demo_nodes_cpp listener
```

## Middleware library dependency

This package links against the closed-source **int2DDS FFI library**
(`libint2dds_ffi.so*` and `int2dds-ffi.h`), which is provided by the
`int2dds_ffi_vendor` package. The vendor package downloads the release
artifact, verifies the selected library against the manifest, and exports the
`int2dds_ffi::int2dds_ffi` CMake target used by this RMW package.

## Test status

All results below were produced by running the listed suites directly; see
`doc/` for methodology. Same-vendor and cross-vendor integration tests use the
official ROS 2 repositories (`rmw_implementation`, `system_tests`).

| Suite | Jazzy | Humble |
|---|---|---|
| `test_rmw_implementation` (RMW conformance gate) | 16/16 | 15/15 (full set; `test_event` exists on Jazzy only) |
| `test_communication` same-RMW | 30/30 | 29/29 |
| `test_quality_of_service` | 4/4 | 3/3 (full set; `best_available` is Iron+) |
| `test_rclcpp` | 27/27 | 27/27 |
| Cross-vendor pub/sub vs `rmw_fastrtps_cpp` | 8/8 | 8/8 |
| Cross-vendor pub/sub vs `rmw_cyclonedds_cpp` | 8/8 | 8/8 |
| `test_cli_remapping` | 1/1 | 1/1 |
| `test_security` | 6/6 | 6/6 |
| In-repo QoS demos (deadline/durability/history/lifespan/liveliness) | all pass | all pass |
| `rosdoc2 build` | pass | pass |
| `ament_lint` suite | 242 tests, 0 failures | 238 tests, 0 failures |

Cross-vendor service/action combinations are skipped upstream for **all**
vendor pairs and are therefore not part of the cross-vendor scope.

## Known issues

- `spin_all_fail_wait_set_clear` (rclcpp): int2DDS delivers same-participant
  samples asynchronously, so this error-injection robustness test does not
  observe the mocked wait-set clear within its short (~1 ms) window. No data
  loss or crash occurs, and this is not an RMW conformance-gate test; it is
  tracked as a known limitation.
- DDS-Security (SROS 2) is not supported yet (see `doc/security.rst`).
- Content-filtered topics are not functional yet.

## Documentation

- Installation: [doc/installation.rst](doc/installation.rst)
- Usage: [doc/usage.rst](doc/usage.rst)
- QoS mapping: [doc/qos_mapping.rst](doc/qos_mapping.rst)
- Security: [doc/security.rst](doc/security.rst) — **note: DDS-Security / SROS 2 is not supported yet**
- Examples: [examples/](examples/)
- API docs are published at `docs.ros.org/en/{humble,jazzy}/p/rmw_int2dds_cpp/` once released.

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
