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

import argparse
import sys
import time

import rclpy
from rclpy.duration import Duration
try:
    from rclpy.event_handler import PublisherEventCallbacks
except ModuleNotFoundError:
    from rclpy.qos_event import PublisherEventCallbacks
from rclpy.executors import SingleThreadedExecutor
from rclpy.node import Node
from rclpy.qos import DurabilityPolicy, HistoryPolicy, QoSProfile, ReliabilityPolicy
from std_msgs.msg import String


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument('--topic', required=True)
    parser.add_argument('--deadline-sec', type=float, required=True)
    parser.add_argument('--publish-period', type=float, default=0.5)
    parser.add_argument('--seed-prefix', default='tick')
    parser.add_argument('--seed-delay', type=float, default=0.0)
    parser.add_argument('--repeat-count', type=int, default=3)
    parser.add_argument('--keep-alive', type=float, default=3.0)
    parser.add_argument('--wait-for-subscriber-count', type=int, default=0)
    parser.add_argument('--wait-timeout', type=float, default=5.0)
    parser.add_argument('--enable-deadline-events', action='store_true')
    parser.add_argument('--enable-incompatible-events', action='store_true')
    return parser.parse_args()


class DeadlinePublisher(Node):
    def __init__(self, args: argparse.Namespace) -> None:
        super().__init__('qos_deadline_pub', start_parameter_services=False, enable_rosout=False)
        self.args = args
        self.start_time = time.time()
        self.publish_count = 0
        self.deadline_events = []
        self.incompatible_events = []
        self.done = False

        qos = QoSProfile(
            history=HistoryPolicy.KEEP_LAST,
            depth=10,
            reliability=ReliabilityPolicy.RELIABLE,
            durability=DurabilityPolicy.VOLATILE,
            deadline=Duration(seconds=args.deadline_sec),
        )
        callbacks = PublisherEventCallbacks(
            deadline=self._deadline_callback if args.enable_deadline_events else None,
            incompatible_qos=(
                self._incompatible_callback if args.enable_incompatible_events else None),
        )
        self.publisher = self.create_publisher(
            String, args.topic, qos, event_callbacks=callbacks
        )
        self.observation_timer = self.create_timer(0.5, self._observe)
        self.publish_timer = self.create_timer(max(args.seed_delay, 0.01), self._publish_once)
        self.shutdown_timer = self.create_timer(
            max(args.seed_delay + args.keep_alive, 0.1),
            self._finish,
        )
        self.get_logger().info(
            'publisher ready: '
            f'topic={args.topic}, deadline={args.deadline_sec}s'
        )

    def _publish_once(self) -> None:
        if self.args.wait_for_subscriber_count > 0:
            current_count = self.count_subscribers(self.args.topic)
            if current_count < self.args.wait_for_subscriber_count:
                if time.time() - self.start_time > self.args.wait_timeout:
                    self.publish_timer.cancel()
                    self.observation_timer.cancel()
                    self.shutdown_timer.cancel()
                    self.done = True
                    self.get_logger().error(
                        'publisher wait-for-match failed: '
                        f'expected={self.args.wait_for_subscriber_count}, current={current_count}'
                    )
                return

        msg = String()
        msg.data = f'{self.args.seed_prefix}-{self.publish_count}'
        self.publisher.publish(msg)
        self.get_logger().info(f'published: {msg.data}')
        self.publish_count += 1

        if self.publish_count >= self.args.repeat_count:
            self.publish_timer.cancel()
        else:
            self.publish_timer.timer_period_ns = int(self.args.publish_period * 1e9)

    def _deadline_callback(self, event) -> None:
        self.deadline_events.append((event.total_count, event.total_count_change))
        self.get_logger().info(
            'publisher deadline event: '
            f'total_count={event.total_count}, total_count_change={event.total_count_change}'
        )

    def _incompatible_callback(self, event) -> None:
        self.incompatible_events.append(
            (event.total_count, event.total_count_change, event.last_policy_kind)
        )
        self.get_logger().info(
            'publisher incompatible qos event: '
            f'total_count={event.total_count}, total_count_change={event.total_count_change}, '
            f'last_policy_kind={event.last_policy_kind}'
        )

    def _observe(self) -> None:
        self.get_logger().info(
            f'subscriber_count observation: {self.count_subscribers(self.args.topic)}'
        )

    def _finish(self) -> None:
        self.shutdown_timer.cancel()
        self.observation_timer.cancel()
        self.done = True
        self.get_logger().info('publisher keep-alive finished')


def main() -> int:
    args = parse_args()
    rclpy.init()
    node = DeadlinePublisher(args)
    executor = SingleThreadedExecutor()
    executor.add_node(node)

    try:
        while rclpy.ok() and not node.done:
            executor.spin_once(timeout_sec=0.1)
    finally:
        executor.remove_node(node)
        node.destroy_node()
        executor.shutdown(timeout_sec=1.0)
        rclpy.shutdown()
    return 0


if __name__ == '__main__':
    sys.exit(main())
