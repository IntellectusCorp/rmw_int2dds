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

import rclpy
from rclpy.node import Node


class SpinOnceCallbackCheck(Node):
    def __init__(self) -> None:
        super().__init__(
            'spin_once_callback_check',
            start_parameter_services=False,
            enable_rosout=False,
        )
        self.count = 0
        self.guard = self.create_guard_condition(self._callback)

    def _callback(self) -> None:
        self.count += 1
        self.get_logger().info(f'guard callback #{self.count}')


def main() -> int:
    rclpy.init()
    node = SpinOnceCallbackCheck()
    try:
        for _ in range(3):
            node.guard.trigger()
            rclpy.spin_once(node, timeout_sec=0.5)
        ok = node.count == 3
        if ok:
            node.get_logger().info('spin_once callback ok')
        else:
            node.get_logger().error(f'spin_once callback failed: count={node.count}')
    finally:
        node.destroy_guard_condition(node.guard)
        node.destroy_node()
        rclpy.shutdown()
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
