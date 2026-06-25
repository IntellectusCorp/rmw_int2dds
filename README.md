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
# 1) Build in your ROS 2 workspace
cd ~/ros2_ws
colcon build --packages-up-to rmw_int2dds_cpp
source install/setup.bash

# 2) Select int2DDS as the middleware
export RMW_IMPLEMENTATION=rmw_int2dds_cpp

# 3) Run any ROS 2 demo
ros2 run demo_nodes_cpp talker
# in another terminal (same RMW_IMPLEMENTATION):
ros2 run demo_nodes_cpp listener
```

## Middleware library dependency

This package links against the closed-source **int2DDS FFI library**
(`libint2dds_ffi.so` and `int2dds-ffi.h`), which is **not** included in this
repository. Place the library under `lib/` and the header under `include/`
before building (both paths are gitignored). Distribution through a `rosdep`
key and binary packages is planned; until then the library is provided by
Intellectus Corp. (int2dds@int2.us). The int2DDS core itself is scheduled to
be open-sourced.

## Test status

All results below were produced by running the listed suites directly; see
`doc/` for methodology. Same-vendor and cross-vendor integration tests use the
official ROS 2 repositories (`rmw_implementation`, `system_tests`).

| Suite | Jazzy | Humble |
|---|---|---|
| `test_rmw_implementation` (RMW conformance gate) | 16/16 | 15/15 (full set; `test_event` exists on Jazzy only) |
| `test_communication` same-RMW | 30/30 | 29/29 |
| `test_quality_of_service` | 4/4 | 3/3 (full set; `best_available` is Iron+) |
| `test_rclcpp` | 31/31 | 27/27 |
| Cross-vendor pub/sub vs `rmw_fastrtps_cpp` | 8/8 | 8/8 |
| Cross-vendor pub/sub vs `rmw_cyclonedds_cpp` | 8/8 | 8/8 |
| In-repo QoS demos (deadline/durability/history/lifespan/liveliness) | all pass | — |
| `rosdoc2 build` | pass | — |
| `ament_lint` suite | 283 tests, 0 failures | — |

Cross-vendor service/action combinations are skipped upstream for **all**
vendor pairs and are therefore not part of the cross-vendor scope.

## Known issues

- A full-stack run of the `rcl` / `rclcpp` / `rclpy` / `ros2cli` test suites
  (8235 tests, 2026-06-12 baseline) showed 128 failures attributed to this
  RMW. Fixes for the highest-impact gaps (event callbacks, wait timeout
  rounding, message-info timestamps, event taking) landed on 2026-06-12; a
  full re-run after the fixes shows 82 remaining failures, dominated by
  graph/discovery propagation timing. These are tracked for follow-up.
- Cross-vendor pub/sub with RTI Connext passes 20/24; the remaining failures
  are isolated to one large-sample type and are under investigation in the
  int2DDS core (RTPS fragment retransmission interop).
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
