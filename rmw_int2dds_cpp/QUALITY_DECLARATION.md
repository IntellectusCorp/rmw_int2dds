# `rmw_int2dds_cpp` Quality Declaration

This document is a declaration of software quality for the `rmw_int2dds_cpp`
package, based on the guidelines in [REP-2004](https://www.ros.org/reps/rep-2004.html).

The package `rmw_int2dds_cpp` claims to be in the **Quality Level 4** category.

Below are the rationales, notes, and caveats for this claim, organized by
requirement listed in the [Package Quality Categories in REP-2004](https://www.ros.org/reps/rep-2004.html#package-quality-categories).

## Version Policy [1]

### Version Scheme [1.i]

`rmw_int2dds_cpp` uses `semver` according to the recommendation for ROS Core
packages in the [ROS 2 Developer Guide](https://docs.ros.org/en/humble/The-ROS2-Project/Contributing/Developer-Guide.html#versioning).

### Version Stability [1.ii]

`rmw_int2dds_cpp` is not yet at a stable version (`>= 1.0.0`). The current
version can be found in its [package.xml](package.xml), and its change history
can be found in its [CHANGELOG](CHANGELOG.rst).

### Public API Declaration [1.iii]

This package implements the public RMW API declared by the
[`rmw`](https://github.com/ros2/rmw) package. It does not add public API of
its own beyond the RMW interface.

### API / ABI Stability Within a Released ROS Distribution [1.iv/1.v/1.vi]

`rmw_int2dds_cpp` will not break public API or ABI within a released ROS
distribution once it has been released into a distribution.

## Change Control Process [2]

### Change Requests / Contributor Origin [2.i, 2.ii]

All changes occur through a pull request. Contributions are accepted under
the project CLA; see [CONTRIBUTING](CONTRIBUTING.md).

### Peer Review Policy [2.iii]

All pull requests must have at least one peer review from a maintainer.

### Continuous Integration [2.iv]

All pull requests must pass CI (build, unit tests, linters) on the tier 1
platforms targeted by this package before merging.

> TODO: add CI badges / links once public CI is configured.

## Documentation [3]

### Feature Documentation [3.i]

Features are documented under [doc/](doc/): installation, usage, QoS mapping,
security, and architecture (entity mapping).

### Public API Documentation [3.ii]

API documentation is generated with `rosdoc2` (Doxygen + Sphinx) and hosted on
docs.ros.org once released.

### License [3.iii]

The license is Apache 2.0; the full text is in [LICENSE](LICENSE). All source
files include a license and copyright header checked by linters.

### Copyright Statements [3.iv]

Copyright holders are stated in the header of each source file and in
[NOTICE](NOTICE); enforced via `ament_copyright`.

## Testing [4]

### Feature / Public API Testing [4.i, 4.ii]

The package is tested against the ROS 2 RMW conformance suite
[`test_rmw_implementation`](https://github.com/ros2/rmw_implementation) with
`RMW_IMPLEMENTATION=rmw_int2dds_cpp`. System-level behavior is additionally
exercised via `test_rclcpp` / `system_tests`.

> TODO: link to public test results / CI runs.

### Coverage [4.iii] / Performance [4.iv]

Coverage and performance testing policies are not yet established
(consistent with Quality Level 4). Establishing coverage tracking is a
prerequisite for claiming Level 3.

### Linters and Static Analysis [4.v]

`ament_lint_auto` + `ament_lint_common` (uncrustify, cpplint, cppcheck,
copyright) run as part of the package tests.

## Dependencies [5]

Direct runtime dependencies (`rmw`, `rmw_dds_common`, `rcutils`, `rcpputils`,
`rosidl_*`) are ROS core packages at or above this package's quality level.
The `int2dds` core library is an external (non-ROS) dependency provided by
Intellectus Corp. under Apache 2.0.

> TODO: state the quality level assessment of the int2dds core once its own
> quality documentation is published.

## Platform Support [6]

This package targets the Tier 1 platforms of the targeted ROS distribution as
defined in [REP-2000](https://www.ros.org/reps/rep-2000.html); verified
platforms and architectures are listed in the [README](README.md).

## Security [7]

### Vulnerability Disclosure Policy [7.i]

This package conforms to the Vulnerability Disclosure Policy in
[SECURITY.md](SECURITY.md); vulnerabilities are reported privately to
int2dds@int2.us.
