#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# Copyright 2020 Alibaba Group Holding Limited. All Rights Reserved.
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
#

from graphscope.framework.app import AppAssets
from graphscope.framework.app import not_compatible_for

__all__ = [
    "property_dump",
]


@not_compatible_for("dynamic_property", "arrow_projected", "dynamic_projected")
def property_dump(graph, out_prefix="/tmp/dump_graph", write_eid=False):
    """Compute single source shortest path on graph G.

    Args:
        graph (Graph): a property graph.
        src (int, optional): the source. Defaults to 0.

    Returns:
        :class:`graphscope.framework.context.LabeledVertexDataContext`:
            A context with each vertex assigned with the shortest distance from the src, evaluated in eager mode.
    """
    return AppAssets(algo="property_dump", context="labeled_vertex_data")(graph, out_prefix, write_eid)
