"""A river that follows the valleys of the World Builder relief, made by Houdini.

Unreal finds the lowest route across the sculpted ground (from the far
mountains, through the forest, out the other side), carves a bed into the
ground mesh, then finds a few springs on the hills beside it whose small
streams run down into the river (narrower, with little falls, their own bed).
Every route goes to the iwb_river HDA in the open Houdini
(through the local bridge, Documents/houdini22.0/python3.13libs).  Houdini
returns a faceted water ribbon with cascades on the steep steps, foam and
boulders at their feet; Unreal writes it into one ordinary static mesh, so
Play mode does not need Houdini.

The Houdini Engine plugin's "bake" step crashed the editor (UObjectArray
assertion in BakeAllOutputs), so the plugin is not used here.

    import iwb_river; iwb_river.build_river()
"""
import heapq
import json
import math
import os
import random

import unreal

import interactive_world_builder as w

HDA_PATH = os.path.join(unreal.Paths.project_dir(), "Houdini", "iwb_river.hda")
HDA_ASSET = "/Game/WorldBuilder/Houdini/iwb_river"
BAKE_FOLDER = "/Game/WorldBuilder/Houdini/Baked"
RIVER_MESH = BAKE_FOLDER + "/SM_IWB_River"
BRIDGE_PORT = 18820
BRIDGE_TOKEN = os.path.join(os.path.expanduser("~"), "Documents", "houdini22.0", "iwb_bridge_token.txt")
MATERIALS = "/Game/WorldBuilder/Materials"
RIVER_TAG = "IWB_River"
SPACING = 350.0          # cm between two river points
SURFACE_BELOW_GROUND = 35.0
BED_DEPTH = 80.0
STEP_HEIGHT = 170.0      # height of one cascade step on steep ground
STREAMS = 3              # small streams that run down the hills into the river
STREAM_SPACING = 220.0
STREAM_STEP = 70.0       # their little falls
STREAM_BED = 35.0
WATER_HALF_WIDTH = 0.42  # the water ribbon is 0.85 x width wide (see _cook_in_houdini)
BANK_SLOPE = 0.2         # rise of the bed and banks per cm away from the water's edge
STREAM_SLOPE = 0.12
CROSSING = 0.75          # x grid cell: no ground point is farther than this from a water line


# ---------------------------------------------------------------- route
def _river_route(relief, data):
    """Cheapest path over the height grid: low ground is cheap, climbing is dear."""
    terrain = float(data["terrain"])
    half = terrain * 0.5
    n, step, extent, heights = relief.cells, relief.cell, relief.half_extent, relief.heights
    rng = random.Random(int(data["seed"]) + 5151)

    def cell(x, y):
        return (min(n, max(0, int(round((x + extent) / step)))), min(n, max(0, int(round((y + extent) / step)))))

    def position(i, j):
        return -extent + i * step, -extent + j * step

    angle = rng.uniform(0.0, math.tau)
    exit_angle = angle + math.pi + rng.uniform(-0.6, 0.6)
    source = cell(math.cos(angle) * half * 1.75, math.sin(angle) * half * 1.75)
    sink = cell(math.cos(exit_angle) * half * 1.3, math.sin(exit_angle) * half * 1.3)
    low = min(min(row) for row in heights)
    scale = terrain * 0.05
    start_x, start_y = -terrain * 0.36, 0.0
    best = {source: 0.0}
    came = {}
    queue = [(0.0, source)]
    moves = [(1, 0, 1.0), (-1, 0, 1.0), (0, 1, 1.0), (0, -1, 1.0), (1, 1, 1.414), (1, -1, 1.414), (-1, 1, 1.414), (-1, -1, 1.414)]
    while queue:
        cost, here = heapq.heappop(queue)
        if here == sink:
            break
        if cost > best.get(here, 1e30):
            continue
        hi, hj = here
        h_here = heights[hj][hi]
        for di, dj, length in moves:
            i, j = hi + di, hj + dj
            if not (0 <= i <= n and 0 <= j <= n):
                continue
            x, y = position(i, j)
            if math.hypot(x, y) > half * 1.85:
                continue
            h = heights[j][i]
            e = (h - low) / scale
            step_cost = length * step * (1.0 + 4.0 * e * e) + max(0.0, h - h_here) * 30.0
            if math.hypot(x - start_x, y - start_y) < terrain * 0.07:
                step_cost *= 8.0       # keep the player's start dry
            total = cost + step_cost
            if total < best.get((i, j), 1e30):
                best[(i, j)] = total
                came[(i, j)] = here
                heapq.heappush(queue, (total, (i, j)))
    path, node = [sink], sink
    while node in came:
        node = came[node]
        path.append(node)
    path.reverse()
    points = [position(i, j) for i, j in path]
    for _ in range(3):   # soften the grid's stair steps
        points = [points[0]] + [((a[0] + b[0] * 2 + c[0]) / 4.0, (a[1] + b[1] * 2 + c[1]) / 4.0)
                                for a, b, c in zip(points, points[1:], points[2:])] + [points[-1]]
    return w._catmull_rom(points[::2] + [points[-1]], SPACING)


def _river_profile(relief, route, terrain):
    """Water height, width and cascades along the route; the water never climbs."""
    samples = []
    level = None
    total = len(route)
    for index, (x, y) in enumerate(route):
        ground = relief.height(x, y)
        target = ground - SURFACE_BELOW_GROUND
        if level is None:
            level = target
        steep = (level - target) / SPACING > 0.12
        level = min(level, target)
        if steep:
            # Steep ground becomes a staircase of pools and falls.
            level = math.floor(level / STEP_HEIGHT) * STEP_HEIGHT
        t = index / max(1, total - 1)
        width = (520.0 + 480.0 * t) * (terrain / 20000.0) ** 0.5
        samples.append((x, y, level, width))
    return samples


def _sharpen_falls(samples, limit):
    """Hold the upper level until just before each drop, so a fall is a short,
    nearly vertical curtain instead of a long sloping ramp."""
    sharp = [samples[0]] if samples else []
    for a, b in zip(samples, samples[1:]):
        if a[2] - b[2] > limit:
            t = 0.85
            sharp.append((a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2], a[3] + (b[3] - a[3]) * t))
        sharp.append(b)
    return sharp


def _bank(sample, d, step, slope):
    """Height of a V-shaped bed that crosses the water level at the water's edge.

    The ground grid is coarser than a narrow river, so the crossing never sits
    closer than about 3/4 of a grid cell: there is always a ground point below
    the water near its middle, and the edges stay within a few cm of the water.
    """
    _x, _y, level, width = sample
    return level + slope * (d - max(width * WATER_HALF_WIDTH, step * CROSSING))


def _carve(relief, samples, bed_depth=BED_DEPTH, slope=BANK_SLOPE, banks=True):
    """Shape a bed under the water and banks that rise right at its edges.

    The ground grid is coarser than the river (one point every ~5.7 m), and
    the mesh interpolates linearly between points.  A flat bed dug below the
    water left the ribbon hovering over a wide trench; a V that crosses the
    water level at the water's edge stays right under linear interpolation.
    banks=False only lowers the ground (small streams must not bury the river).
    """
    n, step, extent, heights = relief.cells, relief.cell, relief.half_extent, relief.heights
    bucket = {}
    for s in samples:
        bucket.setdefault((int(s[0] // 1500), int(s[1] // 1500)), []).append(s)
    changed = 0
    for j in range(n + 1):
        y = -extent + j * step
        for i in range(n + 1):
            x = -extent + i * step
            bx, by = int(x // 1500), int(y // 1500)
            near, near_d, wall = None, 1e30, None
            for dx in (-1, 0, 1):
                for dy in (-1, 0, 1):
                    for s in bucket.get((bx + dx, by + dy), ()):
                        d = math.hypot(x - s[0], y - s[1])
                        if d < near_d:
                            near, near_d = s, d
                        # Beside a cascade, the bank must stand above the
                        # upper pool too, or the fall hangs in the air.
                        if d < max(s[3] * WATER_HALF_WIDTH, step * CROSSING) + step * 0.6:
                            wall = max(wall if wall is not None else -1e30, _bank(s, d, step, slope))
            if not near:
                continue
            half = max(near[3] * WATER_HALF_WIDTH, step * CROSSING)
            if near_d >= near[3] * 1.7 + step:
                continue
            h = heights[j][i]
            if near_d <= half:
                target = max(_bank(near, near_d, step, slope), near[2] - bed_depth)
                if not banks:
                    target = min(h, target)
            elif banks and wall is not None:
                target = wall
            else:
                target = min(h, _bank(near, near_d, step, slope))
            if abs(target - h) > 0.5:
                heights[j][i] = target
                changed += 1
    return changed


def _height_on(relief, heights, x, y):
    """relief.height on another copy of the height grid (same facets)."""
    n = relief.cells
    gx = min(max((x + relief.half_extent) / relief.cell, 0.0), n - 1e-6)
    gy = min(max((y + relief.half_extent) / relief.cell, 0.0), n - 1e-6)
    i, j = int(gx), int(gy)
    fx, fy = gx - i, gy - j
    row, next_row = heights[j], heights[j + 1]
    if fx + fy <= 1.0:
        h00 = row[i]
        return h00 + fx * (row[i + 1] - h00) + fy * (next_row[i] - h00)
    h11 = next_row[i + 1]
    return h11 + (1.0 - fx) * (next_row[i] - h11) + (1.0 - fy) * (row[i + 1] - h11)


def _reground(relief, before, origin_x=0.0):
    """Move plants, rocks, paths and lanterns with the ground the river reshaped."""
    moved = 0
    layers = w.PLANT_LAYERS + (w.PATH_TAG,)
    for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
        if not w._has_tag(actor, w.GENERATED_TAG) or not any(w._has_tag(actor, layer) for layer in layers):
            continue
        components = actor.get_components_by_class(unreal.InstancedStaticMeshComponent)
        if components:
            for component in components:
                for index in range(component.get_instance_count()):
                    transform = component.get_instance_transform(index, True)
                    spot = transform.translation
                    x, y = spot.x - origin_x, spot.y
                    shift = relief.height(x, y) - _height_on(relief, before, x, y)
                    if abs(shift) > 1.0:
                        transform.translation = unreal.Vector(spot.x, spot.y, spot.z + shift)
                        component.update_instance_transform(index, transform, True, False, True)
                        moved += 1
        else:
            spot = actor.get_actor_location()
            x, y = spot.x - origin_x, spot.y
            shift = relief.height(x, y) - _height_on(relief, before, x, y)
            if abs(shift) > 1.0:
                actor.set_actor_location(unreal.Vector(spot.x, spot.y, spot.z + shift), False, False)
                moved += 1
    return moved


# ---------------------------------------------------------------- bridges
def _crossings(trails, waters):
    """Where a trail meets water: (dry point before, dry point after, water sample)."""
    found = []
    for trail in trails:
        points = trail["points"]
        for samples in waters:
            wet = []
            for index, (x, y) in enumerate(points):
                near = min(samples, key=lambda s: (x - s[0]) ** 2 + (y - s[1]) ** 2)
                if math.hypot(x - near[0], y - near[1]) < near[3] * 0.5 + 90.0:
                    wet.append((index, near))
            run = []
            for item in wet + [(None, None)]:
                if run and (item[0] is None or item[0] != run[-1][0] + 1):
                    first, last = run[0][0], run[-1][0]
                    if first > 0 and last < len(points) - 1:
                        found.append((points[first - 1], points[last + 1], run[len(run) // 2][1], trail["width"]))
                    run = []
                if item[0] is not None:
                    run.append(item)
    return found


def _bridge(batches, relief, start, end, water, path_width, palette):
    """A small low-poly plank bridge, gently arched, solid to walk on."""
    cube = unreal.load_asset("/Engine/BasicShapes/Cube")
    (ax, ay), (bx, by) = start, end
    heading = math.atan2(by - ay, bx - ax)
    ax, ay = ax - math.cos(heading) * 60.0, ay - math.sin(heading) * 60.0
    bx, by = bx + math.cos(heading) * 60.0, by + math.sin(heading) * 60.0
    length = math.hypot(bx - ax, by - ay)
    za, zb = relief.height(ax, ay), relief.height(bx, by)
    big = water[3] > 300.0
    deck_width = 170.0 if big else 120.0
    arch = max(25.0, water[2] + 45.0 - (za + zb) * 0.5) if big else 15.0
    paint = w._paint(palette, "trunk")
    across = (-math.sin(heading), math.cos(heading))

    def at(t, side=0.0, lift=0.0):
        z = za + (zb - za) * t + arch * math.sin(math.pi * t) + lift
        return unreal.Vector(ax + (bx - ax) * t + across[0] * side, ay + (by - ay) * t + across[1] * side, z)

    planks = max(4, int(length / 32.0))
    rng = random.Random(int(ax * 7 + ay * 13))
    for k in range(planks):
        t = (k + 0.5) / planks
        slope = math.degrees(math.atan2((zb - za) / length + arch * math.pi * math.cos(math.pi * t) / length, 1.0))
        batches.add(cube, unreal.Transform(
            location=at(t, rng.uniform(-4.0, 4.0), 4.0),
            rotation=w._rot(math.degrees(heading) + rng.uniform(-2.5, 2.5), slope),
            scale=unreal.Vector(28.0 / 100.0, deck_width * rng.uniform(0.92, 1.0) / 100.0, 0.07),
        ), RIVER_TAG, "IWB_Bridge", paint, solid=True)
    # Two beams under the planks, and, on the river, posts and hand rails.
    for side in (-0.35, 0.35):
        for k in range(6):
            t0, t1 = k / 6.0, (k + 1) / 6.0
            a, b = at(t0, side * deck_width, -6.0), at(t1, side * deck_width, -6.0)
            middle = unreal.Vector((a.x + b.x) / 2, (a.y + b.y) / 2, (a.z + b.z) / 2)
            batches.add(cube, unreal.Transform(
                location=middle, rotation=w._rot(math.degrees(heading), math.degrees(math.atan2(b.z - a.z, length / 6.0))),
                scale=unreal.Vector(length / 6.0 / 100.0 + 0.02, 0.12, 0.12),
            ), RIVER_TAG, "IWB_Bridge", paint, solid=False)
    if big:
        for side in (-0.5, 0.5):
            for k in range(5):
                t0, t1 = k / 5.0, (k + 1) / 5.0
                a, b = at(t0, side * deck_width, 85.0), at(t1, side * deck_width, 85.0)
                middle = unreal.Vector((a.x + b.x) / 2, (a.y + b.y) / 2, (a.z + b.z) / 2)
                batches.add(cube, unreal.Transform(
                    location=middle, rotation=w._rot(math.degrees(heading), math.degrees(math.atan2(b.z - a.z, length / 5.0))),
                    scale=unreal.Vector(length / 5.0 / 100.0 + 0.02, 0.07, 0.07),
                ), RIVER_TAG, "IWB_Bridge", paint, solid=True)
            for t in (0.0, 0.5, 1.0):
                foot = at(t, side * deck_width, 0.0)
                batches.add(cube, unreal.Transform(
                    location=unreal.Vector(foot.x, foot.y, foot.z + 40.0),
                    rotation=w._rot(math.degrees(heading)),
                    scale=unreal.Vector(0.1, 0.1, 0.95),
                ), RIVER_TAG, "IWB_Bridge", paint, solid=True)


def _build_bridges(relief, data, waters, palette):
    trails = w._trail_network(data["seed"], float(data["terrain"]), relief, data.get("paths", "Network"))
    crossings = _crossings(trails, waters)
    batches = w._Batches(w.GENERATED_TAG)
    for start, end, water, path_width in crossings:
        _bridge(batches, relief, start, end, water, path_width, palette)
    batches.flush()
    for actor in _actors():
        if actor.get_actor_label().startswith("IWB_Bridge"):
            tags = list(actor.get_editor_property("tags"))
            if unreal.Name(RIVER_TAG) not in tags:
                actor.set_editor_property("tags", tags + [unreal.Name(RIVER_TAG)])
    return len(crossings)


# ---------------------------------------------------------------- streams
def _grid_path(relief, start, goal, margin):
    """Cheapest grid path from start to goal (x, y), staying low, inside a box."""
    n, step, extent, heights = relief.cells, relief.cell, relief.half_extent, relief.heights

    def cell(x, y):
        return (min(n, max(0, int(round((x + extent) / step)))), min(n, max(0, int(round((y + extent) / step)))))

    a, b = cell(*start), cell(*goal)
    lo_i, hi_i = min(a[0], b[0]) - margin, max(a[0], b[0]) + margin
    lo_j, hi_j = min(a[1], b[1]) - margin, max(a[1], b[1]) + margin
    best, came, queue = {a: 0.0}, {}, [(0.0, a)]
    while queue:
        cost, here = heapq.heappop(queue)
        if here == b:
            break
        if cost > best.get(here, 1e30):
            continue
        h_here = heights[here[1]][here[0]]
        for di, dj, length in ((1, 0, 1.0), (-1, 0, 1.0), (0, 1, 1.0), (0, -1, 1.0),
                               (1, 1, 1.414), (1, -1, 1.414), (-1, 1, 1.414), (-1, -1, 1.414)):
            i, j = here[0] + di, here[1] + dj
            if not (max(0, lo_i) <= i <= min(n, hi_i) and max(0, lo_j) <= j <= min(n, hi_j)):
                continue
            h = heights[j][i]
            # Water runs downhill: going down is cheap, climbing is very dear.
            total = cost + length * step + max(0.0, h - h_here) * 40.0 + max(0.0, h_here - h) * 0.5
            if total < best.get((i, j), 1e30):
                best[(i, j)] = total
                came[(i, j)] = here
                heapq.heappush(queue, (total, (i, j)))
    path, node = [b], b
    while node in came:
        node = came[node]
        path.append(node)
    path.reverse()
    points = [(-extent + i * step, -extent + j * step) for i, j in path]
    points[0], points[-1] = start, goal
    for _ in range(2):
        points = [points[0]] + [((p[0] + q[0] * 2 + r[0]) / 4.0, (p[1] + q[1] * 2 + r[1]) / 4.0)
                                for p, q, r in zip(points, points[1:], points[2:])] + [points[-1]]
    return points


def _stream_routes(relief, samples, data):
    """Springs on the hills beside the river, each running down to join it."""
    terrain = float(data["terrain"])
    half = terrain * 0.5
    rng = random.Random(int(data["seed"]) + 8080)
    start_x, start_y = -terrain * 0.36, 0.0
    inside = [index for index, s in enumerate(samples) if math.hypot(s[0], s[1]) < half * 0.8]
    if len(inside) < 6:
        return []
    streams, used = [], []
    picks = [inside[int(len(inside) * (k + 0.5) / STREAMS)] for k in range(STREAMS)]
    for number, index in enumerate(picks):
        jx, jy, level, width = samples[index]
        ax, ay, _, _ = samples[max(0, index - 1)]
        bx, by, _, _ = samples[min(len(samples) - 1, index + 1)]
        length = math.hypot(bx - ax, by - ay) or 1.0
        side = 1.0 if number % 2 == 0 else -1.0
        nx, ny = -(by - ay) / length * side, (bx - ax) / length * side
        best = None
        for attempt in range(120):
            # Springs sit up the valley side, leaning upstream like real side streams.
            # Try the other bank too if this one stays flat.
            if attempt == 60:
                nx, ny = -nx, -ny
            reach = rng.uniform(2200.0, 5200.0)
            turn = rng.uniform(-0.8, 0.3)
            dx = nx * math.cos(turn) - ny * math.sin(turn)
            dy = nx * math.sin(turn) + ny * math.cos(turn)
            sx, sy = jx + dx * reach, jy + dy * reach
            if math.hypot(sx, sy) > half * 0.88 or math.hypot(sx - start_x, sy - start_y) < terrain * 0.08:
                continue
            if any(math.hypot(sx - s[0], sy - s[1]) < s[3] * 2.0 for s in samples):
                continue
            if any(math.hypot(sx - ux, sy - uy) < 2500.0 for ux, uy in used):
                continue
            rise = relief.height(sx, sy) - level
            if rise > 100.0 and (best is None or rise > best[0]):
                best = (rise, sx, sy)
        if not best:
            continue
        _rise, sx, sy = best
        used.append((sx, sy))
        # End inside the river, so the stream visibly pours into it.
        inner = (jx - nx * width * 0.25, jy - ny * width * 0.25)
        route = w._catmull_rom(_grid_path(relief, (sx, sy), inner, 8), STREAM_SPACING)
        streams.append((route, level))
    return streams


def _stream_profile(relief, route, river_level):
    samples, level = [], None
    total = len(route)
    for index, (x, y) in enumerate(route):
        target = relief.height(x, y) - 15.0
        if level is None:
            level = target
        steep = (level - target) / STREAM_SPACING > 0.15
        level = max(river_level, min(level, target))
        if steep and level > river_level + STREAM_STEP:
            level = max(river_level, math.floor(level / STREAM_STEP) * STREAM_STEP)
        t = index / max(1, total - 1)
        samples.append((x, y, level, 120.0 + 110.0 * t))
    return samples


# ---------------------------------------------------------------- materials
MATERIAL_VERSION = "river-3"


def _material(name, build):
    """Create a river material, or rebuild it in place when its recipe changed."""
    path = "{}/{}".format(MATERIALS, name)
    material = unreal.load_asset(path)
    if material and unreal.EditorAssetLibrary.get_metadata_tag(material, "IWBVersion") == MATERIAL_VERSION:
        return material
    if not material:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, MATERIALS, unreal.Material, unreal.MaterialFactoryNew())
    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
    build(material, unreal.MaterialEditingLibrary)
    unreal.EditorAssetLibrary.set_metadata_tag(material, "IWBVersion", MATERIAL_VERSION)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material, False)
    return material


def _node(lib, material, cls, x, y, **props):
    item = lib.create_material_expression(material, cls, x, y)
    for key, value in props.items():
        item.set_editor_property(key, value)
    return item


def _water(material, lib):
    colour = _node(lib, material, unreal.MaterialExpressionVectorParameter, -500, 0, parameter_name="Color",
                   default_value=unreal.LinearColor(0.035, 0.20, 0.24, 1.0))
    lib.connect_material_property(colour, "", unreal.MaterialProperty.MP_BASE_COLOR)
    glow = _node(lib, material, unreal.MaterialExpressionMultiply, -300, 120, const_b=0.12)
    lib.connect_material_expressions(colour, "", glow, "A")
    lib.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    lib.connect_material_property(_node(lib, material, unreal.MaterialExpressionConstant, -300, 240, r=0.04), "", unreal.MaterialProperty.MP_ROUGHNESS)
    lib.connect_material_property(_node(lib, material, unreal.MaterialExpressionConstant, -300, 320, r=1.0), "", unreal.MaterialProperty.MP_SPECULAR)


def _cascade(material, lib):
    """A see-through curtain of water: pale blue, with thin bright threads
    that run down the fall, brighter where the water breaks."""
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("two_sided", True)
    material.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE)
    world = _node(lib, material, unreal.MaterialExpressionWorldPosition, -1100, 100)
    across_xy = _node(lib, material, unreal.MaterialExpressionComponentMask, -950, 40, r=True, g=True, b=False, a=False)
    lib.connect_material_expressions(world, "", across_xy, "")
    across = _node(lib, material, unreal.MaterialExpressionDotProduct, -820, 40)
    lib.connect_material_expressions(across_xy, "", across, "A")
    lib.connect_material_expressions(_node(lib, material, unreal.MaterialExpressionConstant2Vector, -950, -40, r=0.031, g=0.023), "", across, "B")
    threads = _node(lib, material, unreal.MaterialExpressionSine, -700, 40)
    lib.connect_material_expressions(across, "", threads, "")
    thin = _node(lib, material, unreal.MaterialExpressionAbs, -600, 40)
    lib.connect_material_expressions(threads, "", thin, "")
    sharp = _node(lib, material, unreal.MaterialExpressionPower, -500, 40, const_exponent=12.0)
    lib.connect_material_expressions(thin, "", sharp, "Base")
    height = _node(lib, material, unreal.MaterialExpressionComponentMask, -950, 180, r=False, g=False, b=True, a=False)
    lib.connect_material_expressions(world, "", height, "")
    scaled = _node(lib, material, unreal.MaterialExpressionDivide, -820, 180, const_b=70.0)
    lib.connect_material_expressions(height, "", scaled, "A")
    time = _node(lib, material, unreal.MaterialExpressionTime, -950, 280)
    fast = _node(lib, material, unreal.MaterialExpressionMultiply, -820, 280, const_b=1.8)
    lib.connect_material_expressions(time, "", fast, "A")
    falling = _node(lib, material, unreal.MaterialExpressionAdd, -700, 220)
    lib.connect_material_expressions(scaled, "", falling, "A")
    lib.connect_material_expressions(fast, "", falling, "B")
    wave = _node(lib, material, unreal.MaterialExpressionFrac, -600, 220)
    lib.connect_material_expressions(falling, "", wave, "")
    pulse = _node(lib, material, unreal.MaterialExpressionMultiply, -380, 120)
    lib.connect_material_expressions(sharp, "", pulse, "A")
    lib.connect_material_expressions(wave, "", pulse, "B")
    water = _node(lib, material, unreal.MaterialExpressionConstant3Vector, -380, -80, constant=unreal.LinearColor(0.30, 0.62, 0.72, 1.0))
    white = _node(lib, material, unreal.MaterialExpressionConstant3Vector, -380, -160, constant=unreal.LinearColor(0.92, 0.97, 1.0, 1.0))
    colour = _node(lib, material, unreal.MaterialExpressionLinearInterpolate, -220, -60)
    lib.connect_material_expressions(water, "", colour, "A")
    lib.connect_material_expressions(white, "", colour, "B")
    lib.connect_material_expressions(pulse, "", colour, "Alpha")
    lib.connect_material_property(colour, "", unreal.MaterialProperty.MP_BASE_COLOR)
    glow = _node(lib, material, unreal.MaterialExpressionMultiply, -100, 60, const_b=0.35)
    lib.connect_material_expressions(pulse, "", glow, "A")
    lib.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    opacity = _node(lib, material, unreal.MaterialExpressionLinearInterpolate, -100, 200, const_a=0.45, const_b=0.9)
    lib.connect_material_expressions(pulse, "", opacity, "Alpha")
    lib.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)
    lib.connect_material_property(_node(lib, material, unreal.MaterialExpressionConstant, -100, 320, r=0.1), "", unreal.MaterialProperty.MP_ROUGHNESS)


def _foam(material, lib):
    """Soft white foam you can half see through."""
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE)
    base = _node(lib, material, unreal.MaterialExpressionConstant3Vector, -400, 0, constant=unreal.LinearColor(0.93, 0.96, 0.98, 1.0))
    lib.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    glow = _node(lib, material, unreal.MaterialExpressionMultiply, -250, 120, const_b=0.12)
    lib.connect_material_expressions(base, "", glow, "A")
    lib.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    lib.connect_material_property(_node(lib, material, unreal.MaterialExpressionConstant, -250, 220, r=0.55), "", unreal.MaterialProperty.MP_OPACITY)


def _river_rock(palette):
    """River boulders share the prop material, so they take snow and rain too."""
    path = "{}/M_IWB_RiverRock".format(MATERIALS)
    rock = unreal.load_asset(path)
    if not rock:
        factory = unreal.MaterialInstanceConstantFactoryNew()
        rock = unreal.AssetToolsHelpers.get_asset_tools().create_asset("M_IWB_RiverRock", MATERIALS, unreal.MaterialInstanceConstant, factory)
        rock.set_editor_property("parent", w._get_tint_material())
    mountain = palette["mountain"]
    unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(
        rock, "Tint", unreal.LinearColor(mountain.r * 0.8, mountain.g * 0.8, mountain.b * 0.8, 1.0))
    unreal.EditorAssetLibrary.save_loaded_asset(rock, False)


def _ensure_materials(palette):
    water = _material("M_IWB_Water", _water)
    _material("M_IWB_Cascade", _cascade)
    _material("M_IWB_Foam", _foam)
    _river_rock(palette)
    return water


# ---------------------------------------------------------------- vegetation
def _clear_corridor(samples, origin_x=0.0):
    """Plants, rocks and mushrooms do not grow in the water."""
    bucket = {}
    for s in samples:
        bucket.setdefault((int(s[0] // 1500), int(s[1] // 1500)), []).append(s)

    def wet(x, y, extra):
        bx, by = int(x // 1500), int(y // 1500)
        for dx in (-1, 0, 1):
            for dy in (-1, 0, 1):
                for s in bucket.get((bx + dx, by + dy), ()):
                    if math.hypot(x - s[0], y - s[1]) < max(s[3] * 0.95, s[3] * 0.5 + extra):
                        return True
        return False

    removed = 0
    for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
        if not w._has_tag(actor, w.GENERATED_TAG) or not any(w._has_tag(actor, layer) for layer in w.PLANT_LAYERS + (w.PATH_TAG,)):
            continue
        # A tree's trunk and canopy reach well past its root: keep trees off the banks.
        extra = 330.0 if w._has_tag(actor, w.TREE_TAG) else 70.0
        components = actor.get_components_by_class(unreal.InstancedStaticMeshComponent)
        if components:
            for component in components:
                doomed = [index for index in range(component.get_instance_count())
                          if wet(component.get_instance_transform(index, True).translation.x - origin_x,
                                 component.get_instance_transform(index, True).translation.y, extra)]
                for index in reversed(doomed):
                    component.remove_instance(index)
                removed += len(doomed)
        else:
            location = actor.get_actor_location()
            if wet(location.x - origin_x, location.y, extra):
                actor.destroy_actor()
                removed += 1
    return removed


# ---------------------------------------------------------------- Houdini
def _actors():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()


def _houdini(code, timeout=300.0):
    """Run Python in the open Houdini through the IWB bridge; returns what it printed."""
    import socket
    try:
        token = open(BRIDGE_TOKEN).read().strip()
    except OSError:
        raise RuntimeError("Open Houdini first: the river is made by Houdini.")
    try:
        with socket.create_connection(("127.0.0.1", BRIDGE_PORT), timeout=5) as connection:
            connection.settimeout(timeout)
            connection.sendall((json.dumps({"token": token, "code": code}) + "\n").encode("utf-8"))
            data = b""
            while not data.endswith(b"\n"):
                chunk = connection.recv(1 << 20)
                if not chunk:
                    break
                data += chunk
    except OSError:
        raise RuntimeError("Houdini is not answering: open Houdini, then try again.")
    reply = json.loads(data.decode("utf-8"))
    if not reply["ok"]:
        raise RuntimeError("Houdini could not make the river: " + reply["output"][-300:])
    return reply["output"]


COOK = """
import json
hou.hda.installFile(HDA_FILE)
old = hou.node("/obj/iwb_river_cook")
if old:
    old.destroy()
work = hou.node("/obj").createNode("geo", "iwb_river_cook")
route = work.createNode("python", "route")
route.parm("python").set("geo = hou.pwd().geometry()\\nscale = geo.addAttrib(hou.attribType.Point, 'scale', (1.0, 1.0, 1.0))\\nline = geo.createPolygon(is_closed=False)\\nfor x, y, z, width in %r:\\n    point = geo.createPoint()\\n    point.setPosition((x, y, z))\\n    point.setAttribValue(scale, (width, width, width))\\n    line.addVertex(point)\\n" % (ROUTE_POINTS,))
river = work.createNode("iwb_river", "river")
river.setInput(0, route)
river.parm("seed").set(RIVER_SEED)
river.parm("cascade_drop").set(RIVER_DROP)
river.parm("foam_size").set(0.55)
geometry = river.geometry()
prims = []
for prim in geometry.prims():
    prims.append([prim.attribValue("unreal_material"), list(prim.normal()),
                  [list(vertex.point().position()) for vertex in prim.vertices()]])
falls = geometry.attribValue("iwb_falls")
work.destroy()
print(json.dumps({"falls": falls, "prims": prims}))
"""


def _cook_in_houdini(samples, seed, drop=None):
    # Houdini works in metres with Y up; Unreal in centimetres with Z up.
    # A little narrower than the carved bed, so the water never overhangs a bank.
    points = [(x / 100.0, level / 100.0, y / 100.0, width * 0.85 / 100.0) for x, y, level, width in samples]
    code = (COOK.replace("HDA_FILE", repr(HDA_PATH.replace("\\", "/"))).replace("ROUTE_POINTS", repr(points))
            .replace("RIVER_SEED", str(int(seed) % 1000))
            .replace("RIVER_DROP", repr((drop or STEP_HEIGHT * 0.6) / 100.0)))
    return json.loads(_houdini(code).strip().splitlines()[-1])


def _river_mesh(result):
    """One static mesh, one section per material, flat-shaded facets."""
    groups = {}
    for material, normal, corners in result["prims"]:
        if len(corners) >= 3:
            groups.setdefault(material, []).append((normal, corners))
    if not unreal.EditorAssetLibrary.does_asset_exist(RIVER_MESH):
        unreal.EditorAssetLibrary.duplicate_asset("/Engine/BasicShapes/Plane", RIVER_MESH)
    asset = unreal.load_asset(RIVER_MESH)
    mesh = unreal.DynamicMesh()
    materials = []
    for material_id, (path, faces) in enumerate(sorted(groups.items())):
        materials.append(unreal.load_asset(path.split(".")[0]) or w._get_tint_material())
        vertices, normals, triangles, uvs = [], [], [], []
        for (hx, hy, hz), corners in faces:
            # Houdini's polygon normal points the other way from the visible
            # side once the axes are swapped into Unreal's (Y up -> Z up).
            up = (-hx, -hz, -hy)
            ue = [(c[0] * 100.0, c[2] * 100.0, c[1] * 100.0) for c in corners]
            for i in range(1, len(ue) - 1):
                a, b, c = ue[0], ue[i], ue[i + 1]
                cross = w._cross((b[0] - a[0], b[1] - a[1], b[2] - a[2]), (c[0] - a[0], c[1] - a[1], c[2] - a[2]))
                # Unreal shows a face whose corners turn against its normal.
                if w._dot(cross, up) > 0.0:
                    b, c = c, b
                base = len(vertices)
                for point in (a, b, c):
                    vertices.append(unreal.Vector(*point))
                    normals.append(unreal.Vector(*up))
                    uvs.append(unreal.Vector2D(point[0] / 400.0, point[1] / 400.0))
                triangles.append(unreal.IntVector(base, base + 1, base + 2))
        buffers = unreal.GeometryScriptSimpleMeshBuffers()
        for name, values in (("vertices", vertices), ("normals", normals), ("triangles", triangles), ("uv0", uvs)):
            buffers.set_editor_property(name, values)
        mesh.append_buffers_to_mesh(buffers, material_id)
    subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    settings = subsystem.get_lod_build_settings(asset, 0)
    settings.set_editor_property("generate_lightmap_u_vs", False)
    settings.set_editor_property("recompute_normals", False)
    subsystem.set_lod_build_settings(asset, 0, settings)
    options = unreal.GeometryScriptCopyMeshToAssetOptions()
    options.set_editor_property("enable_recompute_normals", False)
    options.set_editor_property("replace_materials", True)
    options.set_editor_property("new_materials", materials)
    options.set_editor_property("new_material_slot_names", [m.get_name() for m in materials])
    unreal.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(mesh, asset, options, unreal.GeometryScriptMeshWriteLOD())
    unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
    return asset


# ---------------------------------------------------------------- entry point
def remove_river():
    for actor in _actors():
        if w._has_tag(actor, RIVER_TAG):
            actor.destroy_actor()


def build_river(save=True):
    """Add a river to the world built by the last Build World (finishes over a few frames)."""
    ground = w._load_last_build()
    if not ground:
        raise RuntimeError("Build World first: the river follows an existing world.")
    data = dict(ground)
    terrain = float(data["terrain"])
    # Ask Houdini first: never take the old river away if no new one can be made.
    _houdini("print('ready')", timeout=30.0)
    remove_river()
    w._relief_cache.clear()
    relief = w._terrain_features(data)
    route = _river_route(relief, data)
    samples = _river_profile(relief, route, terrain)
    length = round(len(samples) * SPACING / 100.0)
    samples = _sharpen_falls(samples, STEP_HEIGHT * 0.6)
    before = [list(row) for row in relief.heights]
    carved = _carve(relief, samples)
    # Small streams from the hills: routed on the carved ground, carved in turn.
    streams = [_sharpen_falls(_stream_profile(relief, route, level), STREAM_STEP * 0.6)
               for route, level in _stream_routes(relief, samples, data)]
    for stream in streams:
        carved += _carve(relief, stream, STREAM_BED, STREAM_SLOPE, banks=False)
    # Carve the river again so a stream's banks never cover it where they meet.
    if streams:
        carved += _carve(relief, samples)
    w._ground_mesh(relief, terrain, w.GENERATED_TAG)
    # The carved relief is not the seed's relief: drop it from the cache so a
    # new Build World starts from the clean ground again.
    w._relief_cache.clear()
    panel = w._panel_instances()
    palette = w._palette(w._values(panel[0])) if panel else w._palette(dict(data, palette=w.DEFAULT_PALETTE, theme="Summer"))
    _ensure_materials(palette)
    removed = _clear_corridor(samples + [s for stream in streams for s in stream])
    moved = _reground(relief, before)
    bridges = _build_bridges(relief, data, [samples] + streams, palette)
    result = _cook_in_houdini(samples, data["seed"])
    for number, stream in enumerate(streams):
        part = _cook_in_houdini(stream, int(data["seed"]) + number + 1, STREAM_STEP * 0.6)
        result["prims"] += part["prims"]
        result["falls"] += part["falls"]
    actor = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_object(
        _river_mesh(result), unreal.Vector(0, 0, 0), unreal.Rotator())
    actor.set_editor_property("tags", [unreal.Name(w.GENERATED_TAG), unreal.Name(RIVER_TAG)])
    actor.set_actor_label("IWB_River")
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    component.set_editor_property("cast_shadow", False)
    if save:
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    message = "River ready: {} m of water, {} small streams, {} cascades and {} bridges.".format(
        length, len(streams), result["falls"], bridges)
    unreal.log("IWB river: {} ({} faces, {} plants cleared, {} moved with the ground, {} ground points carved)".format(
        message, len(result["prims"]), removed, moved, carved))
    if panel:
        w._status(panel[0], message)
    return message
