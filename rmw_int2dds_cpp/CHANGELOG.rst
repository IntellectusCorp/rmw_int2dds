^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Changelog for package rmw_int2dds_cpp
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Forthcoming
-----------
* Fix participant/context lifecycle: delete contained entities before the
  participant, release the participant when the last node is destroyed,
  recreate context DDS resources on node creation, and release all context
  resources on init failure.
* Apply requested QoS in services/clients; resolve BEST_AVAILABLE and
  SYSTEM_DEFAULT in the service actual QoS; apply deadline/liveliness.
* Wire localhost-only discovery into SPDP.
* Reject STRICTLY_REQUIRED unique network flow endpoints; restore content-filter
  field codes; set error messages on unsupported API stubs; ignore only
  same-node local publications; randomize the GID process field.
* Batch ``take_sequence`` into a single serialized take.
* Move development test executables into ``test/``; align QUALITY_DECLARATION
  and README test status with the current state.
* Decide ``rmw_wait`` readiness before attaching to the wait set, so a call that
  finds work already pending skips the attach/wait/detach round trip. Guard
  conditions join the pre-wait scan through a non-destructive peek, and a
  zero-timeout poll no longer attaches at all. Saturated single-subscription
  throughput roughly doubles.
* Add ``test_rmw_wait_guards``, the first automated test registered with CTest:
  guard condition readiness, trigger consumption, timeout handling and wake-up
  from another thread, all without a DDS participant.
* Move the ``int2dds_ffi_vendor`` package into this repository. Building from
  source no longer needs a second clone, and the dependency is declared without
  a version range because the two packages are now released in lockstep; the
  prebuilt FFI version stays pinned in one place, ``INT2DDS_FFI_VERSION`` in
  ``int2dds_ffi_vendor/CMakeLists.txt``.
* Contributors: Intellectus Corp.

0.0.1 (2026-06-25)
------------------
* Initial public release of the ROS 2 RMW implementation for int2DDS.
* Implement the ``rmw`` C interface: nodes, publishers, subscriptions,
  services, clients, graph queries, guard conditions, wait sets and events.
* Map ROS 2 QoS policies (history, reliability, durability, deadline,
  lifespan, liveliness) onto the int2DDS DDS/RTPS middleware.
* Provide CDR (de)serialization and introspection-based type support
  for both C and C++ messages.
* Register the implementation through ``register_rmw_implementation`` and
  mark the package as a member of ``rmw_implementation_packages``.
* Link against the prebuilt int2DDS FFI library exported by the
  ``int2dds_ffi_vendor`` package.
* Add documentation: installation, usage, architecture, QoS mapping and
  security guides.
* Add a ``validation/`` suite covering QoS, callbacks, content filtering
  and performance (latency / throughput / readiness) for rclcpp and rclpy.
* Contributors: Intellectus Corp.

.. note::

   This file uses the reStructuredText format expected by ``bloom`` /
   ``catkin_generate_changelog`` for ROS 2 package releases. When cutting
   the next release, move entries from ``Forthcoming`` into a new dated,
   versioned section (e.g. ``0.2.0 (YYYY-MM-DD)``).
