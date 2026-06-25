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

import rclpy
from rclpy.executors import SingleThreadedExecutor
from rclpy.node import Node
from rclpy.qos import (DurabilityPolicy, HistoryPolicy, LivelinessPolicy, QoSProfile,
                       ReliabilityPolicy)
from std_msgs.msg import String


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument('--topic', required=True)
    parser.add_argument('--durability', default='transient_local')
    parser.add_argument('--timeout', type=float, default=6.0)
    parser.add_argument('--expect', default=None)
    parser.add_argument('--expect-none', action='store_true')
    parser.add_argument('--require-publisher-observed', action='store_true')
    return parser.parse_args()


def parse_durability(value: str) -> DurabilityPolicy:
    if value == 'volatile':
        return DurabilityPolicy.VOLATILE
    return DurabilityPolicy.TRANSIENT_LOCAL


class LifespanSubscriber(Node):
    def __init__(self, args: argparse.Namespace) -> None:
        super().__init__('qos_lifespan_sub', start_parameter_services=False, enable_rosout=False)
        self.args = args
        self.done = False
        self.received_messages: list[str] = []
        self.publisher_observed = False
        self.max_publisher_count = 0
        qos = QoSProfile(
            history=HistoryPolicy.KEEP_LAST,
            depth=10,
            reliability=ReliabilityPolicy.RELIABLE,
            durability=parse_durability(args.durability),
            liveliness=LivelinessPolicy.AUTOMATIC,
        )
        self.subscription = self.create_subscription(String, args.topic, self._callback, qos)
        self.observe_timer = self.create_timer(0.5, self._observe)
        self.finish_timer = self.create_timer(max(args.timeout, 0.1), self._finish)
        self.get_logger().info(
            'subscriber ready: '
            f'topic={args.topic}, durability={args.durability}'
        )

    def _callback(self, msg: String) -> None:
        self.received_messages.append(msg.data)
        self.get_logger().info(f'subscriber heard: {msg.data}')

    def _observe(self) -> None:
        current_count = self.count_publishers(self.args.topic)
        self.max_publisher_count = max(self.max_publisher_count, current_count)
        if current_count > 0:
            self.publisher_observed = True
        self.get_logger().info(
            f'publisher_count observation: {current_count}'
        )

    def _finish(self) -> None:
        self.finish_timer.cancel()
        self.observe_timer.cancel()
        self.done = True

    def evaluate(self) -> bool:
        if self.args.expect_none:
            ok = len(self.received_messages) == 0
            if self.args.require_publisher_observed:
                ok = ok and self.publisher_observed
            if ok:
                if self.args.require_publisher_observed:
                    self.get_logger().info(
                        'lifespan subscriber ok: received nothing after observing publisher'
                    )
                else:
                    self.get_logger().info('lifespan subscriber ok: received nothing')
            else:
                self.get_logger().error(
                    'lifespan subscriber failed: '
                    f'received={self.received_messages}, '
                    f'publisher_observed={self.publisher_observed}, '
                    f'max_publisher_count={self.max_publisher_count}'
                )
            return ok

        ok = self.args.expect is not None and self.received_messages == [self.args.expect]
        if ok:
            self.get_logger().info(f'lifespan subscriber ok: expected={self.args.expect}')
        else:
            self.get_logger().error(
                'lifespan subscriber failed: '
                f'expected={self.args.expect}, received={self.received_messages}'
            )
        return ok


def main() -> int:
    args = parse_args()
    rclpy.init()
    node = LifespanSubscriber(args)
    executor = SingleThreadedExecutor()
    executor.add_node(node)
    try:
        while rclpy.ok() and not node.done:
            executor.spin_once(timeout_sec=0.1)
        ok = node.evaluate()
    finally:
        executor.remove_node(node)
        node.destroy_node()
        executor.shutdown(timeout_sec=1.0)
        rclpy.shutdown()
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
