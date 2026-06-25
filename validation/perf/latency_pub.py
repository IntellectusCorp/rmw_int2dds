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
from std_msgs.msg import String


class LatencyPublisher(Node):
    def __init__(self, topic: str, rate_hz: float, count: int):
        super().__init__('latency_pub')
        self.pub = self.create_publisher(String, topic, 10)
        self.rate_hz = rate_hz
        self.count = count

    def run(self):
        # Give the subscriber a short window to match so the first few samples
        # are not dominated by startup timing.
        deadline = time.monotonic() + 5.0
        while self.pub.get_subscription_count() == 0 and time.monotonic() < deadline:
            rclpy.spin_once(self, timeout_sec=0.05)
        period = 1.0 / self.rate_hz if self.rate_hz > 0 else 0.0
        next_time = time.monotonic()
        for seq in range(1, self.count + 1):
            now_ns = time.monotonic_ns()
            msg = String()
            msg.data = f'{seq},{now_ns}'
            self.pub.publish(msg)
            if period > 0:
                next_time += period
                sleep_for = next_time - time.monotonic()
                if sleep_for > 0:
                    time.sleep(sleep_for)
        self.get_logger().info(
            f'LATENCY_PUB sent={self.count} rate_hz={self.rate_hz:g}'
        )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--topic', default='/perf_latency')
    parser.add_argument('--rate-hz', type=float, default=100.0)
    parser.add_argument('--count', type=int, default=200)
    args = parser.parse_args()

    rclpy.init()
    node = LatencyPublisher(args.topic, args.rate_hz, args.count)
    try:
        node.run()
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
