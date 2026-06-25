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
try:
    from rclpy.event_handler import SubscriptionEventCallbacks
except ModuleNotFoundError:
    from rclpy.qos_event import SubscriptionEventCallbacks
from rclpy.node import Node
from rclpy.qos import DurabilityPolicy, HistoryPolicy, QoSProfile, ReliabilityPolicy
from std_msgs.msg import String


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument('--topic', default='qos_durability_topic')
    parser.add_argument('--durability', choices=['volatile', 'transient_local'], required=True)
    parser.add_argument('--reliability', choices=['reliable', 'best_effort'], default='reliable')
    parser.add_argument('--timeout', type=float, default=3.0)
    parser.add_argument('--expect')
    parser.add_argument('--expect-none', action='store_true')
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


class DurabilitySubscriber(Node):
    def __init__(self, args: argparse.Namespace) -> None:
        super().__init__('qos_durability_sub')
        self.args = args
        self.received_messages: list[str] = []
        self.incompatible_events = 0

        qos = QoSProfile(
            history=HistoryPolicy.KEEP_LAST,
            depth=1,
            reliability=reliability_policy(args.reliability),
            durability=durability_policy(args.durability),
        )

        subscription_kwargs = {}
        if args.enable_incompatible_events:
            subscription_kwargs['event_callbacks'] = SubscriptionEventCallbacks(
                incompatible_qos=self._incompatible_qos
            )

        self.sub = self.create_subscription(
            String,
            args.topic,
            self._callback,
            qos,
            **subscription_kwargs,
        )

        self.get_logger().info(
            'subscriber ready: '
            f'topic={args.topic}, durability={args.durability}, reliability={args.reliability}'
        )

    def _callback(self, msg: String) -> None:
        self.received_messages.append(msg.data)
        self.get_logger().info(f'received: {msg.data}')

    def _incompatible_qos(self, event) -> None:
        self.incompatible_events += 1
        self.get_logger().info(
            'subscription incompatible qos event: '
            f'total_count={event.total_count}, total_count_change={event.total_count_change}, '
            f'last_policy_kind={event.last_policy_kind}'
        )


def main() -> int:
    args = parse_args()
    if bool(args.expect) == bool(args.expect_none):
        raise SystemExit('exactly one of --expect or --expect-none is required')

    rclpy.init()
    node = DurabilitySubscriber(args)
    ok = False

    try:
        end = time.time() + args.timeout
        while rclpy.ok() and time.time() < end:
            rclpy.spin_once(node, timeout_sec=0.1)

        if args.expect is not None:
            ok = args.expect in node.received_messages
            if ok:
                node.get_logger().info(f'durability subscriber ok: expected={args.expect}')
            else:
                node.get_logger().error(
                    'durability subscriber failed: '
                    f'received={node.received_messages}, '
                    f'publisher_count={node.count_publishers(args.topic)}, '
                    f'incompatible_events={node.incompatible_events}'
                )
        else:
            ok = node.received_messages == []
            if ok:
                node.get_logger().info('durability subscriber ok: received nothing')
            else:
                node.get_logger().error(
                    'durability subscriber failed: '
                    f'received={node.received_messages}, '
                    f'publisher_count={node.count_publishers(args.topic)}, '
                    f'incompatible_events={node.incompatible_events}'
                )
    finally:
        node.destroy_node()
        rclpy.try_shutdown()

    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
