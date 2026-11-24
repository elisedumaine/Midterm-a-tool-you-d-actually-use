"""The magic of the night sky: northern lights, shooting stars and fireflies.

Everything here is a static mesh animated by its material only (no Blueprint,
no C++): it reads Night, Clouds, Wet and Snow from MPC_IWB_Weather, which the
living sky (lowpoly_climate / AForestSky) drives in Play.  So the aurora and
the shooting stars appear when night falls under a clear sky, and the
fireflies dance around the player on dry, warm nights.

    python Tools/ue.py "import lowpoly_night as n; n.install()"
"""
import math
import random

import unreal

import interactive_world_builder as w

NIGHT_TAG = "IWB_Night"
VERSION = "night-1"
FOLDER = "/Game/WorldBuilder/Night"
SKY_DISTANCE = 170000.0      # cm: the aurora and shooting stars ride a dome this far around the camera
FIREFLY_TILE = 6000.0        # cm: fireflies fill a square this wide that follows the player
FIREFLY_COUNT = 420
SHOOTING_STAR_COUNT = 10
SHOOTING_STAR_PERIOD = 45.0  # seconds between two passes of the same streak

lib = unreal.MaterialEditingLibrary


# --- Small material graph helpers -------------------------------------------------

class _Graph:
    def __init__(self, material):
        self.material = material
        self.collection = w._weather_collection()

    def node(self, cls, x=0, y=0, **props):
        item = lib.create_material_expression(self.material, cls, x, y)
        for key, value in props.items():
            item.set_editor_property(key, value)
        return item

    def link(self, a, b, pin="", out=""):
        lib.connect_material_expressions(a, out, b, pin)
        return b

    def param(self, name):
        return self.node(unreal.MaterialExpressionCollectionParameter, collection=self.collection, parameter_name=name)

    def const(self, value):
        return self.node(unreal.MaterialExpressionConstant, r=value)

    def binary(self, cls, a, b, a_out="", b_out=""):
        """a OP b; b may be a number."""
        if isinstance(b, (int, float)):
            item = self.node(cls, const_b=float(b))
            self.link(a, item, "A", a_out)
            return item
        item = self.node(cls)
        self.link(a, item, "A", a_out)
        self.link(b, item, "B", b_out)
        return item

    def mul(self, a, b, a_out="", b_out=""):
        return self.binary(unreal.MaterialExpressionMultiply, a, b, a_out, b_out)

    def add(self, a, b, a_out="", b_out=""):
        return self.binary(unreal.MaterialExpressionAdd, a, b, a_out, b_out)

    def sub(self, a, b, a_out="", b_out=""):
        return self.binary(unreal.MaterialExpressionSubtract, a, b, a_out, b_out)

    def unary(self, cls, a, a_out=""):
        return self.link(a, self.node(cls), "", a_out)

    def sine(self, a, a_out=""):
        """sin(2 pi a): one full wave per unit."""
        return self.unary(unreal.MaterialExpressionSine, a, a_out)

    def saturate(self, a, a_out=""):
        return self.unary(unreal.MaterialExpressionSaturate, a, a_out)

    def frac(self, a, a_out=""):
        return self.unary(unreal.MaterialExpressionFrac, a, a_out)

    def one_minus(self, a, a_out=""):
        return self.unary(unreal.MaterialExpressionOneMinus, a, a_out)

    def colour(self, r, g, b):
        return self.node(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(r, g, b, 1.0))

    def mask(self, a, r=False, g=False, b=False):
        return self.link(a, self.node(unreal.MaterialExpressionComponentMask, r=r, g=g, b=b, a=False))

    def append(self, a, b):
        item = self.node(unreal.MaterialExpressionAppendVector)
        self.link(a, item, "A")
        self.link(b, item, "B")
        return item

    def lerp(self, a, b, alpha):
        item = self.node(unreal.MaterialExpressionLinearInterpolate)
        self.link(a, item, "A")
        self.link(b, item, "B")
        self.link(alpha, item, "Alpha")
        return item

    def clear_night(self):
        """Night x (1 - Clouds): the sky magic only shows on clear nights."""
        return self.mul(self.param("Night"), self.one_minus(self.param("Clouds")))

    def on_sky_dome(self, position=None):
        """Push a unit-dome mesh out to SKY_DISTANCE around the camera, wherever the player walks."""
        position = position or self.node(unreal.MaterialExpressionWorldPosition)
        stretched = self.mul(position, SKY_DISTANCE - 1.0)
        target = self.add(self.node(unreal.MaterialExpressionCameraPositionWS), stretched)
        lib.connect_material_property(target, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)

    def emissive(self, value):
        lib.connect_material_property(value, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)


def _glow_material(name, build):
    """An additive, unlit, two-sided material built by build(graph), rebuilt when VERSION changes."""
    path = FOLDER + "/" + name
    material = unreal.load_asset(path)
    if material and unreal.EditorAssetLibrary.get_metadata_tag(material, "IWBNight") == VERSION:
        return material
    if not material:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    lib.delete_all_material_expressions(material)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)
    build(_Graph(material))
    lib.layout_material_expressions(material)
    lib.recompile_material(material)
    unreal.EditorAssetLibrary.set_metadata_tag(material, "IWBNight", VERSION)
    unreal.EditorAssetLibrary.save_loaded_asset(material, False)
    return material


# --- Materials ---------------------------------------------------------------------------

def _build_aurora(g):
    """Green curtains with violet tops, rippling slowly, with vertical rays."""
    uv = g.node(unreal.MaterialExpressionTextureCoordinate)
    u, v = g.mask(uv, r=True), g.mask(uv, g=True)
    time = g.node(unreal.MaterialExpressionTime)
    # Vertical rays: two waves along the curtain drifting in opposite directions.
    ray_a = g.sine(g.add(g.mul(u, 23.0), g.mul(time, 0.05)))
    ray_b = g.sine(g.sub(g.mul(u, 9.0), g.mul(time, 0.03)))
    rays = g.add(g.mul(g.mul(ray_a, ray_b), 0.45), 0.55)
    # Fades: at both ends of the curtain, a bright lower edge, a soft top.
    ends = g.mul(g.saturate(g.mul(u, 6.0)), g.saturate(g.mul(g.one_minus(u), 6.0)))
    top = g.one_minus(v)
    height = g.mul(g.saturate(g.mul(v, 8.0)), g.mul(top, top))
    # It breathes: brighter and dimmer over a minute or so.
    pulse = g.add(g.mul(g.sine(g.add(g.mul(time, 0.02), g.mul(u, 0.7))), 0.35), 0.65)
    colour = g.lerp(g.colour(0.08, 1.0, 0.42), g.colour(0.6, 0.18, 1.0), v)
    layer = g.node(unreal.MaterialExpressionVertexColor)
    glow = g.mul(g.mul(g.mul(rays, ends), g.mul(height, pulse)), g.mul(g.clear_night(), 5.0))
    glow = g.mul(glow, layer, "", "R")
    g.emissive(g.mul(colour, glow))
    # The curtain waves up and down along its length.
    wave = g.mul(g.sine(g.add(g.mul(u, 3.0), g.mul(time, 0.015))), 0.025)
    zero = g.const(0.0)
    shift = g.append(g.append(zero, zero), wave)
    g.on_sky_dome(g.add(g.node(unreal.MaterialExpressionWorldPosition), shift))


def _build_shooting_stars(g):
    """Each streak lights up from its tail to its head in about two seconds, then waits its turn.

    Vertex colour: R = when it falls (phase), G = speed variation, B = 0 at the tail, 1 at the head."""
    colors = g.node(unreal.MaterialExpressionVertexColor)
    time = g.node(unreal.MaterialExpressionTime)
    pace = g.add(g.mul(colors, 0.6, "G"), 0.7)
    cycle = g.add(g.mul(g.mul(time, 1.0 / SHOOTING_STAR_PERIOD), pace), colors, "", "R")
    # The bright head sweeps along the streak during the first 1/25 of the cycle.
    sweep = g.mul(g.frac(cycle), 25.0)
    behind = g.sub(sweep, colors, "", "B")
    trail = g.mul(g.saturate(g.mul(behind, 50.0)), g.saturate(g.one_minus(g.mul(behind, 2.0))))
    glow = g.mul(trail, g.mul(g.clear_night(), 14.0))
    g.emissive(g.mul(g.colour(0.85, 0.92, 1.0), glow))
    g.on_sky_dome()


def _build_fireflies(g):
    """Tiny lights blinking around the player, on dry nights outside winter.

    The mesh fills a square tile at the world origin; the material wraps it
    around the camera so the fireflies stay put in the world as you walk,
    and come back in at the far side.  Vertex colour G = blink phase."""
    colors = g.node(unreal.MaterialExpressionVertexColor)
    time = g.node(unreal.MaterialExpressionTime)
    position = g.node(unreal.MaterialExpressionWorldPosition)
    camera = g.node(unreal.MaterialExpressionCameraPositionWS)
    phase = g.mask(colors, g=True)
    # A slow wandering flight.
    drift_x = g.mul(g.sine(g.add(g.mul(time, 0.05), phase)), 70.0)
    drift_y = g.mul(g.sine(g.add(g.mul(time, 0.04), g.mul(phase, 3.0))), 70.0)
    bob = g.mul(g.sine(g.add(g.mul(time, 0.12), g.mul(phase, 5.0))), 35.0)
    drift = g.append(drift_x, drift_y)
    flat = g.add(g.mask(position, r=True, g=True), drift)
    eye = g.mask(camera, r=True, g=True)
    # Wrap into a tile centred on the camera.
    relative = g.mul(g.sub(flat, eye), 1.0 / FIREFLY_TILE)
    wrapped = g.mul(g.sub(g.frac(g.add(relative, 0.5)), 0.5), FIREFLY_TILE)
    # Height above the ground, measured from the player's eyes.
    height = g.add(g.add(g.mask(camera, b=True), g.mask(position, b=True)), g.add(bob, -230.0))
    target = g.append(g.add(eye, wrapped), height)
    offset = g.sub(target, position)
    lib.connect_material_property(offset, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    # Fade out towards the tile's edge so nothing pops.
    reach = g.unary(unreal.MaterialExpressionLength, wrapped)
    fade = g.saturate(g.mul(g.one_minus(g.mul(reach, 2.0 / FIREFLY_TILE)), 3.0))
    # Blink: on a third of the time.
    blink = g.saturate(g.sub(g.mul(g.sine(g.add(g.mul(time, 0.35), phase)), 2.0), 0.6))
    dry = g.one_minus(g.mul(g.param("Wet"), 0.85))
    warm = g.one_minus(g.param("Snow"))
    glow = g.mul(g.mul(blink, fade), g.mul(g.mul(g.param("Night"), dry), g.mul(warm, 30.0)))
    g.emissive(g.mul(g.colour(0.75, 1.0, 0.22), glow))


# --- Meshes ---------------------------------------------------------------------------

def _write_mesh(name, material, vertices, colors, triangles, uvs=None):
    path = FOLDER + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        asset = unreal.load_asset(path)
        if asset and unreal.EditorAssetLibrary.get_metadata_tag(asset, "IWBNight") == VERSION:
            return asset
    else:
        unreal.EditorAssetLibrary.duplicate_asset("/Engine/BasicShapes/Plane", path)
    asset = unreal.load_asset(path)
    up = unreal.Vector(0, 0, 1)
    buffers = unreal.GeometryScriptSimpleMeshBuffers()
    values = [("vertices", [unreal.Vector(*v) for v in vertices]), ("normals", [up] * len(vertices)),
              ("vertex_colors", [unreal.LinearColor(c[0], c[1], c[2], 1.0) for c in colors]),
              ("triangles", [unreal.IntVector(*t) for t in triangles])]
    if uvs:
        values.append(("uv0", [unreal.Vector2D(*uv) for uv in uvs]))
    for key, value in values:
        buffers.set_editor_property(key, value)
    mesh = unreal.DynamicMesh()
    mesh.append_buffers_to_mesh(buffers)
    options = unreal.GeometryScriptCopyMeshToAssetOptions()
    options.set_editor_property("enable_recompute_normals", False)
    options.set_editor_property("replace_materials", True)
    options.set_editor_property("new_materials", [material])
    options.set_editor_property("new_material_slot_names", [name])
    unreal.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(mesh, asset, options, unreal.GeometryScriptMeshWriteLOD())
    unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).remove_collisions(asset)
    unreal.EditorAssetLibrary.set_metadata_tag(asset, "IWBNight", VERSION)
    unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
    return asset


def _dome(azimuth, elevation):
    a, e = math.radians(azimuth), math.radians(elevation)
    return (math.cos(e) * math.cos(a), math.cos(e) * math.sin(a), math.sin(e))


def aurora_mesh():
    """Two folded curtains low over the northern horizon, on the unit dome."""
    material = _glow_material("M_IWB_Aurora", _build_aurora)
    vertices, colors, uvs, triangles = [], [], [], []
    columns = 160
    for centre, width, low, tall, strength in ((0.0, 150.0, 9.0, 24.0, 1.0), (35.0, 90.0, 16.0, 18.0, 0.55)):
        start = len(vertices)
        for i in range(columns + 1):
            u = i / float(columns)
            azimuth = centre - width / 2.0 + width * u
            base = low + 4.0 * math.sin(u * math.pi * 3.0 + centre)
            for v in (0.0, 1.0):
                vertices.append(_dome(azimuth, base + tall * v))
                colors.append((strength, 0.0, 0.0))
                uvs.append((u, v))
        for i in range(columns):
            a = start + i * 2
            triangles += [(a, a + 2, a + 1), (a + 1, a + 2, a + 3)]
    return _write_mesh("SM_IWB_Aurora", material, vertices, colors, triangles, uvs)


def shooting_stars_mesh():
    """SHOOTING_STAR_COUNT thin streaks on the unit dome, each tapering from head to tail."""
    material = _glow_material("M_IWB_ShootingStars", _build_shooting_stars)
    rng = random.Random(53)
    vertices, colors, triangles = [], [], []
    segments = 10
    for index in range(SHOOTING_STAR_COUNT):
        centre = _dome(rng.uniform(0.0, 360.0), rng.uniform(25.0, 70.0))
        # A direction along the sky, a little downwards.
        side = (-centre[1], centre[0], 0.0)
        norm = math.sqrt(side[0] ** 2 + side[1] ** 2) or 1.0
        side = (side[0] / norm, side[1] / norm, 0.0)
        down = (centre[1] * side[2] - centre[2] * side[1], centre[2] * side[0] - centre[0] * side[2],
                centre[0] * side[1] - centre[1] * side[0])
        tilt = rng.uniform(-0.6, 0.6)
        direction = tuple(s * math.cos(tilt) - d * math.sin(tilt) for s, d in zip(side, down))
        across = (centre[1] * direction[2] - centre[2] * direction[1], centre[2] * direction[0] - centre[0] * direction[2],
                  centre[0] * direction[1] - centre[1] * direction[0])
        length = rng.uniform(0.06, 0.12)
        phase, pace = (index + rng.random() * 0.5) / SHOOTING_STAR_COUNT, rng.random()
        start = len(vertices)
        for step in range(segments + 1):
            b = step / float(segments)
            point = tuple(c + d * length * (b - 0.5) for c, d in zip(centre, direction))
            half = 0.0011 * (0.15 + 0.85 * b)
            for sign in (-1.0, 1.0):
                vertices.append(tuple(p + a * half * sign for p, a in zip(point, across)))
                colors.append((phase, pace, b))
        for step in range(segments):
            a = start + step * 2
            triangles += [(a, a + 2, a + 1), (a + 1, a + 2, a + 3)]
    return _write_mesh("SM_IWB_ShootingStars", material, vertices, colors, triangles)


def fireflies_mesh():
    """FIREFLY_COUNT tiny octahedra in a tile at the origin, 20 to 260 cm above the ground."""
    material = _glow_material("M_IWB_Fireflies", _build_fireflies)
    rng = random.Random(71)
    vertices, colors, triangles = [], [], []
    half = FIREFLY_TILE / 2.0
    for _ in range(FIREFLY_COUNT):
        centre = (rng.uniform(-half, half), rng.uniform(-half, half), rng.uniform(20.0, 260.0))
        size = rng.uniform(1.6, 2.8)
        colour = (1.0, rng.random(), 0.0)
        tips = [(size, 0, 0), (-size, 0, 0), (0, size, 0), (0, -size, 0), (0, 0, size), (0, 0, -size)]
        start = len(vertices)
        for tip in tips:
            vertices.append(tuple(c + t for c, t in zip(centre, tip)))
            colors.append(colour)
        for a, b, c in ((0, 2, 4), (2, 1, 4), (1, 3, 4), (3, 0, 4), (2, 0, 5), (1, 2, 5), (3, 1, 5), (0, 3, 5)):
            triangles.append((start + a, start + b, start + c))
    return _write_mesh("SM_IWB_Fireflies", material, vertices, colors, triangles)


# --- In the level ------------------------------------------------------------------------

def remove():
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if actor.actor_has_tag(NIGHT_TAG):
            unreal.EditorLevelLibrary.destroy_actor(actor)


def install():
    """Put the aurora, the shooting stars and the fireflies in the open level (replaces old ones)."""
    remove()
    made = []
    for label, mesh in (("IWB_Night_Aurora", aurora_mesh()), ("IWB_Night_ShootingStars", shooting_stars_mesh()),
                        ("IWB_Night_Fireflies", fireflies_mesh())):
        actor = unreal.EditorLevelLibrary.spawn_actor_from_object(mesh, unreal.Vector(0, 0, 0), unreal.Rotator())
        actor.set_actor_label(label)
        actor.tags = [unreal.Name(NIGHT_TAG), unreal.Name(w.WEATHER_TAG)]
        actor.set_actor_enable_collision(False)
        component = actor.get_component_by_class(unreal.StaticMeshComponent)
        component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        component.set_editor_property("cast_shadow", False)
        # Moved by their material: never let the engine cull them.
        component.set_editor_property("bounds_scale", 1000.0)
        made.append(label)
    unreal.log("Night sky ready: " + ", ".join(made))
    return made


def preview(night=1.0):
    """See the night magic in the editor viewport (0 puts the editor's weather back)."""
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    collection = w._weather_collection()
    for name, value in (("Night", night), ("Clouds", 0.0)):
        unreal.MaterialLibrary.set_scalar_parameter_value(world, collection, name, value)
