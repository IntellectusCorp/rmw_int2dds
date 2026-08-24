Installation
============

This guide covers building ``rmw_int2dds_cpp`` from source and (later) installing
from binaries.

Prerequisites
-------------

* A supported ROS 2 distribution installed and sourced (Humble, Jazzy, Lyrical or Rolling).
* ``colcon`` build tools and ``rosdep``.
* A C++20 compiler and CMake >= 3.14.4.
* The int2DDS core library (resolved via the ``int2dds`` rosdep key).

Build from source
-----------------

.. code-block:: bash

   mkdir -p ~/ros2_ws/src && cd ~/ros2_ws/src
   git clone https://github.com/IntellectusCorp/rmw_int2dds.git

   cd ~/ros2_ws
   rosdep install --from-paths src --ignore-src -r -y
   colcon build --packages-up-to rmw_int2dds_cpp
   source install/setup.bash

Verify
------

.. code-block:: bash

   # The implementation should be listed
   ros2 doctor --report | grep -i rmw
   RMW_IMPLEMENTATION=rmw_int2dds_cpp ros2 run demo_nodes_cpp talker

Binary installation
-------------------

Once released through the ROS buildfarm, install via the package manager:

.. code-block:: bash

   # Humble
   sudo apt install ros-humble-rmw-int2dds-cpp
   # Jazzy
   sudo apt install ros-jazzy-rmw-int2dds-cpp
   # Lyrical
   sudo apt install ros-lyrical-rmw-int2dds-cpp
   # Rolling
   sudo apt install ros-rolling-rmw-int2dds-cpp

.. note::

   Binary availability depends on the package being released into ``rosdistro``.
