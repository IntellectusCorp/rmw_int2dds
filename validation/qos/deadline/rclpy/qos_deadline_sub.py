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


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument('--topic', required=True)
    parser.add_argument('--deadline-sec', type=float, required=True)
    parser.add_argument('--timeout', type=float, default=8.0)
    parser.add_argument('--expect-at-least', type=int, default=0)
    parser.add_argument('--expect-none', action='store_true')
    parser.add_argument('--require-deadline-event', action='store_true')
    parser.add_argument('--require-incompatible-event', action='store_true')
    return parser.parse_args()


class DeadlineSubscriber(Node):
    def __init__(self, args: argparse.Namespace) -> None:
        super().__init__('qos_deadline_sub', start_parameter_services=False, enable_rosout=False)
        self.args = args
        self.done = False
        self.received_messages = []
        self.deadline_events = []
        self.incompatible_events = []

        qos = QoSProfile(
            history=HistoryPolicy.KEEP_LAST,
            depth=10,
            reliability=ReliabilityPolicy.RELIABLE,
            durability=DurabilityPolicy.VOLATILE,
            deadline=Duration(seconds=args.deadline_sec),
        )
        callbacks = SubscriptionEventCallbacks(
            deadline=self._deadline_callback if args.require_deadline_event else None,
            incompatible_qos=(
                self._incompatible_callback if args.require_incompatible_event else None),
        )
        self.subscription = self.create_subscription(
            String, args.topic, self._callback, qos, event_callbacks=callbacks
        )
        self.observation_timer = self.create_timer(0.5, self._observe)
        self.finish_timer = self.create_timer(max(args.timeout, 0.1), self._finish)
        self.get_logger().info(
            'subscriber ready: '
            f'topic={args.topic}, deadline={args.deadline_sec}s'
        )

    def _callback(self, msg: String) -> None:
        self.received_messages.append(msg.data)
        self.get_logger().info(f'subscriber heard: {msg.data}')

    def _deadline_callback(self, event) -> None:
        self.deadline_events.append((event.total_count, event.total_count_change))
        self.get_logger().info(
            'subscription deadline event: '
            f'total_count={event.total_count}, total_count_change={event.total_count_change}'
        )

    def _incompatible_callback(self, event) -> None:
        self.incompatible_events.append(
            (event.total_count, event.total_count_change, event.last_policy_kind)
        )
        self.get_logger().info(
            'subscription incompatible qos event: '
            f'total_count={event.total_count}, total_count_change={event.total_count_change}, '
            f'last_policy_kind={event.last_policy_kind}'
        )

    def _observe(self) -> None:
        self.get_logger().info(
            f'publisher_count observation: {self.count_publishers(self.args.topic)}'
        )

    def _finish(self) -> None:
        self.finish_timer.cancel()
        self.observation_timer.cancel()
        self.done = True

    def evaluate(self) -> bool:
        ok = True
        if self.args.expect_none:
            ok = ok and len(self.received_messages) == 0
        else:
            ok = ok and len(self.received_messages) >= self.args.expect_at_least
        if self.args.require_deadline_event:
            ok = ok and len(self.deadline_events) >= 1
        if self.args.require_incompatible_event:
            ok = ok and len(self.incompatible_events) >= 1

        if ok:
            if self.args.expect_none:
                self.get_logger().info('deadline subscriber ok: received nothing')
            else:
                self.get_logger().info(
                    f'deadline subscriber ok: received_count={len(self.received_messages)}'
                )
        else:
            self.get_logger().error(
                'deadline subscriber failed: '
                f'received={self.received_messages}, '
                f'deadline_events={self.deadline_events}, '
                f'incompatible_events={self.incompatible_events}'
            )
        return ok


def main() -> int:
    args = parse_args()
    if args.expect_none and args.expect_at_least > 0:
        raise RuntimeError('--expect-none and --expect-at-least cannot be used together')
    rclpy.init()
    node = DeadlineSubscriber(args)
    executor = SingleThreadedExecutor()
    executor.add_node(node)
    try:
        while rclpy.ok() and not node.done:
            executor.spin_once(timeout_sec=0.1)
        ok = node.evaluate()
    finally:
        executor.remove_node(node)
        node.destroy_node()
        executor.shutdown(timeout_sec=1.0)
        rclpy.shutdown()
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
