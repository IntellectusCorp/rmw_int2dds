#!/usr/bin/env python3

# Copyright 2026 Int2DDS Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import sys
import time

import rclpy
from rclpy.executors import SingleThreadedExecutor
from rclpy.node import Node
from rclpy.qos import DurabilityPolicy, HistoryPolicy, QoSProfile, ReliabilityPolicy
from std_msgs.msg import String


class LateJoinerSubscriber(Node):
    def __init__(self, name: str, topic: str, qos: QoSProfile) -> None:
        super().__init__(name, start_parameter_services=False, enable_rosout=False)
        self.received_messages: list[str] = []
        self.create_subscription(String, topic, self._callback, qos)

    def _callback(self, msg: String) -> None:
        self.received_messages.append(msg.data)
        self.get_logger().info(f'{self.get_name()} heard: {msg.data}')


def spin_for(executor: SingleThreadedExecutor, duration_sec: float) -> None:
    end = time.time() + duration_sec
    while rclpy.ok() and time.time() < end:
        executor.spin_once(timeout_sec=0.1)


def main() -> int:
    rclpy.init()
    executor = SingleThreadedExecutor()
    publisher_node = Node(
        'qos_durability_late_joiner_pub',
        start_parameter_services=False,
        enable_rosout=False,
    )
    executor.add_node(publisher_node)

    transient_qos = QoSProfile(
        history=HistoryPolicy.KEEP_LAST,
        depth=1,
        reliability=ReliabilityPolicy.RELIABLE,
        durability=DurabilityPolicy.TRANSIENT_LOCAL,
    )
    volatile_qos = QoSProfile(
        history=HistoryPolicy.KEEP_LAST,
        depth=1,
        reliability=ReliabilityPolicy.RELIABLE,
        durability=DurabilityPolicy.VOLATILE,
    )

    transient_subscriber = None
    volatile_subscriber = None
    transient_publisher = None
    volatile_publisher = None
    ok = False

    try:
        transient_publisher = publisher_node.create_publisher(
            String, 'qos_durability_transient_topic', transient_qos
        )
        transient_msg = String()
        transient_msg.data = 'transient-cached'
        transient_publisher.publish(transient_msg)
        publisher_node.get_logger().info('published transient seed')
        spin_for(executor, 0.5)

        transient_subscriber = LateJoinerSubscriber(
            'qos_durability_transient_sub',
            'qos_durability_transient_topic',
            transient_qos,
        )
        executor.add_node(transient_subscriber)
        spin_for(executor, 2.0)

        volatile_publisher = publisher_node.create_publisher(
            String, 'qos_durability_volatile_topic', volatile_qos
        )
        volatile_msg = String()
        volatile_msg.data = 'volatile-not-cached'
        volatile_publisher.publish(volatile_msg)
        publisher_node.get_logger().info('published volatile seed')
        spin_for(executor, 0.5)

        volatile_subscriber = LateJoinerSubscriber(
            'qos_durability_volatile_sub',
            'qos_durability_volatile_topic',
            volatile_qos,
        )
        executor.add_node(volatile_subscriber)
        spin_for(executor, 2.0)

        transient_ok = transient_subscriber.received_messages == ['transient-cached']
        volatile_ok = volatile_subscriber.received_messages == []
        ok = transient_ok and volatile_ok

        if transient_ok:
            publisher_node.get_logger().info(
                'transient local late joiner received cached sample'
            )
        else:
            publisher_node.get_logger().error(
                'transient local late joiner failed: '
                f'received={transient_subscriber.received_messages}'
            )

        if volatile_ok:
            publisher_node.get_logger().info(
                'volatile late joiner correctly received nothing'
            )
        else:
            publisher_node.get_logger().error(
                'volatile late joiner failed: '
                f'received={volatile_subscriber.received_messages}'
            )

        if ok:
            publisher_node.get_logger().info('qos durability late joiner ok')
    finally:
        if transient_subscriber is not None:
            executor.remove_node(transient_subscriber)
            transient_subscriber.destroy_node()
        if volatile_subscriber is not None:
            executor.remove_node(volatile_subscriber)
            volatile_subscriber.destroy_node()
        if transient_publisher is not None:
            publisher_node.destroy_publisher(transient_publisher)
        if volatile_publisher is not None:
            publisher_node.destroy_publisher(volatile_publisher)
        executor.remove_node(publisher_node)
        publisher_node.destroy_node()
        executor.shutdown(timeout_sec=1.0)
        rclpy.shutdown()

    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
