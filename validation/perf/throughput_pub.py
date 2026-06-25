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
from array import array
import time

import rclpy
from rclpy.node import Node
from std_msgs.msg import ByteMultiArray


class ThroughputPublisher(Node):
    def __init__(self, topic: str, payload_bytes: int, duration_sec: float, rate_hz: float):
        super().__init__('throughput_pub')
        self.pub = self.create_publisher(ByteMultiArray, topic, 10)
        self.payload_bytes = payload_bytes
        self.duration_sec = duration_sec
        self.rate_hz = rate_hz

    def run(self):
        deadline = time.monotonic() + 5.0
        while self.pub.get_subscription_count() == 0 and time.monotonic() < deadline:
            rclpy.spin_once(self, timeout_sec=0.05)

        msg = ByteMultiArray()
        msg.data = array('B', [0]) * self.payload_bytes

        start = time.monotonic()
        end = start + self.duration_sec
        sent_msgs = 0
        sent_bytes = 0
        period = 1.0 / self.rate_hz if self.rate_hz > 0 else 0.0
        next_time = time.monotonic()

        while time.monotonic() < end:
            self.pub.publish(msg)
            sent_msgs += 1
            sent_bytes += self.payload_bytes
            if period > 0:
                next_time += period
                sleep_for = next_time - time.monotonic()
                if sleep_for > 0:
                    time.sleep(sleep_for)

        elapsed = max(time.monotonic() - start, 1e-9)
        print(
            'THROUGHPUT_PUB '
            f'payload_bytes={self.payload_bytes} '
            f'duration_s={elapsed:.3f} '
            f'sent_msgs={sent_msgs} '
            f'sent_mps={sent_msgs/elapsed:.3f} '
            f'sent_MBps={sent_bytes/elapsed/1_000_000.0:.3f}'
        )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--topic', default='/perf_throughput')
    parser.add_argument('--payload-bytes', type=int, default=4096)
    parser.add_argument('--duration-sec', type=float, default=5.0)
    parser.add_argument('--rate-hz', type=float, default=0.0, help='0 = max speed')
    args = parser.parse_args()

    rclpy.init()
    node = ThroughputPublisher(
        args.topic, args.payload_bytes, args.duration_sec, args.rate_hz
    )
    try:
        node.run()
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
