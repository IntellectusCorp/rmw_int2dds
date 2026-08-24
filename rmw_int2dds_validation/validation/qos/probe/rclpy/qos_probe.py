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
from rclpy.node import Node
from rclpy.qos import (
    DurabilityPolicy,
    HistoryPolicy,
    LivelinessPolicy,
    QoSProfile,
    ReliabilityPolicy,
)
from rclpy.qos_event import PublisherEventCallbacks, SubscriptionEventCallbacks
from rclpy.qos_event import UnsupportedEventTypeError
from std_msgs.msg import String


CASES = {
    'q1': {
        'reliability': 'best_effort',
        'durability': 'volatile',
        'history': 'keep_last',
        'depth': 10,
    },
    'q2': {
        'reliability': 'reliable',
        'durability': 'volatile',
        'history': 'keep_last',
        'depth': 10,
    },
    'q3': {
        'reliability': 'reliable',
        'durability': 'volatile',
        'history': 'keep_last',
        'depth': 1,
    },
    'q4': {
        'reliability': 'reliable',
        'durability': 'transient_local',
        'history': 'keep_last',
        'depth': 10,
    },
}

RELIABILITY = {
    'system_default': ReliabilityPolicy.SYSTEM_DEFAULT,
    'reliable': ReliabilityPolicy.RELIABLE,
    'best_effort': ReliabilityPolicy.BEST_EFFORT,
}

DURABILITY = {
    'system_default': DurabilityPolicy.SYSTEM_DEFAULT,
    'volatile': DurabilityPolicy.VOLATILE,
    'transient_local': DurabilityPolicy.TRANSIENT_LOCAL,
}

HISTORY = {
    'system_default': HistoryPolicy.SYSTEM_DEFAULT,
    'keep_last': HistoryPolicy.KEEP_LAST,
    'keep_all': HistoryPolicy.KEEP_ALL,
}

LIVELINESS = {
    'system_default': LivelinessPolicy.SYSTEM_DEFAULT,
    'automatic': LivelinessPolicy.AUTOMATIC,
    'manual_by_topic': LivelinessPolicy.MANUAL_BY_TOPIC,
}


def duration_or_none(seconds):
    if seconds is None:
        return None
    return Duration(seconds=seconds)


def apply_case_defaults(args):
    if not args.case:
        return
    case = CASES[args.case]
    args.reliability = case['reliability']
    args.durability = case['durability']
    args.history = case['history']
    args.depth = case['depth']


def make_qos(args):
    kwargs = {
        'reliability': RELIABILITY[args.reliability],
        'durability': DURABILITY[args.durability],
        'history': HISTORY[args.history],
        'depth': args.depth,
        'liveliness': LIVELINESS[args.liveliness],
    }
    deadline = duration_or_none(args.deadline_sec)
    lifespan = duration_or_none(args.lifespan_sec)
    lease = duration_or_none(args.liveliness_lease_sec)
    if deadline is not None:
        kwargs['deadline'] = deadline
    if lifespan is not None:
        kwargs['lifespan'] = lifespan
    if lease is not None:
        kwargs['liveliness_lease_duration'] = lease
    return QoSProfile(**kwargs)


def label(args):
    return args.label if args.label else (args.case if args.case else 'custom')


def event_kinds(args):
    if not args.event_log:
        return set()
    kinds = set(args.event_kind or ['deadline'])
    if 'all' in kinds:
        kinds.remove('all')
        kinds.update(['deadline', 'liveliness', 'incompatible_qos'])
        if args.role == 'sub':
            kinds.add('message_lost')
    return kinds


def qos_text(args):
    parts = [
        f'reliability={args.reliability.upper()}',
        f'durability={args.durability.upper()}',
        f'history={args.history.upper()}',
        f'depth={args.depth}',
        f"deadline_sec={args.deadline_sec if args.deadline_sec is not None else 'default'}",
        f"lifespan_sec={args.lifespan_sec if args.lifespan_sec is not None else 'default'}",
        f'liveliness={args.liveliness.upper()}',
        'liveliness_lease_sec='
        f"{args.liveliness_lease_sec if args.liveliness_lease_sec is not None else 'default'}",
    ]
    return ' '.join(parts)


class EventLog:
    def __init__(self):
        self.publisher_deadline = 0
        self.publisher_liveliness = 0
        self.publisher_incompatible_qos = 0
        self.subscription_deadline = 0
        self.subscription_liveliness = 0
        self.subscription_incompatible_qos = 0
        self.subscription_message_lost = 0

    def pub_callbacks(self, kinds):
        return PublisherEventCallbacks(
            deadline=self.on_pub_deadline if 'deadline' in kinds else None,
            liveliness=self.on_pub_liveliness if 'liveliness' in kinds else None,
            incompatible_qos=self.on_pub_incompatible_qos if 'incompatible_qos' in kinds else None,
            use_default_callbacks=False,
        )

    def sub_callbacks(self, kinds):
        return SubscriptionEventCallbacks(
            deadline=self.on_sub_deadline if 'deadline' in kinds else None,
            liveliness=self.on_sub_liveliness if 'liveliness' in kinds else None,
            incompatible_qos=self.on_sub_incompatible_qos if 'incompatible_qos' in kinds else None,
            message_lost=self.on_sub_message_lost if 'message_lost' in kinds else None,
            use_default_callbacks=False,
        )

    def on_pub_deadline(self, event):
        self.publisher_deadline += 1
        if getattr(self, 'detail', False):
            print(f'EVENT publisher_deadline {event}', flush=True)

    def on_pub_liveliness(self, event):
        self.publisher_liveliness += 1
        if getattr(self, 'detail', False):
            print(f'EVENT publisher_liveliness {event}', flush=True)

    def on_pub_incompatible_qos(self, event):
        self.publisher_incompatible_qos += 1
        if getattr(self, 'detail', False):
            print(f'EVENT publisher_incompatible_qos {event}', flush=True)

    def on_sub_deadline(self, event):
        self.subscription_deadline += 1
        if getattr(self, 'detail', False):
            print(f'EVENT subscription_deadline {event}', flush=True)

    def on_sub_liveliness(self, event):
        self.subscription_liveliness += 1
        if getattr(self, 'detail', False):
            print(f'EVENT subscription_liveliness {event}', flush=True)

    def on_sub_incompatible_qos(self, event):
        self.subscription_incompatible_qos += 1
        if getattr(self, 'detail', False):
            print(f'EVENT subscription_incompatible_qos {event}', flush=True)

    def on_sub_message_lost(self, event):
        self.subscription_message_lost += 1
        if getattr(self, 'detail', False):
            print(f'EVENT subscription_message_lost {event}', flush=True)

    def print_summary(self):
        print(
            'EVENT_SUMMARY '
            f'pub_deadline={self.publisher_deadline} '
            f'pub_liveliness={self.publisher_liveliness} '
            f'pub_incompatible_qos={self.publisher_incompatible_qos} '
            f'sub_deadline={self.subscription_deadline} '
            f'sub_liveliness={self.subscription_liveliness} '
            f'sub_incompatible_qos={self.subscription_incompatible_qos} '
            f'sub_message_lost={self.subscription_message_lost}',
            flush=True,
        )


class PubNode(Node):
    def __init__(self, args):
        super().__init__('rmw_qos_probe_pub')
        self.args = args
        self.events = EventLog()
        self.events.detail = args.event_detail
        event_callbacks = self.events.pub_callbacks(event_kinds(args)) if args.event_log else None
        try:
            self.pub = self.create_publisher(
                String, args.topic, make_qos(args), event_callbacks=event_callbacks
            )
            if args.event_log:
                print('EVENT_SUPPORT publisher=supported', flush=True)
        except UnsupportedEventTypeError as exc:
            print(f'EVENT_SUPPORT publisher=unsupported error={exc}', flush=True)
            self.pub = self.create_publisher(String, args.topic, make_qos(args))

    def run(self):
        print(f'ROLE=pub LABEL={label(self.args)} TOPIC={self.args.topic}', flush=True)
        print(f"RMW_IMPLEMENTATION={os.environ.get('RMW_IMPLEMENTATION', '')}", flush=True)
        print(f"ROS_DOMAIN_ID={os.environ.get('ROS_DOMAIN_ID', '')}", flush=True)
        print(f'QOS {qos_text(self.args)}', flush=True)

        if self.args.wait_match:
            deadline = time.time() + self.args.match_timeout
            while time.time() < deadline and self.pub.get_subscription_count() < 1:
                rclpy.spin_once(self, timeout_sec=0.1)
            print(f'MATCHED_SUBS={self.pub.get_subscription_count()}', flush=True)

        interval = 1.0 / self.args.rate
        sent = 0
        for i in range(self.args.count):
            msg = String()
            msg.data = f'{label(self.args)}:{i}'
            self.pub.publish(msg)
            if self.args.assert_liveliness:
                self.pub.assert_liveliness()
            sent += 1
            rclpy.spin_once(self, timeout_sec=0.0)
            time.sleep(interval)

        print(f'SENT={sent}', flush=True)

        end = time.time() + self.args.post_sleep
        last_liveliness_assert = 0.0
        while time.time() < end:
            now = time.time()
            if (
                self.args.assert_liveliness
                and self.args.liveliness_assert_period > 0
                and now - last_liveliness_assert >= self.args.liveliness_assert_period
            ):
                self.pub.assert_liveliness()
                last_liveliness_assert = now
            rclpy.spin_once(self, timeout_sec=0.1)

        if self.args.event_log:
            self.events.print_summary()
        print('PUB_DONE', flush=True)


class SubNode(Node):
    def __init__(self, args):
        super().__init__('rmw_qos_probe_sub')
        self.args = args
        self.received = 0
        self.samples = []
        self.events = EventLog()
        self.events.detail = args.event_detail
        event_callbacks = self.events.sub_callbacks(event_kinds(args)) if args.event_log else None
        try:
            self.sub = self.create_subscription(
                String, args.topic, self.on_msg, make_qos(args), event_callbacks=event_callbacks
            )
            if args.event_log:
                print('EVENT_SUPPORT subscription=supported', flush=True)
        except UnsupportedEventTypeError as exc:
            print(f'EVENT_SUPPORT subscription=unsupported error={exc}', flush=True)
            self.sub = self.create_subscription(String, args.topic, self.on_msg, make_qos(args))

    def on_msg(self, msg):
        self.received += 1
        if len(self.samples) < 5:
            self.samples.append(msg.data)

    def run(self):
        print(f'ROLE=sub LABEL={label(self.args)} TOPIC={self.args.topic}', flush=True)
        print(f"RMW_IMPLEMENTATION={os.environ.get('RMW_IMPLEMENTATION', '')}", flush=True)
        print(f"ROS_DOMAIN_ID={os.environ.get('ROS_DOMAIN_ID', '')}", flush=True)
        print(f'QOS {qos_text(self.args)}', flush=True)

        deadline = time.time() + self.args.timeout
        while time.time() < deadline and self.received < self.args.expected:
            rclpy.spin_once(self, timeout_sec=0.1)

        print(f'EXPECTED={self.args.expected}', flush=True)
        print(f'RECEIVED={self.received}', flush=True)
        print(f'SAMPLES={self.samples}', flush=True)

        if self.received >= self.args.expected:
            print('RESULT=PASS', flush=True)
        else:
            print('RESULT=FAIL_TIMEOUT_OR_LOSS', flush=True)
        if self.args.event_log:
            self.events.print_summary()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--role', choices=['pub', 'sub'], required=True)
    parser.add_argument('--case', choices=sorted(CASES.keys()))
    parser.add_argument('--label')
    parser.add_argument('--topic', default='/qos_probe')
    parser.add_argument(
        '--reliability',
        choices=sorted(RELIABILITY.keys()),
        default='reliable',
    )
    parser.add_argument(
        '--durability',
        choices=sorted(DURABILITY.keys()),
        default='volatile',
    )
    parser.add_argument(
        '--history',
        choices=sorted(HISTORY.keys()),
        default='keep_last',
    )
    parser.add_argument('--depth', type=int, default=10)
    parser.add_argument('--deadline-sec', type=float)
    parser.add_argument('--lifespan-sec', type=float)
    parser.add_argument(
        '--liveliness',
        choices=sorted(LIVELINESS.keys()),
        default='automatic',
    )
    parser.add_argument('--liveliness-lease-sec', type=float)
    parser.add_argument('--count', type=int, default=20)
    parser.add_argument('--expected', type=int, default=20)
    parser.add_argument('--rate', type=float, default=20.0)
    parser.add_argument('--timeout', type=float, default=10.0)
    parser.add_argument('--match-timeout', type=float, default=5.0)
    parser.add_argument('--post-sleep', type=float, default=1.0)
    parser.add_argument('--wait-match', action='store_true')
    parser.add_argument('--event-log', action='store_true')
    parser.add_argument(
        '--event-kind',
        action='append',
        choices=[
            'deadline',
            'liveliness',
            'incompatible_qos',
            'message_lost',
            'all'],
        help='QoS event callback to register. Repeat for multiple. '
             'Defaults to deadline when --event-log is set.',
    )
    parser.add_argument('--event-detail', action='store_true')
    parser.add_argument('--assert-liveliness', action='store_true')
    parser.add_argument('--liveliness-assert-period', type=float, default=0.0)
    args = parser.parse_args()
    apply_case_defaults(args)

    rclpy.init()
    node = PubNode(args) if args.role == 'pub' else SubNode(args)
    try:
        node.run()
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
