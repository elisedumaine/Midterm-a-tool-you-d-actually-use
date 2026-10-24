"""Force Unreal to reload the World Builder source and rebuild its live panel.

Run this file through Tools > Execute Python Script after editing the tool.
"""

import importlib

import interactive_world_builder


world_builder = importlib.reload(interactive_world_builder)
widget = world_builder.reload_tool()
# This file is intentionally an explicit rebuild command: it bypasses the
# unreliable live UMG click callback and replaces the stale generated actors.
world_builder.generate(widget)
