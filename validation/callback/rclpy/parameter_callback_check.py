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

from rcl_interfaces.msg import SetParametersResult

import rclpy
from rclpy.node import Node
from rclpy.parameter import Parameter


class ParameterCallbackCheck(Node):
    def __init__(self) -> None:
        super().__init__(
            'parameter_callback_check',
            start_parameter_services=False,
            enable_rosout=False,
        )
        self.declare_parameter('param1', 0.0)
        self.declare_parameter('param2', 0.0)

        self.events: list[tuple[str, str, float]] = []
        self.done = False
        self.success = False
        self.has_pre_post_callbacks = (
            hasattr(self, 'add_pre_set_parameters_callback') and
            hasattr(self, 'add_post_set_parameters_callback')
        )

        if self.has_pre_post_callbacks:
            self.add_pre_set_parameters_callback(self._pre_callback)
            self.add_post_set_parameters_callback(self._post_callback)
        self.add_on_set_parameters_callback(self._on_callback)

        self._timer = self.create_timer(0.1, self._run_once)

    def _pre_callback(self, parameters: list[Parameter]) -> list[Parameter]:
        updated = list(parameters)
        for param in parameters:
            self.events.append(('pre', param.name, float(param.value)))
            if param.name == 'param1':
                updated.append(Parameter('param2', Parameter.Type.DOUBLE, 4.0))
        return updated

    def _on_callback(self, parameters: list[Parameter]) -> SetParametersResult:
        result = SetParametersResult()
        result.successful = True
        result.reason = 'ok'
        for param in parameters:
            self.events.append(('on', param.name, float(param.value)))
        return result

    def _post_callback(self, parameters: list[Parameter]) -> None:
        for param in parameters:
            self.events.append(('post', param.name, float(param.value)))

    def _run_once(self) -> None:
        self._timer.cancel()
        result = self.set_parameters([Parameter('param1', Parameter.Type.DOUBLE, 3.0)])
        param1 = self.get_parameter('param1').value
        param2 = self.get_parameter('param2').value

        result_ok = len(result) == 1 and result[0].successful
        self.success = result_ok and (
            ('on', 'param1', 3.0) in self.events and
            param1 == 3.0
        )

        if self.has_pre_post_callbacks:
            self.success = self.success and (
                ('pre', 'param1', 3.0) in self.events and
                ('post', 'param1', 3.0) in self.events and
                ('on', 'param2', 4.0) in self.events and
                ('post', 'param2', 4.0) in self.events and
                param2 == 4.0
            )
        else:
            self.success = self.success and param2 == 0.0

        if self.success:
            self.get_logger().info('parameter callbacks ok')
        else:
            self.get_logger().error(f'parameter callbacks failed: events={self.events}')
        self.done = True


def main() -> int:
    rclpy.init()
    node = ParameterCallbackCheck()
    try:
        while rclpy.ok() and not node.done:
            rclpy.spin_once(node, timeout_sec=0.1)
    finally:
        ok = node.success
        node.destroy_node()
        rclpy.shutdown()
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
