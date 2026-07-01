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

import sys
import time

import rclpy
from rclpy.callback_groups import MutuallyExclusiveCallbackGroup, ReentrantCallbackGroup
from rclpy.executors import MultiThreadedExecutor
from rclpy.node import Node


class CallbackGroupCheck(Node):
    def __init__(self) -> None:
        super().__init__(
            'callback_group_check',
            start_parameter_services=False,
            enable_rosout=False,
        )
        self.mutex_group = MutuallyExclusiveCallbackGroup()
        self.reentrant_group = ReentrantCallbackGroup()

        self.mutex_active = 0
        self.reentrant_active = 0
        self.mutex_overlap = False
        self.reentrant_overlap = False
        self.mutex_hits = 0
        self.reentrant_hits = 0

        self.validation_timers = [
            self.create_timer(0.05, self._mutex_callback, callback_group=self.mutex_group),
            self.create_timer(0.05, self._mutex_callback, callback_group=self.mutex_group),
            self.create_timer(0.05, self._reentrant_callback, callback_group=self.reentrant_group),
            self.create_timer(0.05, self._reentrant_callback, callback_group=self.reentrant_group),
        ]

    def _mutex_callback(self) -> None:
        self.mutex_active += 1
        if self.mutex_active > 1:
            self.mutex_overlap = True
        self.mutex_hits += 1
        time.sleep(0.1)
        self.mutex_active -= 1

    def _reentrant_callback(self) -> None:
        self.reentrant_active += 1
        if self.reentrant_active > 1:
            self.reentrant_overlap = True
        self.reentrant_hits += 1
        time.sleep(0.1)
        self.reentrant_active -= 1


def main() -> int:
    rclpy.init()
    node = CallbackGroupCheck()
    executor = MultiThreadedExecutor(num_threads=4)
    executor.add_node(node)

    end = time.time() + 2.0
    try:
        while rclpy.ok() and time.time() < end:
            executor.spin_once(timeout_sec=0.2)
    finally:
        ok = (
            node.mutex_hits >= 2 and
            node.reentrant_hits >= 2 and
            not node.mutex_overlap and
            node.reentrant_overlap
        )
        if ok:
            node.get_logger().info('callback group behavior ok')
        else:
            node.get_logger().error(
                'callback group behavior failed: '
                f'mutex_hits={node.mutex_hits}, mutex_overlap={node.mutex_overlap}, '
                f'reentrant_hits={node.reentrant_hits}, reentrant_overlap={node.reentrant_overlap}'
            )
        for timer in node.validation_timers:
            timer.cancel()
        executor.shutdown(timeout_sec=1.0)
        executor.remove_node(node)
        node.destroy_node()
        rclpy.shutdown()
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
