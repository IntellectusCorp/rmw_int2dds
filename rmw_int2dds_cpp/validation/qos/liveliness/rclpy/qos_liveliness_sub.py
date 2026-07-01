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
from rclpy.qos import (DurabilityPolicy, HistoryPolicy, LivelinessPolicy, QoSProfile,
                       ReliabilityPolicy)
from std_msgs.msg import String


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument('--topic', required=True)
    parser.add_argument('--liveliness', default='automatic')
    parser.add_argument('--lease-sec', type=float, required=True)
    parser.add_argument('--timeout', type=float, default=6.0)
    parser.add_argument('--expect-at-least', type=int, default=1)
    parser.add_argument('--require-not-alive-event', action='store_true')
    return parser.parse_args()


def parse_liveliness(value: str) -> LivelinessPolicy:
    if value == 'manual_by_topic':
        return LivelinessPolicy.MANUAL_BY_TOPIC
    return LivelinessPolicy.AUTOMATIC


class LivelinessSubscriber(Node):
    def __init__(self, args: argparse.Namespace) -> None:
        super().__init__('qos_liveliness_sub', start_parameter_services=False, enable_rosout=False)
        self.args = args
        self.done = False
        self.received_messages = []
        self.liveliness_events = []

        qos = QoSProfile(
            history=HistoryPolicy.KEEP_LAST,
            depth=10,
            reliability=ReliabilityPolicy.RELIABLE,
            durability=DurabilityPolicy.VOLATILE,
            liveliness=parse_liveliness(args.liveliness),
            liveliness_lease_duration=Duration(seconds=args.lease_sec),
        )
        callbacks = SubscriptionEventCallbacks(liveliness=self._liveliness_callback)
        self.subscription = self.create_subscription(
            String, args.topic, self._callback, qos, event_callbacks=callbacks
        )
        self.observe_timer = self.create_timer(0.5, self._observe)
        self.finish_timer = self.create_timer(max(args.timeout, 0.1), self._finish)
        self.get_logger().info(
            'subscriber ready: '
            f'topic={args.topic}, liveliness={args.liveliness}, lease={args.lease_sec}s'
        )

    def _callback(self, msg: String) -> None:
        self.received_messages.append(msg.data)
        self.get_logger().info(f'subscriber heard: {msg.data}')

    def _liveliness_callback(self, event) -> None:
        info = (
            event.alive_count,
            event.not_alive_count,
            event.alive_count_change,
            event.not_alive_count_change,
        )
        self.liveliness_events.append(info)
        self.get_logger().info(
            'subscription liveliness event: '
            f'alive_count={event.alive_count}, '
            f'not_alive_count={event.not_alive_count}, '
            f'alive_count_change={event.alive_count_change}, '
            f'not_alive_count_change={event.not_alive_count_change}'
        )

    def _observe(self) -> None:
        self.get_logger().info(
            f'publisher_count observation: {self.count_publishers(self.args.topic)}'
        )

    def _finish(self) -> None:
        self.finish_timer.cancel()
        self.observe_timer.cancel()
        self.done = True

    def evaluate(self) -> bool:
        liveliness_loss_events = [
            evt for evt in self.liveliness_events
            if evt[2] < 0 or evt[3] > 0
        ]
        ok = len(self.received_messages) >= self.args.expect_at_least
        if self.args.require_not_alive_event:
            ok = ok and len(liveliness_loss_events) >= 1

        if ok:
            self.get_logger().info(
                'liveliness subscriber ok: '
                f'received_count={len(self.received_messages)}, '
                f'liveliness_loss_events={len(liveliness_loss_events)}'
            )
        else:
            self.get_logger().error(
                'liveliness subscriber failed: '
                f'received={self.received_messages}, '
                f'events={self.liveliness_events}'
            )
        return ok


def main() -> int:
    args = parse_args()
    rclpy.init()
    node = LivelinessSubscriber(args)
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
