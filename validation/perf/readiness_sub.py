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


class ReadinessSubscriber(Node):
    def __init__(self, topic: str):
        super().__init__('readiness_sub')
        self.sub = self.create_subscription(String, topic, self.on_msg, 10)
        self.first_arrival_ms = None
        self.first_transport_latency_us = None

    def on_msg(self, msg: String):
        if self.first_arrival_ms is not None:
            return
        try:
            pub_start_ns = int(msg.data.strip())
        except Exception:
            return
        now_ns = time.monotonic_ns()
        self.first_arrival_ms = (now_ns - pub_start_ns) / 1e6
        self.first_transport_latency_us = (now_ns - pub_start_ns) / 1000.0

    def run(self, timeout_sec: float, hold_after_match_sec: float):
        deadline = time.monotonic() + timeout_sec
        while time.monotonic() < deadline:
            if self.first_arrival_ms is not None:
                print(
                    'READINESS_DATA '
                    f'first_arrival_ms={self.first_arrival_ms:.3f} '
                    f'first_sample_total_us={self.first_transport_latency_us:.3f}'
                )
                hold_deadline = time.monotonic() + hold_after_match_sec
                while time.monotonic() < hold_deadline:
                    rclpy.spin_once(self, timeout_sec=0.05)
                return
            rclpy.spin_once(self, timeout_sec=0.05)
        print('READINESS_DATA first_arrival_ms=TIMEOUT')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--topic', default='/perf_ready')
    parser.add_argument('--timeout-sec', type=float, default=10.0)
    parser.add_argument('--hold-after-match-sec', type=float, default=1.0)
    args = parser.parse_args()

    rclpy.init()
    node = ReadinessSubscriber(args.topic)
    try:
        node.run(args.timeout_sec, args.hold_after_match_sec)
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
