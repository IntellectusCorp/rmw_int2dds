# Examples

These examples show how to run standard ROS 2 nodes on top of int2DDS by
setting `RMW_IMPLEMENTATION=rmw_int2dds_cpp`. They mirror the level provided by
other DDS vendors (e.g. Cyclone DDS).

## 1. Talker / Listener (pub-sub)

```bash
# Terminal 1
export RMW_IMPLEMENTATION=rmw_int2dds_cpp
ros2 run demo_nodes_cpp talker

# Terminal 2
export RMW_IMPLEMENTATION=rmw_int2dds_cpp
ros2 run demo_nodes_cpp listener
```

## 2. Add / AddTwoInts (service-client)

```bash
export RMW_IMPLEMENTATION=rmw_int2dds_cpp
ros2 run demo_nodes_cpp add_two_ints_server
ros2 run demo_nodes_cpp add_two_ints_client
```

## 3. Cross-vendor interoperability

Run the publisher with int2DDS and the subscriber with another RMW to verify
RTPS interoperability:

```bash
# Publisher
RMW_IMPLEMENTATION=rmw_int2dds_cpp ros2 run demo_nodes_cpp talker
# Subscriber (different vendor)
RMW_IMPLEMENTATION=rmw_cyclonedds_cpp ros2 run demo_nodes_cpp listener
```

## 4. QoS demo

```bash
export RMW_IMPLEMENTATION=rmw_int2dds_cpp
ros2 run demo_nodes_cpp talker_qos     # if available in your distro
```

> TODO: add package-local example nodes (rclcpp) where helpful, with a
> CMakeLists.txt that builds them.
