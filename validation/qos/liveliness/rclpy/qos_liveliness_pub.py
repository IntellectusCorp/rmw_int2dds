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
from rclpy.executors import SingleThreadedExecutor
from rclpy.node import Node
from rclpy.qos import (DurabilityPolicy, HistoryPolicy, LivelinessPolicy, QoSProfile,
                       ReliabilityPolicy)
from std_msgs.msg import String


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument('--topic', required=True)
    parser.add_argument('--liveliness', default='automatic')
    parser.add_argument('--lease-sec', type=float, required=True)
    parser.add_argument('--seed-prefix', default='alive')
    parser.add_argument('--seed-delay', type=float, default=0.1)
    parser.add_argument('--repeat-count', type=int, default=1)
    parser.add_argument('--repeat-period', type=float, default=0.5)
    parser.add_argument('--assert-period', type=float, default=0.0)
    parser.add_argument('--keep-alive', type=float, default=2.0)
    parser.add_argument('--wait-for-subscriber-count', type=int, default=0)
    parser.add_argument('--wait-timeout', type=float, default=5.0)
    return parser.parse_args()


def parse_liveliness(value: str) -> LivelinessPolicy:
    if value == 'manual_by_topic':
        return LivelinessPolicy.MANUAL_BY_TOPIC
    return LivelinessPolicy.AUTOMATIC


class LivelinessPublisher(Node):
    def __init__(self, args: argparse.Namespace) -> None:
        super().__init__('qos_liveliness_pub', start_parameter_services=False, enable_rosout=False)
        self.args = args
        self.start_time = time.time()
        self.done = False
        self.publish_count = 0
        self.assert_count = 0

        qos = QoSProfile(
            history=HistoryPolicy.KEEP_LAST,
            depth=10,
            reliability=ReliabilityPolicy.RELIABLE,
            durability=DurabilityPolicy.VOLATILE,
            liveliness=parse_liveliness(args.liveliness),
            liveliness_lease_duration=Duration(seconds=args.lease_sec),
        )

        self.publisher = self.create_publisher(String, args.topic, qos)
        self.observe_timer = self.create_timer(0.5, self._observe)
        self.publish_timer = self.create_timer(max(args.seed_delay, 0.01), self._publish_once)
        self.assert_timer = None
        if args.assert_period > 0.0:
            self.assert_timer = self.create_timer(
                max(args.assert_period, 0.01), self._assert_liveliness)
        self.shutdown_timer = self.create_timer(
            max(args.seed_delay + args.keep_alive, 0.1), self._finish
        )
        self.get_logger().info(
            'publisher ready: '
            f'topic={args.topic}, liveliness={args.liveliness}, lease={args.lease_sec}s, '
            f'assert_period={args.assert_period}s'
        )

    def _observe(self) -> None:
        self.get_logger().info(
            f'subscriber_count observation: {self.count_subscribers(self.args.topic)}'
        )

    def _publish_once(self) -> None:
        if self.args.wait_for_subscriber_count > 0:
            current_count = self.count_subscribers(self.args.topic)
            if current_count < self.args.wait_for_subscriber_count:
                if time.time() - self.start_time > self.args.wait_timeout:
                    self.publish_timer.cancel()
                    self.observe_timer.cancel()
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
            self.publish_timer.timer_period_ns = int(self.args.repeat_period * 1e9)

    def _assert_liveliness(self) -> None:
        if self.publish_count == 0:
            return
        self.publisher.assert_liveliness()
        self.assert_count += 1
        self.get_logger().info(f'asserted liveliness #{self.assert_count}')

    def _finish(self) -> None:
        self.shutdown_timer.cancel()
        self.observe_timer.cancel()
        if self.assert_timer is not None:
            self.assert_timer.cancel()
        self.done = True
        self.get_logger().info('publisher keep-alive finished')


def main() -> int:
    args = parse_args()
    rclpy.init()
    node = LivelinessPublisher(args)
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
