# Security Policy

## Supported Versions

Security updates are provided for the latest released version on each
supported ROS 2 distribution.

## Reporting a Vulnerability

Please do **not** open a public GitHub issue for security vulnerabilities.

Instead, report privately to: **int2dds@int2.us**

Include where possible:

- A description of the vulnerability and its impact
- Steps to reproduce
- Affected version(s) and platform
- Any suggested mitigation

We aim to acknowledge reports within 5 business days and to provide a
remediation timeline after triage.

## DDS Security

**Not supported yet.** `rmw_int2dds_cpp` does not currently implement
ROS 2 / DDS-Security (SROS 2, `ROS_SECURITY_*`). No authentication, access
control, or encryption is provided. See `doc/security.rst` for details and
deployment guidance.
