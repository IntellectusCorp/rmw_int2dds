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


class DepthSubscriber(Node):
    def __init__(self, topic: str, qos: QoSProfile) -> None:
        super().__init__(
            'qos_history_depth_sub', start_parameter_services=False, enable_rosout=False)
        self.received_messages: list[str] = []
        self.create_subscription(String, topic, self._callback, qos)

    def _callback(self, msg: String) -> None:
        self.received_messages.append(msg.data)
        self.get_logger().info(f'depth subscriber heard: {msg.data}')


def spin_until(executor: SingleThreadedExecutor, predicate, timeout_sec: float) -> bool:
    end = time.time() + timeout_sec
    while rclpy.ok() and time.time() < end:
        if predicate():
            return True
        executor.spin_once(timeout_sec=0.1)
    return predicate()


def spin_for(executor: SingleThreadedExecutor, duration_sec: float) -> None:
    end = time.time() + duration_sec
    while rclpy.ok() and time.time() < end:
        executor.spin_once(timeout_sec=0.1)


def main() -> int:
    rclpy.init()
    executor = SingleThreadedExecutor()
    publisher_node = Node(
        'qos_history_depth_pub',
        start_parameter_services=False,
        enable_rosout=False,
    )
    subscriber_qos = QoSProfile(
        history=HistoryPolicy.KEEP_LAST,
        depth=1,
        reliability=ReliabilityPolicy.RELIABLE,
        durability=DurabilityPolicy.VOLATILE,
    )
    publisher_qos = QoSProfile(
        history=HistoryPolicy.KEEP_LAST,
        depth=5,
        reliability=ReliabilityPolicy.RELIABLE,
        durability=DurabilityPolicy.VOLATILE,
    )

    subscriber = DepthSubscriber('qos_history_depth_topic', subscriber_qos)
    publisher = None
    ok = False

    executor.add_node(publisher_node)
    executor.add_node(subscriber)

    try:
        publisher = publisher_node.create_publisher(
            String, 'qos_history_depth_topic', publisher_qos)
        matched = spin_until(
            executor,
            lambda: publisher_node.count_subscribers('qos_history_depth_topic') == 1 and
            subscriber.count_publishers('qos_history_depth_topic') == 1,
            3.0,
        )
        if matched:
            publisher_node.get_logger().info('history/depth publisher matched subscriber')
        else:
            publisher_node.get_logger().error('history/depth match failed')
            return 1

        for index in range(5):
            msg = String()
            msg.data = f'burst-{index}'
            publisher.publish(msg)
        publisher_node.get_logger().info('published burst-0..4 without spinning subscriber')

        spin_for(executor, 1.0)

        ok = subscriber.received_messages == ['burst-4']
        if ok:
            publisher_node.get_logger().info('keep_last depth=1 retained only the latest sample')
            publisher_node.get_logger().info('qos history depth ok')
        else:
            publisher_node.get_logger().error(
                f'qos history depth failed: received={subscriber.received_messages}'
            )
    finally:
        if publisher is not None:
            publisher_node.destroy_publisher(publisher)
        executor.remove_node(subscriber)
        subscriber.destroy_node()
        executor.remove_node(publisher_node)
        publisher_node.destroy_node()
        executor.shutdown(timeout_sec=1.0)
        rclpy.shutdown()

    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
