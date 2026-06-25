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
import os
import time

import rclpy
from rclpy.duration import Duration
try:
    from rclpy.event_handler import SubscriptionEventCallbacks
except ModuleNotFoundError:
    from rclpy.qos_event import SubscriptionEventCallbacks
from rclpy.executors import SingleThreadedExecutor
from rclpy.node import Node
from rclpy.qos import DurabilityPolicy, HistoryPolicy, QoSProfile, ReliabilityPolicy
from std_msgs.msg import String


class DeadlineSub(Node):
    def __init__(self, args):
        super().__init__('deadline_probe_sub', start_parameter_services=False, enable_rosout=False)
        self.args = args
        self.received = 0
        self.samples = []
        self.deadline_events = 0

        qos = QoSProfile(
            reliability=ReliabilityPolicy.RELIABLE,
            durability=DurabilityPolicy.VOLATILE,
            history=HistoryPolicy.KEEP_LAST,
            depth=args.depth,
            deadline=Duration(seconds=args.deadline_sec),
        )
        callbacks = SubscriptionEventCallbacks(deadline=self.on_deadline)
        self.sub = self.create_subscription(
            String, args.topic, self.on_msg, qos, event_callbacks=callbacks)

    def on_msg(self, msg):
        self.received += 1
        if len(self.samples) < 5:
            self.samples.append(msg.data)

    def on_deadline(self, event):
        self.deadline_events += 1

    def run(self):
        print(f'ROLE=sub TOPIC={self.args.topic}', flush=True)
        print(f"RMW_IMPLEMENTATION={os.environ.get('RMW_IMPLEMENTATION', '')}", flush=True)
        print(f"ROS_DOMAIN_ID={os.environ.get('ROS_DOMAIN_ID', '')}", flush=True)
        print(
            'QOS reliability=RELIABLE durability=VOLATILE history=KEEP_LAST '
            f'depth={self.args.depth} deadline_sec={self.args.deadline_sec}',
            flush=True,
        )

        end = time.time() + self.args.timeout
        while rclpy.ok() and time.time() < end:
            self.executor.spin_once(timeout_sec=0.1)

        data_ok = self.received >= self.args.expected
        event_ok = self.deadline_events >= self.args.expected_deadline_events
        result = 'PASS' if data_ok and event_ok else 'FAIL'

        print(f'EXPECTED={self.args.expected}', flush=True)
        print(f'RECEIVED={self.received}', flush=True)
        print(f'SAMPLES={self.samples}', flush=True)
        print(f'EXPECTED_DEADLINE_EVENTS={self.args.expected_deadline_events}', flush=True)
        print(f'SUB_DEADLINE_EVENTS={self.deadline_events}', flush=True)
        print(f'RESULT={result}', flush=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--topic', default='/deadline_probe')
    parser.add_argument('--deadline-sec', type=float, default=0.2)
    parser.add_argument('--depth', type=int, default=10)
    parser.add_argument('--timeout', type=float, default=5.0)
    parser.add_argument('--expected', type=int, default=1)
    parser.add_argument('--expected-deadline-events', type=int, default=1)
    args = parser.parse_args()

    rclpy.init()
    node = DeadlineSub(args)
    executor = SingleThreadedExecutor()
    executor.add_node(node)
    node.executor = executor
    try:
        node.run()
    finally:
        executor.remove_node(node)
        node.destroy_node()
        executor.shutdown(timeout_sec=1.0)
        rclpy.shutdown()


if __name__ == '__main__':
    main()
