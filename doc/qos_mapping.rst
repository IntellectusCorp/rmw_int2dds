QoS Mapping
===========

This page documents how ROS 2 QoS policies map onto int2DDS (DDS) QoS, and the
current support status of each policy.

.. list-table:: ROS 2 ↔ int2DDS QoS
   :header-rows: 1
   :widths: 22 28 50

   * - ROS 2 QoS
     - DDS QoS Policy
     - Notes (Supported · Partial · Planned)
   * - Reliability (RELIABLE / BEST_EFFORT)
     - Reliability
     - Supported
   * - Durability (TRANSIENT_LOCAL / VOLATILE)
     - Durability
     - Supported
   * - History (KEEP_LAST / KEEP_ALL)
     - History
     - Supported
   * - Depth
     - History depth
     - Supported
   * - Deadline
     - Deadline
     - Supported (missed-deadline events on publisher and subscription)
   * - Lifespan
     - Lifespan
     - Supported (expired samples are not delivered)
   * - Liveliness (AUTOMATIC / MANUAL_BY_TOPIC)
     - Liveliness
     - Supported. Deprecated ``MANUAL_BY_NODE`` is accepted and mapped to DDS
       ``MANUAL_BY_PARTICIPANT``
   * - Liveliness lease duration
     - Liveliness lease_duration
     - Supported (finite and infinite leases; compatibility verified against
       other vendors)
   * - System default policies
     - n/a
     - Resolved to concrete defaults (RELIABLE / VOLATILE / KEEP_LAST 10 /
       AUTOMATIC) and reported via ``get_actual_qos``
   * - Best available (Jazzy)
     - n/a
     - Supported. Adapted to discovered endpoints via
       ``rmw_dds_common::qos_profile_get_best_available_for_topic_*``

Verification evidence (2026-06-11, Jazzy)
-----------------------------------------

* Official ``test_quality_of_service`` suite: 4/4 PASS
  (deadline, lifespan, liveliness, best_available).
* In-repo QoS demos (``validation/qos``) on ``rmw_int2dds_cpp``: deadline event,
  durability late-joiner, history/depth drain, lifespan expiry,
  liveliness AUTOMATIC and MANUAL_BY_TOPIC — all OK.
* QoS *compatibility* rules: ``rmw_qos_profile_check_compatible`` delegates to
  ``rmw_dds_common`` (identical verdicts to the reference RMWs), and
  cross-vendor pub/sub interop with rmw_fastrtps_cpp and rmw_cyclonedds_cpp
  passes 8/8 each, which exercises reliability/durability/liveliness
  compatibility on the wire.
