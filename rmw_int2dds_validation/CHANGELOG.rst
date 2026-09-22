^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Changelog for package rmw_int2dds_validation
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

0.1.6 (2026-09-22)
------------------
* Build on Foxy. The QoS probes set policies with the ``RMW_QOS_POLICY_*``
  values instead of the ``rclcpp::*Policy`` enums added after Foxy, the content
  filtered topic probe is built only where the rmw headers provide that API,
  and the rclpy liveliness check also runs on Python 3.8.
* Contributors: Intellectus Corp.

0.1.5 (2026-09-10)
------------------
* No source changes; released in lockstep with ``rmw_int2dds_cpp``.
* Contributors: Intellectus Corp.

0.1.4 (2026-09-04)
------------------
* No source changes; released in lockstep with ``rmw_int2dds_cpp``.
* Contributors: Intellectus Corp.

0.1.3 (2026-09-02)
------------------
* No source changes; released in lockstep with ``rmw_int2dds_cpp``.
* Contributors: Intellectus Corp.

0.1.1 (2026-08-28)
------------------
* No source changes; released in lockstep with ``rmw_int2dds_cpp``.
* Contributors: Intellectus Corp.

0.1.0 (2026-08-21)
------------------
* Split the rclcpp and rclpy validation probes out of ``rmw_int2dds_cpp`` into
  this package: QoS behaviour (durability, history depth, deadline, liveliness,
  lifespan), content-filtered topic lifecycle, executor callback smoke checks,
  and latency/throughput/readiness measurements. An RMW implementation cannot
  depend on ``rclcpp`` without closing a build dependency cycle, so the probes
  live here.
* Contributors: Intellectus Corp.
