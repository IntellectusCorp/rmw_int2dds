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
try:
    from rclpy.event_handler import PublisherEventCallbacks, SubscriptionEventCallbacks
except ModuleNotFoundError:
    from rclpy.qos_event import PublisherEventCallbacks, SubscriptionEventCallbacks
from rclpy.duration import Duration
from rclpy.executors import SingleThreadedExecutor
from rclpy.node import Node
from rclpy.qos import DurabilityPolicy, HistoryPolicy, QoSProfile, ReliabilityPolicy
from std_msgs.msg import String


class DeadlinePublisher(Node):
    def __init__(self) -> None:
        super().__init__('qos_deadline_pub', start_parameter_services=False, enable_rosout=False)
        self.deadline_events: list[tuple[int, int]] = []
        self.publish_count = 0

        qos = QoSProfile(
            history=HistoryPolicy.KEEP_LAST,
            depth=10,
            reliability=ReliabilityPolicy.RELIABLE,
            durability=DurabilityPolicy.VOLATILE,
            deadline=Duration(seconds=0.2),
        )
        callbacks = PublisherEventCallbacks(deadline=self._deadline_callback)
        self.publisher = self.create_publisher(
            String, 'qos_deadline_topic', qos, event_callbacks=callbacks
        )
        self.timer = self.create_timer(0.5, self._publish)

    def _publish(self) -> None:
        msg = String()
        msg.data = f'tick-{self.publish_count}'
        self.publisher.publish(msg)
        self.get_logger().info(f'published: {msg.data}')
        self.publish_count += 1

    def _deadline_callback(self, event) -> None:
        self.deadline_events.append((event.total_count, event.total_count_change))
        self.get_logger().info(
            'publisher deadline event: '
            f'total_count={event.total_count}, total_count_change={event.total_count_change}'
        )


class DeadlineSubscriber(Node):
    def __init__(self) -> None:
        super().__init__('qos_deadline_sub', start_parameter_services=False, enable_rosout=False)
        self.deadline_events: list[tuple[int, int]] = []
        self.received_messages: list[str] = []

        qos = QoSProfile(
            history=HistoryPolicy.KEEP_LAST,
            depth=10,
            reliability=ReliabilityPolicy.RELIABLE,
            durability=DurabilityPolicy.VOLATILE,
            deadline=Duration(seconds=0.2),
        )
        callbacks = SubscriptionEventCallbacks(deadline=self._deadline_callback)
        self.subscription = self.create_subscription(
            String, 'qos_deadline_topic', self._callback, qos, event_callbacks=callbacks
        )

    def _callback(self, msg: String) -> None:
        self.received_messages.append(msg.data)
        self.get_logger().info(f'subscriber heard: {msg.data}')

    def _deadline_callback(self, event) -> None:
        self.deadline_events.append((event.total_count, event.total_count_change))
        self.get_logger().info(
            'subscription deadline event: '
            f'total_count={event.total_count}, total_count_change={event.total_count_change}'
        )


def spin_until(executor: SingleThreadedExecutor, predicate, timeout_sec: float) -> bool:
    end = time.time() + timeout_sec
    while rclpy.ok() and time.time() < end:
        if predicate():
            return True
        executor.spin_once(timeout_sec=0.1)
    return predicate()


def main() -> int:
    rclpy.init()
    executor = SingleThreadedExecutor()
    publisher = DeadlinePublisher()
    subscriber = DeadlineSubscriber()
    executor.add_node(publisher)
    executor.add_node(subscriber)

    ok = False
    try:
        matched = spin_until(
            executor,
            lambda: publisher.count_subscribers('qos_deadline_topic') == 1 and
            subscriber.count_publishers('qos_deadline_topic') == 1,
            3.0,
        )
        if not matched:
            publisher.get_logger().error('deadline match failed')
            return 1

        ok = spin_until(
            executor,
            lambda: len(publisher.deadline_events) >= 1 and len(subscriber.deadline_events) >= 1,
            5.0,
        )

        if ok:
            publisher.get_logger().info('qos deadline event ok')
        else:
            publisher.get_logger().error(
                'qos deadline event failed: '
                f'publisher_events={publisher.deadline_events}, '
                f'subscriber_events={subscriber.deadline_events}, '
                f'received={subscriber.received_messages}'
            )
    finally:
        executor.remove_node(subscriber)
        executor.remove_node(publisher)
        subscriber.destroy_node()
        publisher.destroy_node()
        executor.shutdown(timeout_sec=1.0)
        rclpy.shutdown()

    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
