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
import time

import rclpy
from rclpy.node import Node
from rclpy.qos import QoSHistoryPolicy
from rclpy.qos import QoSProfile
from std_msgs.msg import String


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument('--topic', required=True)
    parser.add_argument('--depth', type=int, default=1)
    parser.add_argument('--expect-latest', required=True)
    parser.add_argument('--timeout', type=float, default=10.0)
    parser.add_argument('--drain-after', type=float, default=1.0)
    return parser.parse_args()


class HistoryDepthSubscriber(Node):
    def __init__(self, args):
        super().__init__('qos_history_depth_sub')
        qos = QoSProfile(
            history=QoSHistoryPolicy.KEEP_LAST,
            depth=args.depth,
        )
        self.subscription = self.create_subscription(
            String, args.topic, self.callback, qos)
        self.publisher_count_observed = 0
        self.received = []
        self.args = args
        self.started_draining = False

    def callback(self, msg):
        self.received.append(msg.data)
        self.get_logger().info(f'received: {msg.data}')

    def publisher_count(self):
        return self.subscription.get_publisher_count()


def main():
    args = parse_args()
    rclpy.init()
    node = HistoryDepthSubscriber(args)
    node.get_logger().info(
        f'subscriber ready: topic={args.topic}, history=keep_last, depth={args.depth}'
    )
    start = time.monotonic()
    drain_start = None

    while rclpy.ok() and time.monotonic() - start < args.timeout:
        count = node.publisher_count()
        node.get_logger().info(f'publisher_count observation: {count}')
        if count > 0 and drain_start is None:
            drain_start = time.monotonic() + args.drain_after
        if drain_start is not None and time.monotonic() >= drain_start:
            node.started_draining = True
            break
        time.sleep(0.5)

    if not node.started_draining:
        node.get_logger().error('history/depth subscriber failed: publisher not observed in time')
        node.destroy_node()
        rclpy.shutdown()
        raise SystemExit(1)

    drain_deadline = time.monotonic() + 2.0
    while rclpy.ok() and time.monotonic() < drain_deadline:
        rclpy.spin_once(node, timeout_sec=0.1)
        time.sleep(0.05)

    if not node.received:
        node.get_logger().error('history/depth subscriber failed: received_count=0')
        node.destroy_node()
        rclpy.shutdown()
        raise SystemExit(1)

    latest = node.received[-1]
    node.get_logger().info(f'depth subscriber heard: {latest}')
    if latest != args.expect_latest or len(node.received) != 1:
        node.get_logger().error(
            f'history/depth subscriber failed: expected only {args.expect_latest}, '
            f'received={node.received}'
        )
        node.destroy_node()
        rclpy.shutdown()
        raise SystemExit(1)

    node.get_logger().info(
        f'history/depth subscriber ok: retained latest sample only ({args.expect_latest})'
    )
    node.destroy_node()
    rclpy.shutdown()


if __name__ == '__main__':
    main()
