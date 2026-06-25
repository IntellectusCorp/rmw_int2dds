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
from std_msgs.msg import ByteMultiArray


class ThroughputSubscriber(Node):
    def __init__(self, topic: str, duration_sec: float):
        super().__init__('throughput_sub')
        self.duration_sec = duration_sec
        self.start_wall = time.monotonic()
        self.first_ns = None
        self.last_ns = None
        self.recv_msgs = 0
        self.recv_bytes = 0
        self.create_subscription(ByteMultiArray, topic, self.on_msg, 10)
        self.create_timer(0.1, self.on_timer)

    def on_msg(self, msg: ByteMultiArray):
        now_ns = time.monotonic_ns()
        if self.first_ns is None:
            self.first_ns = now_ns
        self.last_ns = now_ns
        self.recv_msgs += 1
        self.recv_bytes += len(msg.data)

    def print_summary(self):
        if self.first_ns is None or self.last_ns is None:
            elapsed = self.duration_sec
        else:
            elapsed = max((self.last_ns - self.first_ns) / 1e9, 1e-9)
        print(
            'THROUGHPUT_SUB '
            f'duration_s={elapsed:.3f} '
            f'recv_msgs={self.recv_msgs} '
            f'recv_mps={self.recv_msgs/elapsed:.3f} '
            f'recv_MBps={self.recv_bytes/elapsed/1_000_000.0:.3f}'
        )

    def on_timer(self):
        if time.monotonic() - self.start_wall >= self.duration_sec:
            self.print_summary()
            self.destroy_node()
            rclpy.shutdown()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--topic', default='/perf_throughput')
    parser.add_argument('--duration-sec', type=float, default=5.0)
    parser.add_argument('--timeout-sec', type=float, default=10.0)
    args = parser.parse_args()

    rclpy.init()
    node = ThroughputSubscriber(args.topic, args.duration_sec)
    try:
        deadline = time.monotonic() + args.timeout_sec
        while rclpy.ok() and time.monotonic() < deadline:
            rclpy.spin_once(node, timeout_sec=0.1)
    finally:
        if rclpy.ok():
            node.print_summary()
            node.destroy_node()
            rclpy.shutdown()


if __name__ == '__main__':
    main()
