---
name: Bug Report
about: Report a bug in the ROS 2 RMW binding.
labels: bug
---

> ⚠️ **Security Notice**
>
> Please do **NOT** report security vulnerabilities here.
> If your issue involves a potential security vulnerability, report it
> **privately** per [SECURITY.md](../../SECURITY.md) to allow responsible
> disclosure.
>
> If the problem is in the underlying DDS/RTPS core rather than this RMW
> binding, please file it in the [int2DDS](https://github.com/IntellectusCorp/int2DDS/issues)
> repository instead.

## Problem Description

- Describe the problem in detail.

## Steps to Reproduce

1. Step-by-step instructions to reproduce the issue.
2. ...
3. ...

Include the exact commands and, where possible, a minimal `rclcpp`/`rclpy`
node or `ros2` CLI invocation. Remember to set `RMW_IMPLEMENTATION=rmw_int2dds_cpp`.

## Expected Behavior

- Describe what should happen if there were no problem.

## Actual Behavior

- Describe what actually happens (error messages, hangs, crashes).

## Environment

- OS / arch: [e.g., Ubuntu 22.04 amd64]
- ROS 2 distribution: [e.g., Humble, Jazzy]
- rmw_int2dds_cpp version/commit: [e.g., commit abc123]
- int2DDS FFI library version: [e.g., libint2dds_ffi.so x.y.z]
- Other RMW for cross-vendor tests (if any): [e.g., rmw_fastrtps_cpp]

## Logs

- Relevant output. For more detail, run with
  `RCUTILS_CONSOLE_OUTPUT_FORMAT='[{severity}] [{name}]: {message}'` and
  raise the log level (e.g., `--ros-args --log-level debug`).

## Additional Information

- Anything else that may help understand and resolve this issue.
