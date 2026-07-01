^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Changelog for package rmw_int2dds_cpp
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Forthcoming
-----------
* No changes yet.

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
