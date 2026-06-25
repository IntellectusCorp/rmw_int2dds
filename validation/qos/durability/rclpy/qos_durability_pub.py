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
import sys
import time

import rclpy
from rclpy.executors import SingleThreadedExecutor
from rclpy.node import Node
try:
    from rclpy.event_handler import PublisherEventCallbacks
except ModuleNotFoundError:
    from rclpy.qos_event import PublisherEventCallbacks
from rclpy.qos import DurabilityPolicy, HistoryPolicy, QoSProfile, ReliabilityPolicy
from std_msgs.msg import String


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument('--topic', default='qos_durability_topic')
    parser.add_argument('--durability', choices=['volatile', 'transient_local'], required=True)
    parser.add_argument('--reliability', choices=['reliable', 'best_effort'], default='reliable')
    parser.add_argument('--seed', required=True)
    parser.add_argument('--seed-delay', type=float, default=0.0)
    parser.add_argument('--keep-alive', type=float, default=5.0)
    parser.add_argument('--repeat-count', type=int, default=1)
    parser.add_argument('--repeat-period', type=float, default=0.5)
    parser.add_argument('--wait-for-subscriber-count', type=int, default=0)
    parser.add_argument('--wait-timeout', type=float, default=5.0)
    parser.add_argument('--enable-incompatible-events', action='store_true')
    return parser.parse_args()


def durability_policy(name: str) -> DurabilityPolicy:
    return (
        DurabilityPolicy.TRANSIENT_LOCAL
        if name == 'transient_local'
        else DurabilityPolicy.VOLATILE
    )


def reliability_policy(name: str) -> ReliabilityPolicy:
    return (
        ReliabilityPolicy.RELIABLE
        if name == 'reliable'
        else ReliabilityPolicy.BEST_EFFORT
    )


class DurabilityPublisher(Node):
    def __init__(self, args: argparse.Namespace) -> None:
        super().__init__('qos_durability_pub', start_parameter_services=False, enable_rosout=False)
        self.args = args
        self.done = False
        self.incompatible_events = 0
        self.start_time = time.time()
        self.publish_count = 0

        qos = QoSProfile(
            history=HistoryPolicy.KEEP_LAST,
            depth=1,
            reliability=reliability_policy(args.reliability),
            durability=durability_policy(args.durability),
        )
        publisher_kwargs = {}
        if args.enable_incompatible_events:
            publisher_kwargs['event_callbacks'] = PublisherEventCallbacks(
                incompatible_qos=self._incompatible_qos
            )
        self.publisher = self.create_publisher(String, args.topic, qos, **publisher_kwargs)
        self.observation_timer = self.create_timer(0.5, self._observe)
        self.publish_timer = self.create_timer(max(args.seed_delay, 0.01), self._publish_once)
        self.shutdown_timer = self.create_timer(
            max(args.seed_delay + args.keep_alive, 0.1),
            self._finish,
        )
        self.get_logger().info(
            'publisher ready: '
            f'topic={args.topic}, durability={args.durability}, reliability={args.reliability}'
        )

    def _publish_once(self) -> None:
        if self.args.wait_for_subscriber_count > 0:
            current_count = self.count_subscribers(self.args.topic)
            if current_count < self.args.wait_for_subscriber_count:
                if time.time() - self.start_time > self.args.wait_timeout:
                    self.publish_timer.cancel()
                    self.observation_timer.cancel()
                    self.shutdown_timer.cancel()
                    self.done = True
                    self.get_logger().error(
                        'publisher wait-for-match failed: '
                        f'expected={self.args.wait_for_subscriber_count}, current={current_count}'
                    )
                return
        msg = String()
        msg.data = self.args.seed
        self.publisher.publish(msg)
        self.publish_count += 1
        self.get_logger().info(f'published seed #{self.publish_count}: {msg.data}')
        if self.publish_count >= self.args.repeat_count:
            self.publish_timer.cancel()
        else:
            self.publish_timer.timer_period_ns = int(self.args.repeat_period * 1e9)

    def _incompatible_qos(self, event) -> None:
        self.incompatible_events += 1
        self.get_logger().info(
            'publisher incompatible qos event: '
            f'total_count={event.total_count}, total_count_change={event.total_count_change}, '
            f'last_policy_kind={event.last_policy_kind}'
        )

    def _observe(self) -> None:
        self.get_logger().info(
            f'subscriber_count observation: {self.count_subscribers(self.args.topic)}'
        )

    def _finish(self) -> None:
        self.shutdown_timer.cancel()
        self.observation_timer.cancel()
        self.done = True
        self.get_logger().info('publisher keep-alive finished')


def main() -> int:
    args = parse_args()
    rclpy.init()
    node = DurabilityPublisher(args)
    executor = SingleThreadedExecutor()
    executor.add_node(node)

    try:
        while rclpy.ok() and not node.done:
            executor.spin_once(timeout_sec=0.1)
    finally:
        executor.remove_node(node)
        node.destroy_node()
        executor.shutdown(timeout_sec=1.0)
        rclpy.shutdown()

    return 0


if __name__ == '__main__':
    sys.exit(main())
