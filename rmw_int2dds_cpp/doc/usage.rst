Usage
=====

Selecting int2DDS as the RMW
----------------------------

Set the ``RMW_IMPLEMENTATION`` environment variable before running any ROS 2
process:

.. code-block:: bash

   export RMW_IMPLEMENTATION=rmw_int2dds_cpp
   ros2 run demo_nodes_cpp talker

Confirm the active middleware:

.. code-block:: bash

   ros2 doctor --report | grep middleware

Switching back
--------------

.. code-block:: bash

   unset RMW_IMPLEMENTATION   # revert to the default RMW

Running examples
----------------

See the ``examples/`` directory in the repository. Minimal pub/sub:

.. code-block:: bash

   # Terminal 1
   export RMW_IMPLEMENTATION=rmw_int2dds_cpp
   ros2 run demo_nodes_cpp talker

   # Terminal 2
   export RMW_IMPLEMENTATION=rmw_int2dds_cpp
   ros2 run demo_nodes_cpp listener

Environment variables
---------------------

int2DDS-specific configuration is provided through environment variables
inherited from the int2DDS core:

======================================  ==================================================
Variable                                Description
======================================  ==================================================
``INT2DDS_NETWORK_INTERFACE``           Network interface to bind (e.g. ``eth0``)
``INT2DDS_NETWORK_IP``                  Bind to a specific IP address directly
``INT2DDS_BROADCAST_ENABLED``           Enable broadcast discovery alongside multicast
``INT2DDS_USE_LOOPBACK_INTERFACE``      Use loopback interface for discovery/endpoints
``INT2DDS_FORCE_LOOPBACK_MULTICAST``    Force multicast egress via loopback (local test)
``INT2DDS_DATA_FRAG_SIZE``              DATA_FRAG fragment size in bytes. rmw_init seeds
                                        ``1344`` when unset, scoping that default to the
                                        ROS path; the int2DDS core's own default is 65000
``INT2DDS_MAX_MESSAGE_SIZE``            Maximum message size in bytes. rmw_init seeds
                                        ``13440`` when unset
``RUST_LOG``                            Log level (``error``/``warn``/``info``/``debug``)
``ROS_DOMAIN_ID``                       ROS 2 domain id (standard ROS variable)
======================================  ==================================================

.. note::

   Keep this table in sync with the int2DDS core documentation.
