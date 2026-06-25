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


class ReadinessPublisher(Node):
    def __init__(self, topic: str):
        super().__init__('readiness_pub')
        self.pub = self.create_publisher(String, topic, 10)

    def run(self, timeout_sec: float, period_ms: float):
        start_ns = time.monotonic_ns()
        deadline = time.monotonic() + timeout_sec
        period_sec = max(period_ms / 1000.0, 0.001)
        next_send = time.monotonic()
        sent = 0
        while time.monotonic() < deadline:
            now_mono = time.monotonic()
            if now_mono >= next_send:
                msg = String()
                # Encode publisher start time so the subscriber can measure
                # "publisher start -> first actual sample arrival".
                msg.data = str(start_ns)
                self.pub.publish(msg)
                sent += 1
                next_send += period_sec
            rclpy.spin_once(self, timeout_sec=0.01)
        print(f'READINESS_PUB sent={sent} period_ms={period_ms:g}')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--topic', default='/perf_ready')
    parser.add_argument('--timeout-sec', type=float, default=10.0)
    parser.add_argument('--period-ms', type=float, default=20.0)
    args = parser.parse_args()

    rclpy.init()
    node = ReadinessPublisher(args.topic)
    try:
        node.run(args.timeout_sec, args.period_ms)
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
