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
    parser.add_argument('--burst-count', type=int, default=5)
    parser.add_argument('--publish-period', type=float, default=0.05)
    parser.add_argument('--wait-for-subscriber-count', type=int, default=1)
    parser.add_argument('--wait-timeout', type=float, default=10.0)
    parser.add_argument('--keep-alive', type=float, default=2.0)
    parser.add_argument('--prefix', default='burst')
    return parser.parse_args()


class HistoryDepthPublisher(Node):
    def __init__(self, args):
        super().__init__('qos_history_depth_pub')
        qos = QoSProfile(
            history=QoSHistoryPolicy.KEEP_LAST,
            depth=args.depth,
        )
        self.publisher = self.create_publisher(String, args.topic, qos)
        self.args = args

    def wait_for_subscriber(self):
        deadline = time.monotonic() + self.args.wait_timeout
        while time.monotonic() < deadline:
            count = self.publisher.get_subscription_count()
            self.get_logger().info(f'subscriber_count observation: {count}')
            if count >= self.args.wait_for_subscriber_count:
                return True
            rclpy.spin_once(self, timeout_sec=0.1)
            time.sleep(0.4)
        return False

    def publish_burst(self):
        for i in range(self.args.burst_count):
            msg = String()
            msg.data = f'{self.args.prefix}-{i}'
            self.publisher.publish(msg)
            self.get_logger().info(f'published: {msg.data}')
            time.sleep(self.args.publish_period)


def main():
    args = parse_args()
    rclpy.init()
    node = HistoryDepthPublisher(args)
    node.get_logger().info(
        f'publisher ready: topic={args.topic}, history=keep_last, depth={args.depth}, '
        f'burst_count={args.burst_count}'
    )
    if not node.wait_for_subscriber():
        node.get_logger().error('publisher failed: subscriber did not appear in time')
        node.destroy_node()
        rclpy.shutdown()
        raise SystemExit(1)
    node.get_logger().info('history/depth publisher matched subscriber')
    node.publish_burst()
    node.get_logger().info(
        f'published {args.prefix}-0..{args.prefix}-{args.burst_count - 1} burst'
    )
    keep_deadline = time.monotonic() + args.keep_alive
    while time.monotonic() < keep_deadline:
        count = node.publisher.get_subscription_count()
        node.get_logger().info(f'subscriber_count observation: {count}')
        rclpy.spin_once(node, timeout_sec=0.1)
        time.sleep(0.4)
    node.get_logger().info('publisher keep-alive finished')
    node.destroy_node()
    rclpy.shutdown()


if __name__ == '__main__':
    main()
