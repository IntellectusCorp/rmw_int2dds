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
from rclpy.duration import Duration
from rclpy.executors import SingleThreadedExecutor
from rclpy.node import Node
from rclpy.qos import (DurabilityPolicy, HistoryPolicy, LivelinessPolicy, QoSProfile,
                       ReliabilityPolicy)
from std_msgs.msg import String


class RecordingSubscriber(Node):
    def __init__(self, name: str, context: rclpy.context.Context) -> None:
        super().__init__(
            name,
            context=context,
            start_parameter_services=False,
            enable_rosout=False,
        )
        self.received_messages: list[str] = []
        qos = QoSProfile(
            history=HistoryPolicy.KEEP_LAST,
            depth=10,
            reliability=ReliabilityPolicy.RELIABLE,
            durability=DurabilityPolicy.TRANSIENT_LOCAL,
            liveliness=LivelinessPolicy.AUTOMATIC,
        )
        self.subscription = self.create_subscription(
            String,
            'qos_lifespan_topic',
            self._callback,
            qos,
        )

    def _callback(self, msg: String) -> None:
        self.received_messages.append(msg.data)
        self.get_logger().info(f'subscriber heard: {msg.data}')


def spin_until(executor: SingleThreadedExecutor, condition, timeout_sec: float) -> bool:
    deadline = time.time() + timeout_sec
    while time.time() < deadline:
        executor.spin_once(timeout_sec=0.1)
        if condition():
            return True
    return False


def main() -> int:
    pub_context = rclpy.context.Context()
    early_context = rclpy.context.Context()
    late_context = rclpy.context.Context()
    rclpy.init(context=pub_context)
    rclpy.init(context=early_context)
    rclpy.init(context=late_context)

    publisher_node = None
    early_sub = None
    late_sub = None
    publisher = None
    pub_executor = SingleThreadedExecutor(context=pub_context)
    early_executor = SingleThreadedExecutor(context=early_context)
    late_executor = SingleThreadedExecutor(context=late_context)
    ok = False

    try:
        early_sub = RecordingSubscriber('qos_lifespan_early_sub', early_context)
        early_executor.add_node(early_sub)

        publisher_node = Node(
            'qos_lifespan_pub',
            context=pub_context,
            start_parameter_services=False,
            enable_rosout=False,
        )
        pub_executor.add_node(publisher_node)

        publisher_qos = QoSProfile(
            history=HistoryPolicy.KEEP_LAST,
            depth=10,
            reliability=ReliabilityPolicy.RELIABLE,
            durability=DurabilityPolicy.TRANSIENT_LOCAL,
            liveliness=LivelinessPolicy.AUTOMATIC,
            lifespan=Duration(seconds=0.5),
        )
        publisher = publisher_node.create_publisher(String, 'qos_lifespan_topic', publisher_qos)

        matched = spin_until(
            early_executor,
            lambda: publisher_node.count_subscribers('qos_lifespan_topic') == 1
            and early_sub.count_publishers('qos_lifespan_topic') == 1,
            timeout_sec=5.0,
        )
        if not matched:
            publisher_node.get_logger().error('lifespan publisher match failed')
            return 1

        publisher_node.get_logger().info('lifespan publisher matched early subscriber')

        msg = String()
        msg.data = 'lifespan-alive'
        publisher.publish(msg)
        publisher_node.get_logger().info('published seed: lifespan-alive')

        delivered = spin_until(
            early_executor,
            lambda: early_sub.received_messages == ['lifespan-alive'],
            timeout_sec=2.0,
        )
        if not delivered:
            early_sub.get_logger().error(
                f'early subscriber delivery failed: received={early_sub.received_messages}'
            )
            return 1

        publisher_node.get_logger().info('sleeping past lifespan before creating late subscriber')
        time.sleep(1.0)

        late_sub = RecordingSubscriber('qos_lifespan_late_sub', late_context)
        late_executor.add_node(late_sub)

        matched_late = spin_until(
            late_executor,
            lambda: late_sub.count_publishers('qos_lifespan_topic') == 1,
            timeout_sec=5.0,
        )
        if not matched_late:
            late_sub.get_logger().error('late subscriber did not observe publisher')
            return 1

        no_cached_sample = not spin_until(
            late_executor,
            lambda: len(late_sub.received_messages) > 0,
            timeout_sec=1.0,
        )

        if no_cached_sample:
            publisher_node.get_logger().info('qos lifespan ok')
            ok = True
        else:
            publisher_node.get_logger().error(
                'qos lifespan failed: '
                f'early_received={early_sub.received_messages}, '
                f'late_received={late_sub.received_messages}'
            )
            ok = False
    finally:
        if publisher is not None and publisher_node is not None:
            publisher_node.destroy_publisher(publisher)
        if publisher_node is not None:
            pub_executor.remove_node(publisher_node)
            publisher_node.destroy_node()
        if early_sub is not None:
            early_executor.remove_node(early_sub)
            early_sub.destroy_node()
        if late_sub is not None:
            late_executor.remove_node(late_sub)
            late_sub.destroy_node()
        pub_executor.shutdown(timeout_sec=1.0)
        early_executor.shutdown(timeout_sec=1.0)
        late_executor.shutdown(timeout_sec=1.0)
        rclpy.shutdown(context=pub_context)
        rclpy.shutdown(context=early_context)
        rclpy.shutdown(context=late_context)

    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
