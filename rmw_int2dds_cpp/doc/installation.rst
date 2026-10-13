Installation
============

This guide covers installing ``rmw_int2dds_cpp`` from binary packages and
building it from source.

Prerequisites
-------------

* ROS 2 Lyrical installed and sourced.
* ``colcon`` build tools.
* A C++20 compiler and CMake >= 3.20.
* Network access to ``github.com`` when building from source: the
  ``int2dds_ffi_vendor`` package in this repository downloads the int2DDS FFI
  library at CMake configure time.

Binary installation
-------------------

.. code-block:: bash

   # Ubuntu
   sudo apt install ros-lyrical-rmw-int2dds-cpp
   # RHEL
   sudo dnf install ros-lyrical-rmw-int2dds-cpp

The ``int2dds_ffi_vendor`` package is installed with it.

Build from source
-----------------

.. code-block:: bash

   mkdir -p ~/ros2_ws/src && cd ~/ros2_ws/src
   git clone -b lyrical https://github.com/IntellectusCorp/rmw_int2dds.git

   cd ~/ros2_ws
   source /opt/ros/lyrical/setup.bash
   colcon build --packages-up-to rmw_int2dds_cpp
   source install/setup.bash

On RHEL 10, install the ament lint tools and GoogleTest before building:

.. code-block:: bash

   sudo dnf install ros-lyrical-ament-flake8 ros-lyrical-ament-cpplint ros-lyrical-ament-copyright \
     ros-lyrical-ament-cppcheck ros-lyrical-ament-lint-cmake ros-lyrical-ament-uncrustify \
     ros-lyrical-ament-xmllint ros-lyrical-ament-pep257 ros-lyrical-ament-mypy \
     ros-lyrical-ament-pycodestyle gtest-devel gmock-devel

Verify
------

.. code-block:: bash

   # The implementation should be listed
   ros2 doctor --report | grep -i rmw
   RMW_IMPLEMENTATION=rmw_int2dds_cpp ros2 run demo_nodes_cpp talker
