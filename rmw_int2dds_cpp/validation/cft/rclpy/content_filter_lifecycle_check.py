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

from example_interfaces.msg import Int32

import rclpy
from rclpy.executors import SingleThreadedExecutor
from rclpy.node import Node
from rclpy.qos import QoSProfile
from rclpy.subscription_content_filter_options import ContentFilterOptions


def spin_until(executor: SingleThreadedExecutor, predicate, timeout_sec: float) -> bool:
    deadline = time.time() + timeout_sec
    while rclpy.ok() and time.time() < deadline:
        if predicate():
            return True
        executor.spin_once(timeout_sec=0.1)
    return predicate()


def spin_for(executor: SingleThreadedExecutor, duration_sec: float) -> None:
    deadline = time.time() + duration_sec
    while rclpy.ok() and time.time() < deadline:
        executor.spin_once(timeout_sec=0.05)


def filter_tuple(subscription) -> tuple[str, list[str]]:
    options = subscription.get_content_filter()
    return options.filter_expression, list(options.expression_parameters)


def publish_values(
    publisher,
    executor: SingleThreadedExecutor,
    received: list[int],
    values: list[int],
    expected: list[int],
) -> bool:
    received.clear()
    for value in values:
        publisher.publish(Int32(data=value))
        spin_for(executor, 0.25)
    spin_for(executor, 0.25)
    return received == expected


def main() -> int:
    rclpy.init()
    executor = SingleThreadedExecutor()
    publisher_node = Node(
        'content_filter_lifecycle_pub',
        start_parameter_services=False,
        enable_rosout=False,
    )
    subscriber_node = Node(
        'content_filter_lifecycle_sub',
        start_parameter_services=False,
        enable_rosout=False,
    )
    qos = QoSProfile(depth=10)
    received: list[int] = []
    ok = False

    subscription = subscriber_node.create_subscription(
        Int32,
        '/cft_demo',
        lambda msg: received.append(msg.data),
        qos,
        content_filter_options=ContentFilterOptions('data >= %0', ['3']),
    )
    publisher = publisher_node.create_publisher(Int32, '/cft_demo', qos)
    executor.add_node(publisher_node)
    executor.add_node(subscriber_node)

    try:
        matched = spin_until(
            executor,
            lambda: (
                publisher.get_subscription_count() == 1 and
                subscriber_node.count_publishers('/cft_demo') == 1
            ),
            3.0,
        )
        if not matched:
            publisher_node.get_logger().error('content filter match failed')
            return 1

        publisher_node.get_logger().info('content filter publisher matched subscriber')

        phase1_ok = publish_values(publisher, executor, received, [1, 3, 5], [3, 5])
        phase1_filter = filter_tuple(subscription)
        publisher_node.get_logger().info(
            f'phase1_filter expr={phase1_filter[0]!r} params={phase1_filter[1]!r}'
        )
        publisher_node.get_logger().info(f'phase1_received {received}')

        subscription.set_content_filter('data >= %0', ['5'])
        spin_for(executor, 0.25)
        phase2_ok = publish_values(publisher, executor, received, [3, 5, 7], [5, 7])
        phase2_filter = filter_tuple(subscription)
        publisher_node.get_logger().info(
            f'phase2_filter expr={phase2_filter[0]!r} params={phase2_filter[1]!r}'
        )
        publisher_node.get_logger().info(f'phase2_received {received}')

        subscription.set_content_filter('', [])
        spin_for(executor, 0.25)
        phase3_ok = publish_values(publisher, executor, received, [2, 4, 6], [2, 4, 6])
        publisher_node.get_logger().info(f'phase3_enabled {subscription.is_cft_enabled}')
        publisher_node.get_logger().info(f'phase3_received {received}')

        ok = (
            phase1_ok and
            phase1_filter == ('data >= %0', ['3']) and
            phase2_ok and
            phase2_filter == ('data >= %0', ['5']) and
            phase3_ok and
            not subscription.is_cft_enabled
        )

        if ok:
            publisher_node.get_logger().info('content filter lifecycle ok')
        else:
            publisher_node.get_logger().error(
                'content filter lifecycle failed: '
                f'phase1_ok={phase1_ok} phase1_filter={phase1_filter} '
                f'phase2_ok={phase2_ok} phase2_filter={phase2_filter} '
                f'phase3_ok={phase3_ok} phase3_enabled={subscription.is_cft_enabled}'
            )
    finally:
        publisher_node.destroy_publisher(publisher)
        subscriber_node.destroy_subscription(subscription)
        executor.remove_node(subscriber_node)
        subscriber_node.destroy_node()
        executor.remove_node(publisher_node)
        publisher_node.destroy_node()
        executor.shutdown(timeout_sec=1.0)
        rclpy.shutdown()

    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
