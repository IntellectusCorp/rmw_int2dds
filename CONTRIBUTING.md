# Contributing to rmw_int2dds_cpp

Thank you for your interest in contributing! This package is the ROS 2 RMW
binding for int2DDS. For contributions to the underlying DDS core, see the
[int2DDS](https://github.com/IntellectusCorp/int2DDS) repository.

## Code of Conduct

This project adheres to a [Code of Conduct](CODE_OF_CONDUCT.md).

## Contributor License Agreement (CLA)

By submitting a pull request you agree to the project CLA
([individual](CLA-Individual.md) / [corporate](CLA-Corporate.md)).

## Development Environment

### Prerequisites

- A supported **ROS 2** distribution (Humble or Jazzy) installed and sourced
- **colcon** build tool
- A C++17 compiler (GCC/Clang/MSVC), CMake >= 3.14.4
- The **int2DDS** core library available as a dependency

### Workspace setup

```bash
mkdir -p ~/ros2_ws/src && cd ~/ros2_ws/src
git clone https://github.com/IntellectusCorp/rmw_int2dds.git
cd ~/ros2_ws
rosdep install --from-paths src --ignore-src -r -y
```

## Building and Testing

```bash
# Build
colcon build --packages-up-to rmw_int2dds_cpp

# Run this package's tests
colcon test --packages-select rmw_int2dds_cpp
colcon test-result --verbose

# Run the RMW interface conformance suite against int2DDS
RMW_IMPLEMENTATION=rmw_int2dds_cpp \
  colcon test --packages-select test_rmw_implementation
```

> Tier 3 listing requires the full `test_rmw_implementation` suite to pass.

## Code Style

This package follows the ROS 2
[Code Style and Language Versions](https://docs.ros.org/en/jazzy/The-ROS2-Project/Contributing/Code-Style-Language-Versions.html)
guide:

- C++17, max line length 100, `.hpp`/`.cpp` extensions
- Linters run as tests via `ament_lint_auto` + `ament_lint_common`
  (`ament_clang_format`, `ament_cpplint`, `ament_uncrustify`, `ament_cppcheck`)
- Build with `-Wall -Wextra -Wpedantic`

```bash
# Run linters
colcon test --packages-select rmw_int2dds_cpp --ctest-args -R lint
```

## Commit Messages

Use the format `TYPE: short description` (max 72 chars), imperative mood.
Types: `FEAT`, `FIX`, `DOCS`, `STYLE`, `REFACTOR`, `TEST`, `CHORE`, `PERF`.
Reference issues with `Fixes #123`.

## Branch Naming

`feature/`, `fix/`, `hotfix/`, `docs/`, `refactor/`, `test/` prefixes;
lowercase with hyphens (e.g., `feature/qos-deadline`).

## Pull Request Process

1. Rebase on the latest `upstream/main`.
2. Ensure `colcon build` and `colcon test` pass, including linters.
3. Update documentation and `CHANGELOG.rst` if behavior or APIs changed.
4. Open a PR with a clear summary, rationale, and testing notes.
5. Request review from maintainers.

## Reporting Bugs

Open a GitHub issue with: description, reproduction steps, expected vs actual
behavior, environment (OS, ROS 2 distro, int2DDS version), and logs.
For security issues, follow [SECURITY.md](SECURITY.md) instead.

## License

By contributing, you agree your contributions are licensed under the
[Apache License 2.0](LICENSE).

## License of contributions

Any contribution that you make to this repository will
be under the Apache 2 License, as dictated by that
[license](http://www.apache.org/licenses/LICENSE-2.0.html):

~~~
5. Submission of Contributions. Unless You explicitly state otherwise,
   any Contribution intentionally submitted for inclusion in the Work
   by You to the Licensor shall be under the terms and conditions of
   this License, without any additional terms or conditions.
   Notwithstanding the above, nothing herein shall supersede or modify
   the terms of any separate license agreement you may have executed
   with Licensor regarding such Contributions.
~~~
