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
    from rclpy.event_handler import PublisherEventCallbacks
except ModuleNotFoundError:
    from rclpy.qos_event import PublisherEventCallbacks
from rclpy.executors import SingleThreadedExecutor
from rclpy.node import Node
from rclpy.qos import DurabilityPolicy, HistoryPolicy, QoSProfile, ReliabilityPolicy
from std_msgs.msg import String


class DeadlinePub(Node):
    def __init__(self, args):
        super().__init__('deadline_probe_pub', start_parameter_services=False, enable_rosout=False)
        self.args = args
        self.sent = 0
        self.deadline_events = 0
        self.done = False
        self.start_time = time.time()

        qos = QoSProfile(
            reliability=ReliabilityPolicy.RELIABLE,
            durability=DurabilityPolicy.VOLATILE,
            history=HistoryPolicy.KEEP_LAST,
            depth=args.depth,
            deadline=Duration(seconds=args.deadline_sec),
        )
        callbacks = PublisherEventCallbacks(deadline=self.on_deadline)
        self.pub = self.create_publisher(String, args.topic, qos, event_callbacks=callbacks)
        self.timer = self.create_timer(max(args.publish_period, 0.001), self.on_timer)

    def on_deadline(self, event):
        self.deadline_events += 1

    def on_timer(self):
        if self.args.wait_match and self.pub.get_subscription_count() < 1:
            if time.time() - self.start_time > self.args.match_timeout:
                self.done = True
            return

        if self.sent >= self.args.count:
            self.timer.cancel()
            return

        msg = String()
        msg.data = f'deadline:{self.sent}'
        self.pub.publish(msg)
        self.sent += 1

    def run(self):
        print(f'ROLE=pub TOPIC={self.args.topic}', flush=True)
        print(f"RMW_IMPLEMENTATION={os.environ.get('RMW_IMPLEMENTATION', '')}", flush=True)
        print(f"ROS_DOMAIN_ID={os.environ.get('ROS_DOMAIN_ID', '')}", flush=True)
        print(
            'QOS reliability=RELIABLE durability=VOLATILE history=KEEP_LAST '
            f'depth={self.args.depth} deadline_sec={self.args.deadline_sec}',
            flush=True,
        )

        end = time.time() + self.args.keep_alive
        while rclpy.ok() and time.time() < end and not self.done:
            self.executor.spin_once(timeout_sec=0.1)

        print(f'MATCHED_SUBS={self.pub.get_subscription_count()}', flush=True)
        print(f'SENT={self.sent}', flush=True)
        print(f'PUB_DEADLINE_EVENTS={self.deadline_events}', flush=True)
        print('PUB_DONE', flush=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--topic', default='/deadline_probe')
    parser.add_argument('--deadline-sec', type=float, default=0.2)
    parser.add_argument('--publish-period', type=float, default=0.5)
    parser.add_argument('--count', type=int, default=1)
    parser.add_argument('--depth', type=int, default=10)
    parser.add_argument('--keep-alive', type=float, default=5.0)
    parser.add_argument('--wait-match', action='store_true')
    parser.add_argument('--match-timeout', type=float, default=3.0)
    args = parser.parse_args()

    rclpy.init()
    node = DeadlinePub(args)
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
