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
from rclpy.task import Future


class FutureDoneCallbackCheck(Node):
    def __init__(self) -> None:
        super().__init__(
            'future_done_callback_check',
            start_parameter_services=False,
            enable_rosout=False,
        )
        self.future = Future()
        self.done_callback_called = False
        self.result_value = None
        self._timer = self.create_timer(0.1, self._complete_future)

    def _complete_future(self) -> None:
        self._timer.cancel()
        self.future.set_result(42)

    def done_callback(self, future: Future) -> None:
        self.done_callback_called = True
        self.result_value = future.result()
        self.get_logger().info(f'done callback result: {self.result_value}')


def main() -> int:
    rclpy.init()
    node = FutureDoneCallbackCheck()
    node.future.add_done_callback(node.done_callback)

    try:
        while rclpy.ok() and not node.done_callback_called:
            rclpy.spin_once(node, timeout_sec=0.2)
        ok = node.done_callback_called and node.result_value == 42
        if ok:
            node.get_logger().info('future done callback ok')
        else:
            node.get_logger().error(
                f'future done callback failed: '
                f'called={node.done_callback_called}, '
                f'result={node.result_value}'
            )
    finally:
        node.destroy_node()
        rclpy.shutdown()
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
