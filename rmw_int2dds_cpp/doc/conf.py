# Copyright 2024 Int2DDS Project
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

# Sphinx configuration for rmw_int2dds_cpp.
# rosdoc2 wraps and augments this file; project/author/version are supplied
# from package.xml. See:
# https://docs.ros.org/en/jazzy/How-To-Guides/Documenting-a-ROS-2-Package.html
import os
import sys

sys.path.insert(0, os.path.abspath('.'))

project = 'rmw_int2dds_cpp'
author = 'Intellectus Corp.'
copyright = '2025-2026, Intellectus Corp.'  # noqa: A001

extensions = [
    'sphinx.ext.autodoc',
    'sphinx.ext.intersphinx',
    'myst_parser',   # allow including Markdown (e.g. README.md)
    'breathe',       # Doxygen XML integration for C++ API docs
    'exhale',        # auto-generate C++ API tree
]

source_suffix = {
    '.rst': 'restructuredtext',
    '.md': 'markdown',
}

html_theme = 'sphinx_rtd_theme'

# breathe / exhale are configured by rosdoc2 at build time; values here are
# sensible defaults for local `rosdoc2 build` runs.
breathe_default_project = 'rmw_int2dds_cpp'
