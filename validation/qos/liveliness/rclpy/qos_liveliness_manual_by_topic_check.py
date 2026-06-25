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
try:
    from rclpy.event_handler import SubscriptionEventCallbacks
except ModuleNotFoundError:
    from rclpy.qos_event import SubscriptionEventCallbacks
from rclpy.executors import SingleThreadedExecutor
from rclpy.node import Node
from rclpy.qos import (DurabilityPolicy, HistoryPolicy, LivelinessPolicy, QoSProfile,
                       ReliabilityPolicy)
from std_msgs.msg import String


class LivelinessSubscriber(Node):
    def __init__(self, context: rclpy.context.Context) -> None:
        super().__init__(
            'qos_liveliness_sub',
            context=context,
            start_parameter_services=False,
            enable_rosout=False,
        )
        self.received_messages: list[str] = []
        self.liveliness_events: list[tuple[int, int, int, int]] = []
        qos = QoSProfile(
            history=HistoryPolicy.KEEP_LAST,
            depth=10,
            reliability=ReliabilityPolicy.RELIABLE,
            durability=DurabilityPolicy.VOLATILE,
            liveliness=LivelinessPolicy.MANUAL_BY_TOPIC,
            liveliness_lease_duration=Duration(seconds=0.5),
        )
        callbacks = SubscriptionEventCallbacks(liveliness=self._liveliness_callback)
        self.subscription = self.create_subscription(
            String,
            'qos_liveliness_topic_manual',
            self._callback,
            qos,
            event_callbacks=callbacks,
        )

    def _callback(self, msg: String) -> None:
        self.received_messages.append(msg.data)
        self.get_logger().info(f'subscriber heard: {msg.data}')

    def _liveliness_callback(self, event) -> None:
        info = (
            event.alive_count,
            event.not_alive_count,
            event.alive_count_change,
            event.not_alive_count_change,
        )
        self.liveliness_events.append(info)
        self.get_logger().info(
            'subscription liveliness event: '
            f'alive_count={event.alive_count}, '
            f'not_alive_count={event.not_alive_count}, '
            f'alive_count_change={event.alive_count_change}, '
            f'not_alive_count_change={event.not_alive_count_change}'
        )


def spin_until(executor: SingleThreadedExecutor, condition, timeout_sec: float) -> bool:
    deadline = time.time() + timeout_sec
    while time.time() < deadline:
        executor.spin_once(timeout_sec=0.1)
        if condition():
            return True
    return False


def main() -> int:
    pub_context = rclpy.context.Context()
    sub_context = rclpy.context.Context()
    rclpy.init(context=pub_context)
    rclpy.init(context=sub_context)

    publisher_node = None
    subscriber = None
    pub_executor = SingleThreadedExecutor(context=pub_context)
    sub_executor = SingleThreadedExecutor(context=sub_context)
    publisher = None
    ok = False

    try:
        subscriber = LivelinessSubscriber(sub_context)
        sub_executor.add_node(subscriber)

        publisher_node = Node(
            'qos_liveliness_pub',
            context=pub_context,
            start_parameter_services=False,
            enable_rosout=False,
        )
        pub_executor.add_node(publisher_node)

        publisher_qos = QoSProfile(
            history=HistoryPolicy.KEEP_LAST,
            depth=10,
            reliability=ReliabilityPolicy.RELIABLE,
            durability=DurabilityPolicy.VOLATILE,
            liveliness=LivelinessPolicy.MANUAL_BY_TOPIC,
            liveliness_lease_duration=Duration(seconds=0.5),
        )
        publisher = publisher_node.create_publisher(
            String, 'qos_liveliness_topic_manual', publisher_qos
        )

        matched = spin_until(
            sub_executor,
            lambda: publisher_node.count_subscribers('qos_liveliness_topic_manual') == 1
            and subscriber.count_publishers('qos_liveliness_topic_manual') == 1,
            timeout_sec=5.0,
        )
        if not matched:
            publisher_node.get_logger().error('manual liveliness match failed')
            return 1

        publisher_node.get_logger().info('manual liveliness publisher matched subscriber')

        msg = String()
        msg.data = 'alive'
        publisher.publish(msg)
        publisher_node.get_logger().info('published seed: alive')

        delivered = spin_until(
            sub_executor,
            lambda: subscriber.received_messages == ['alive'],
            timeout_sec=2.0,
        )
        if not delivered:
            subscriber.get_logger().error(
                f'manual liveliness delivery failed: received={subscriber.received_messages}'
            )
            return 1

        pre_assert_event_count = len(subscriber.liveliness_events)
        assert_deadline = time.time() + 1.5
        assert_count = 0
        while time.time() < assert_deadline:
            publisher.assert_liveliness()
            assert_count += 1
            publisher_node.get_logger().info(f'asserted liveliness #{assert_count}')
            pub_executor.spin_once(timeout_sec=0.05)
            sub_executor.spin_once(timeout_sec=0.05)
            time.sleep(0.2)

        if len(subscriber.liveliness_events) != pre_assert_event_count:
            subscriber.get_logger().error(
                'manual liveliness asserted period unexpectedly produced event: '
                f'events={subscriber.liveliness_events}'
            )
            return 1

        subscriber.get_logger().info(
            'stopped asserting publisher liveliness; waiting for lease expiration'
        )

        observed_not_alive = spin_until(
            sub_executor,
            lambda: any(evt[3] > 0 for evt in subscriber.liveliness_events),
            timeout_sec=3.0,
        )

        if observed_not_alive and subscriber.received_messages == ['alive']:
            subscriber.get_logger().info('qos liveliness manual_by_topic ok')
            ok = True
        else:
            subscriber.get_logger().error(
                'qos liveliness manual_by_topic failed: '
                f'events={subscriber.liveliness_events}, '
                f'received={subscriber.received_messages}'
            )
            ok = False
    finally:
        if publisher is not None and publisher_node is not None:
            publisher_node.destroy_publisher(publisher)
        if publisher_node is not None:
            pub_executor.remove_node(publisher_node)
            publisher_node.destroy_node()
        if subscriber is not None:
            sub_executor.remove_node(subscriber)
            subscriber.destroy_node()
        pub_executor.shutdown(timeout_sec=1.0)
        sub_executor.shutdown(timeout_sec=1.0)
        rclpy.shutdown(context=pub_context)
        rclpy.shutdown(context=sub_context)

    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
