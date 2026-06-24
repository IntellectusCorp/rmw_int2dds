# Third-Party Licenses

This document lists third-party components used by **rmw_int2dds_cpp**
and the corresponding license terms.

## 1. Bundled third-party code

- None at this time.

## 2. Upstream DDS core

- **int2DDS** (Intellectus Corp.) — Apache License 2.0
  - The DDS/RTPS implementation that this RMW binds to.

## 3. ROS 2 build/runtime dependencies

This package depends on ROS 2 packages fetched via the ROS distribution and
`rosdep`. These are not vendored into this repository. Refer to each upstream
project for full license details (most are Apache-2.0 or BSD).

- `rmw`, `rmw_dds_common`
- `rosidl_runtime_c`, `rosidl_runtime_cpp`
- `rosidl_typesupport_introspection_c`, `rosidl_typesupport_introspection_cpp`
- `rcpputils`, `rcutils`
- `rosidl_typesupport_fastrtps_c`, `rosidl_typesupport_fastrtps_cpp` (if used)

> An up-to-date dependency list is declared in `package.xml`.
