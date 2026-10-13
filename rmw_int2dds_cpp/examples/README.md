# Examples

These examples show how to run standard ROS 2 nodes on top of int2DDS by
setting `RMW_IMPLEMENTATION=rmw_int2dds_cpp`.

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

## 3. QoS demo

```bash
export RMW_IMPLEMENTATION=rmw_int2dds_cpp
ros2 run demo_nodes_cpp talker_qos     # if available in your distro
```
