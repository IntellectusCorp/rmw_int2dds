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
import math
import time

import rclpy
from rclpy.node import Node
from std_msgs.msg import String


def percentile(sorted_values, q: float) -> float:
    if not sorted_values:
        return 0.0
    if len(sorted_values) == 1:
        return sorted_values[0]
    idx = q * (len(sorted_values) - 1)
    lo = math.floor(idx)
    hi = math.ceil(idx)
    if lo == hi:
        return sorted_values[lo]
    frac = idx - lo
    return sorted_values[lo] * (1.0 - frac) + sorted_values[hi] * frac


class LatencySubscriber(Node):
    def __init__(self, topic: str, expected: int):
        super().__init__('latency_sub')
        self.expected = expected
        self.samples_us = []
        self.received = 0
        self.create_subscription(String, topic, self.on_msg, 10)

    def on_msg(self, msg: String):
        try:
            _seq, sent_ns = msg.data.split(',', 1)
            sent_ns = int(sent_ns)
        except Exception:
            return
        now_ns = time.monotonic_ns()
        self.samples_us.append((now_ns - sent_ns) / 1000.0)
        self.received += 1

    def print_summary(self):
        samples = sorted(self.samples_us)
        avg = sum(samples) / len(samples) if samples else 0.0
        print(
            'LATENCY '
            f'samples={len(samples)} '
            f'avg_us={avg:.3f} '
            f'p50_us={percentile(samples, 0.50):.3f} '
            f'p95_us={percentile(samples, 0.95):.3f} '
            f'p99_us={percentile(samples, 0.99):.3f} '
            f'min_us={samples[0]:.3f} '
            f'max_us={samples[-1]:.3f}'
        )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--topic', default='/perf_latency')
    parser.add_argument('--expected', type=int, default=200)
    parser.add_argument('--timeout-sec', type=float, default=10.0)
    args = parser.parse_args()

    rclpy.init()
    node = LatencySubscriber(args.topic, args.expected)
    deadline = time.monotonic() + args.timeout_sec
    try:
        while rclpy.ok() and time.monotonic() < deadline and node.received < node.expected:
            rclpy.spin_once(node, timeout_sec=0.1)
        node.print_summary()
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
