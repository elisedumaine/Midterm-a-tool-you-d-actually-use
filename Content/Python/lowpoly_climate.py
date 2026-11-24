"""Living sky for the World Builder: time passes and the weather evolves in Play.

The C++ actor AForestSky (Source/ForestGame) does the work in Play.  This
module sets it up in the level: it adds Night, Clouds and Wind to the weather
parameter collection, makes the star dome (mesh + material), makes sure both
the rain and the snow actors exist (the sky fades them in and out), and
spawns the sky with the World Builder's choices (time of day, weather, season).

    python Tools/ue.py "import lowpoly_climate as c; c.install()"
"""
import math
import random

import unreal

import interactive_world_builder as w
import iwb_river as river

SKY_TAG = "IWB_Sky"
STARS_MESH = "/Game/WorldBuilder/Meshes/SM_IWB_Stars"
STARS_MATERIAL = "/Game/WorldBuilder/Materials/M_IWB_Stars"
STAR_COUNT = 1400
STAR_DISTANCE = 180000.0     # cm: the dome's radius around the camera
EXTRA_PARAMETERS = ("Night", "Clouds", "Wind")

# The World Builder's moods, as an hour of the day.
START_HOURS = {"Golden hour": 17.6, "Sunset": 18.6, "Day": 13.0, "Blue hour": 19.4, "Night": 23.0}
START_WEATHER = {"Clear": "Clear", "Rain": "Rain", "Snow": "Snow"}


def ensure_parameters():
    """Night, Clouds and Wind join Snow and Wet in MPC_IWB_Weather."""
    collection = w._weather_collection()
    params = list(collection.get_editor_property("scalar_parameters"))
    names = {str(p.get_editor_property("parameter_name")) for p in params}
    added = False
    for name in EXTRA_PARAMETERS:
        if name not in names:
            param = unreal.CollectionScalarParameter()
            param.set_editor_property("parameter_name", name)
            param.set_editor_property("default_value", 0.0)
            params.append(param)
            added = True
    if added:
        collection.set_editor_property("scalar_parameters", params)
        unreal.EditorAssetLibrary.save_loaded_asset(collection, False)
    return collection


def stars_material():
    """Additive, unlit stars that twinkle, shown only at night under a clear sky.

    Each star sits on a unit dome around the origin; the material pushes the
    dome out to STAR_DISTANCE around the camera, so the stars are always far
    away, wherever the player walks."""
    material = unreal.load_asset(STARS_MATERIAL)
    if material and w._material_is_current(material):
        return material
    lib = unreal.MaterialEditingLibrary
    if not material:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "M_IWB_Stars", "/Game/WorldBuilder/Materials", unreal.Material, unreal.MaterialFactoryNew())
    lib.delete_all_material_expressions(material)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)
    collection = ensure_parameters()

    def node(cls, x, y, **props):
        item = lib.create_material_expression(material, cls, x, y)
        for key, value in props.items():
            item.set_editor_property(key, value)
        return item

    def link(a, b, pin="", out=""):
        lib.connect_material_expressions(a, out, b, pin)

    # Visible = Night x (1 - Clouds)
    night = node(unreal.MaterialExpressionCollectionParameter, -1200, -300, collection=collection, parameter_name="Night")
    clouds = node(unreal.MaterialExpressionCollectionParameter, -1200, -180, collection=collection, parameter_name="Clouds")
    clear = node(unreal.MaterialExpressionOneMinus, -1000, -180)
    link(clouds, clear)
    visible = node(unreal.MaterialExpressionMultiply, -820, -240)
    link(night, visible, "A")
    link(clear, visible, "B")
    # Twinkle = 0.7 + 0.3 sin(time * 2.5 + phase), phase in the vertex colour's green.
    colors = node(unreal.MaterialExpressionVertexColor, -1200, 0)
    time = node(unreal.MaterialExpressionTime, -1200, 160)
    speed = node(unreal.MaterialExpressionMultiply, -1040, 160, const_b=2.5)
    link(time, speed, "A")
    spread = node(unreal.MaterialExpressionMultiply, -1040, 60, const_b=60.0)
    link(colors, spread, "A", "G")
    phase = node(unreal.MaterialExpressionAdd, -880, 120)
    link(speed, phase, "A")
    link(spread, phase, "B")
    wave = node(unreal.MaterialExpressionSine, -740, 120)
    link(phase, wave)
    twinkle = node(unreal.MaterialExpressionMultiply, -620, 120, const_b=0.3)
    link(wave, twinkle, "A")
    steady = node(unreal.MaterialExpressionAdd, -500, 120, const_b=0.7)
    link(twinkle, steady, "A")
    bright = node(unreal.MaterialExpressionMultiply, -500, 0, const_b=9.0)
    link(colors, bright, "A", "R")
    shine = node(unreal.MaterialExpressionMultiply, -360, 40)
    link(bright, shine, "A")
    link(steady, shine, "B")
    lit = node(unreal.MaterialExpressionMultiply, -220, -80)
    link(shine, lit, "A")
    link(visible, lit, "B")
    tint = node(unreal.MaterialExpressionConstant3Vector, -360, -200, constant=unreal.LinearColor(0.85, 0.9, 1.0, 1.0))
    emissive = node(unreal.MaterialExpressionMultiply, -80, -120)
    link(tint, emissive, "A")
    link(lit, emissive, "B")
    lib.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    # Position: camera + dome point x distance (the actor stays at the world origin).
    camera = node(unreal.MaterialExpressionCameraPositionWS, -620, 360)
    position = node(unreal.MaterialExpressionWorldPosition, -620, 460)
    stretch = node(unreal.MaterialExpressionMultiply, -460, 460, const_b=STAR_DISTANCE - 1.0)
    link(position, stretch, "A")
    target = node(unreal.MaterialExpressionAdd, -300, 400)
    link(camera, target, "A")
    link(stretch, target, "B")
    lib.connect_material_property(target, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    w._finish_material(material)
    return material


def stars_mesh():
    """STAR_COUNT tiny octahedra on the upper half of a unit dome; brightness and twinkle phase in vertex colour."""
    material = stars_material()
    if unreal.EditorAssetLibrary.does_asset_exist(STARS_MESH):
        asset = unreal.load_asset(STARS_MESH)
        if asset and unreal.EditorAssetLibrary.get_metadata_tag(asset, "IWBVersion") == w.MATERIAL_VERSION:
            return asset
    else:
        unreal.EditorAssetLibrary.duplicate_asset("/Engine/BasicShapes/Plane", STARS_MESH)
    asset = unreal.load_asset(STARS_MESH)
    rng = random.Random(31)
    vertices, colors, normals, triangles = [], [], [], []
    for _ in range(STAR_COUNT):
        # Uniform on the sky, a little denser near the horizon is fine; never below 4 degrees.
        z = rng.uniform(math.sin(math.radians(4.0)), 1.0)
        a = rng.uniform(0.0, 2.0 * math.pi)
        r = math.sqrt(1.0 - z * z)
        centre = (r * math.cos(a), r * math.sin(a), z)
        size = 0.0011 * rng.choice((0.6, 0.7, 0.8, 1.0, 1.0, 1.3, 1.8))
        brightness = rng.uniform(0.25, 1.0) ** 2
        colour = unreal.LinearColor(brightness, rng.random(), 0.0, 1.0)
        tips = [(size, 0, 0), (-size, 0, 0), (0, size, 0), (0, -size, 0), (0, 0, size), (0, 0, -size)]
        corners = [tuple(c + t for c, t in zip(centre, tip)) for tip in tips]
        for a_, b_, c_ in ((0, 2, 4), (2, 1, 4), (1, 3, 4), (3, 0, 4), (2, 0, 5), (1, 2, 5), (3, 1, 5), (0, 3, 5)):
            base = len(vertices)
            for index in (a_, b_, c_):
                vertices.append(unreal.Vector(*corners[index]))
                colors.append(colour)
                normals.append(unreal.Vector(*centre))
            triangles.append(unreal.IntVector(base, base + 1, base + 2))
    buffers = unreal.GeometryScriptSimpleMeshBuffers()
    for key, values in (("vertices", vertices), ("normals", normals), ("vertex_colors", colors), ("triangles", triangles)):
        buffers.set_editor_property(key, values)
    mesh = unreal.DynamicMesh()
    mesh.append_buffers_to_mesh(buffers)
    options = unreal.GeometryScriptCopyMeshToAssetOptions()
    options.set_editor_property("enable_recompute_normals", False)
    options.set_editor_property("replace_materials", True)
    options.set_editor_property("new_materials", [material])
    options.set_editor_property("new_material_slot_names", ["Stars"])
    unreal.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(mesh, asset, options, unreal.GeometryScriptMeshWriteLOD())
    unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).remove_collisions(asset)
    unreal.EditorAssetLibrary.set_metadata_tag(asset, "IWBVersion", w.MATERIAL_VERSION)
    unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
    return asset


LEAF_MESH = "/Game/WorldBuilder/Meshes/SM_IWB_WindLeaf"
LEAF_MATERIAL = "/Game/WorldBuilder/Materials/M_IWB_WindLeaf"
RAINBOW_MESH = "/Game/WorldBuilder/Meshes/SM_IWB_Rainbow"
RAINBOW_MATERIAL = "/Game/WorldBuilder/Materials/M_IWB_Rainbow"
BOLT_MESH = "/Game/WorldBuilder/Meshes/SM_IWB_Bolt"
BOLT_MATERIAL = "/Game/WorldBuilder/Materials/M_IWB_Bolt"
DAY_MINUTES = 12.0
# The rainbow's colours from its inner edge (violet) to its outer edge (red).
SPECTRUM = ((0.45, 0.1, 0.8), (0.1, 0.25, 1.0), (0.1, 0.85, 0.3), (1.0, 0.95, 0.1), (1.0, 0.5, 0.05), (1.0, 0.08, 0.05))


def _new_material(path, blend, shading, two_sided=True):
    """An empty material at path (made if missing), ready to be rebuilt."""
    lib = unreal.MaterialEditingLibrary
    material = unreal.load_asset(path)
    if not material:
        folder, name = path.rsplit("/", 1)
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, folder, unreal.Material, unreal.MaterialFactoryNew())
    lib.delete_all_material_expressions(material)
    material.set_editor_property("blend_mode", blend)
    material.set_editor_property("shading_model", shading)
    material.set_editor_property("two_sided", two_sided)

    def node(cls, x, y, **props):
        item = lib.create_material_expression(material, cls, x, y)
        for key, value in props.items():
            item.set_editor_property(key, value)
        return item

    def link(a, b, pin="", out=""):
        lib.connect_material_expressions(a, out, b, pin)

    return material, node, link


def leaf_material():
    """Two-sided leaves in autumn colours: each leaf picks yellow, orange, red or green from its instance's random value."""
    material = unreal.load_asset(LEAF_MATERIAL)
    if material and w._material_is_current(material):
        return material
    material, node, link = _new_material(LEAF_MATERIAL, unreal.BlendMode.BLEND_OPAQUE, unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    lib = unreal.MaterialEditingLibrary
    pick = node(unreal.MaterialExpressionPerInstanceCustomData, -900, 0, data_index=0)
    colours = [node(unreal.MaterialExpressionConstant3Vector, -900, 120 + i * 90, constant=unreal.LinearColor(*c, 1.0))
               for i, c in enumerate(((0.85, 0.62, 0.08), (0.8, 0.3, 0.04), (0.55, 0.08, 0.04), (0.3, 0.45, 0.08)))]
    current = colours[0]
    for index, colour in enumerate(colours[1:], start=1):
        # From 0..1 to the share between colour index-1 and index.
        scaled = node(unreal.MaterialExpressionMultiply, -700, index * 100, const_b=3.0)
        link(pick, scaled, "A")
        shifted = node(unreal.MaterialExpressionSubtract, -580, index * 100, const_b=float(index - 1))
        link(scaled, shifted, "A")
        share = node(unreal.MaterialExpressionSaturate, -460, index * 100)
        link(shifted, share)
        mix = node(unreal.MaterialExpressionLinearInterpolate, -320, index * 100)
        link(current, mix, "A")
        link(colour, mix, "B")
        link(share, mix, "Alpha")
        current = mix
    lib.connect_material_property(current, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = node(unreal.MaterialExpressionConstant, -320, 500, r=0.8)
    lib.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    w._finish_material(material)
    return material


def _glow_material(path, strength_name, colour_scale, horizon_fade):
    """Unlit additive glow: vertex colour x vertex alpha x a strength parameter.

    horizon_fade hides what lies below the horizon (the rainbow's lower half)."""
    material = unreal.load_asset(path)
    if material and w._material_is_current(material):
        return material
    material, node, link = _new_material(path, unreal.BlendMode.BLEND_ADDITIVE, unreal.MaterialShadingModel.MSM_UNLIT)
    lib = unreal.MaterialEditingLibrary
    colors = node(unreal.MaterialExpressionVertexColor, -900, 0)
    strength = node(unreal.MaterialExpressionScalarParameter, -900, 200, parameter_name=strength_name, default_value=0.0)
    lit = node(unreal.MaterialExpressionMultiply, -700, 40)
    link(colors, lit, "A")
    link(colors, lit, "B", "A")
    shine = node(unreal.MaterialExpressionMultiply, -560, 100, const_b=colour_scale)
    link(lit, shine, "A")
    result = node(unreal.MaterialExpressionMultiply, -420, 140)
    link(shine, result, "A")
    link(strength, result, "B")
    if horizon_fade:
        camera = node(unreal.MaterialExpressionCameraPositionWS, -900, 360)
        position = node(unreal.MaterialExpressionWorldPosition, -900, 460)
        away = node(unreal.MaterialExpressionSubtract, -740, 400)
        link(position, away, "A")
        link(camera, away, "B")
        direction = node(unreal.MaterialExpressionNormalize, -620, 400)
        link(away, direction)
        up = node(unreal.MaterialExpressionComponentMask, -500, 400, r=False, g=False, b=True, a=False)
        link(direction, up)
        steep = node(unreal.MaterialExpressionMultiply, -380, 400, const_b=12.0)
        link(up, steep, "A")
        fade = node(unreal.MaterialExpressionSaturate, -260, 400)
        link(steep, fade)
        faded = node(unreal.MaterialExpressionMultiply, -260, 200)
        link(result, faded, "A")
        link(fade, faded, "B")
        result = faded
    lib.connect_material_property(result, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    w._finish_material(material)
    return material


def _write_mesh(path, vertices, colors, normals, triangles, material, slot):
    """A static mesh from plain lists, without collision."""
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        asset = unreal.load_asset(path)
        if asset and unreal.EditorAssetLibrary.get_metadata_tag(asset, "IWBVersion") == w.MATERIAL_VERSION:
            return asset
    else:
        unreal.EditorAssetLibrary.duplicate_asset("/Engine/BasicShapes/Plane", path)
    asset = unreal.load_asset(path)
    buffers = unreal.GeometryScriptSimpleMeshBuffers()
    for key, values in (("vertices", [unreal.Vector(*v) for v in vertices]),
                        ("normals", [unreal.Vector(*n) for n in normals]),
                        ("vertex_colors", [unreal.LinearColor(*c) for c in colors]),
                        ("triangles", [unreal.IntVector(*t) for t in triangles])):
        buffers.set_editor_property(key, values)
    mesh = unreal.DynamicMesh()
    mesh.append_buffers_to_mesh(buffers)
    options = unreal.GeometryScriptCopyMeshToAssetOptions()
    options.set_editor_property("enable_recompute_normals", False)
    options.set_editor_property("replace_materials", True)
    options.set_editor_property("new_materials", [material])
    options.set_editor_property("new_material_slot_names", [slot])
    unreal.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(mesh, asset, options, unreal.GeometryScriptMeshWriteLOD())
    unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).remove_collisions(asset)
    unreal.EditorAssetLibrary.set_metadata_tag(asset, "IWBVersion", w.MATERIAL_VERSION)
    unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
    return asset


def leaf_mesh():
    """A small folded leaf, 12 cm long."""
    points = [(-6.0, 0.0, 0.0), (0.0, -3.5, 0.8), (6.0, 0.0, 0.0), (0.0, 3.5, 0.8)]
    white = (1.0, 1.0, 1.0, 1.0)
    return _write_mesh(LEAF_MESH, points, [white] * 4, [(0.0, 0.0, 1.0)] * 4, [(0, 2, 1), (0, 3, 2)],
                       leaf_material(), "Leaf")


def _spectrum(t):
    t = max(0.0, min(1.0, t)) * (len(SPECTRUM) - 1)
    index = min(int(t), len(SPECTRUM) - 2)
    f = t - index
    a, b = SPECTRUM[index], SPECTRUM[index + 1]
    return tuple(a[i] + (b[i] - a[i]) * f for i in range(3))


def rainbow_mesh():
    """A ring of colours 40-43 degrees around the +X axis, on a unit sphere.

    The sky points +X at the point opposite the sun and scales it out to
    RainbowDistance; the material hides the part below the horizon."""
    material = _glow_material(RAINBOW_MATERIAL, "Strength", 1.2, True)
    rings, segments = 14, 160
    inner, outer = math.radians(40.0), math.radians(43.0)
    vertices, colors, normals, triangles = [], [], [], []
    for r in range(rings + 1):
        t = r / float(rings)
        theta = inner + (outer - inner) * t
        red, green, blue = _spectrum(t)
        alpha = math.sin(math.pi * t) ** 0.7
        for s in range(segments + 1):
            phi = 2.0 * math.pi * s / segments
            vertices.append((math.cos(theta), math.sin(theta) * math.cos(phi), math.sin(theta) * math.sin(phi)))
            colors.append((red, green, blue, alpha))
            normals.append((-1.0, 0.0, 0.0))
    for r in range(rings):
        for s in range(segments):
            a = r * (segments + 1) + s
            b, c, d = a + 1, a + segments + 1, a + segments + 2
            triangles.extend(((a, c, b), (b, c, d)))
    return _write_mesh(RAINBOW_MESH, vertices, colors, normals, triangles, material, "Rainbow")


def bolt_mesh():
    """A jagged lightning bolt with a few branches, 1 unit tall in its XZ plane (base at 0)."""
    material = _glow_material(BOLT_MATERIAL, "Glow", 40.0, False)
    rng = random.Random(7)
    vertices, colors, normals, triangles = [], [], [], []

    def channel(start, steps, step, drift, width, alpha):
        points, (x, z) = [start], start
        for _ in range(steps):
            x += rng.uniform(-drift, drift) + drift * 0.15
            z -= step * rng.uniform(0.6, 1.4)
            points.append((x, max(0.0, z)))
            if z <= 0.0:
                break
        for (x0, z0), (x1, z1) in zip(points, points[1:]):
            dx, dz = x1 - x0, z1 - z0
            length = math.hypot(dx, dz) or 1.0
            nx, nz = -dz / length * width, dx / length * width
            for half, shade in ((width, alpha), (width * 5.0, alpha * 0.12)):   # the core, then a soft halo
                scale = half / width
                base = len(vertices)
                for px, pz in ((x0 - nx * scale, z0 - nz * scale), (x0 + nx * scale, z0 + nz * scale),
                               (x1 - nx * scale, z1 - nz * scale), (x1 + nx * scale, z1 + nz * scale)):
                    vertices.append((px, 0.0, pz))
                    colors.append((0.8, 0.85, 1.0, shade))
                    normals.append((0.0, 1.0, 0.0))
                triangles.extend(((base, base + 2, base + 1), (base + 1, base + 2, base + 3)))
        return points

    trunk = channel((0.0, 1.0), 26, 0.04, 0.035, 0.006, 1.0)
    for index in (5, 10, 15):
        if index < len(trunk):
            channel(trunk[index], rng.randint(5, 9), 0.035, 0.05, 0.003, 0.7)
    return _write_mesh(BOLT_MESH, vertices, colors, normals, triangles, material, "Bolt")


def _synth_rain(seconds=6.0, seed=5):
    """Steady rain: soft hiss and many small drops."""
    rng = random.Random(seed)
    count = int(seconds * river.SOUND_RATE)
    out, low = [], 0.0
    for _ in range(count):
        white = rng.uniform(-1.0, 1.0)
        low = 0.8 * low + 0.2 * white
        out.append(low * 0.35 + white * 0.08)
    for _ in range(int(seconds * 260)):
        start, length = rng.randrange(count - 300), rng.randint(60, 240)
        amp = rng.uniform(0.05, 0.3)
        for k in range(length):
            out[start + k] += amp * rng.uniform(-1.0, 1.0) * math.exp(-8.0 * k / length)
    return river._seamless(out, river.SOUND_RATE // 2)


def _synth_wind(seconds=10.0, seed=9):
    """Wind: deep filtered noise that swells and fades in gusts."""
    rng = random.Random(seed)
    count = int(seconds * river.SOUND_RATE)
    out, low, mid = [], 0.0, 0.0
    for i in range(count):
        white = rng.uniform(-1.0, 1.0)
        t = i / float(river.SOUND_RATE)
        # The pitch of the whistle rises with each gust.
        gust = 0.55 + 0.3 * math.sin(2.0 * math.pi * t / seconds) + 0.15 * math.sin(2.0 * math.pi * 3.0 * t / seconds + 1.0)
        cut = 0.02 + 0.05 * gust
        low += cut * (white - low)
        mid += (cut * 3.0) * (white - mid)
        out.append((low * 3.0 + (mid - low) * 0.6) * gust)
    return river._seamless(out, river.SOUND_RATE)


def _synth_thunder(seconds=7.0, seed=13):
    """A crack, then a long rolling rumble."""
    rng = random.Random(seed)
    count = int(seconds * river.SOUND_RATE)
    out, low, mid = [], 0.0, 0.0
    rolls = [(rng.uniform(0.3, 4.5), rng.uniform(0.4, 1.0)) for _ in range(7)]
    for i in range(count):
        t = i / float(river.SOUND_RATE)
        white = rng.uniform(-1.0, 1.0)
        low = 0.992 * low + 0.03 * white
        mid = 0.9 * mid + 0.1 * white
        crack = math.exp(-t * 9.0) * (white * 0.7 + mid * 0.6)
        rumble = sum(amp * math.exp(-((t - at) ** 2) / 0.35) for at, amp in rolls) + 0.6 * math.exp(-t * 0.5)
        fade = min(1.0, (seconds - t) / 1.5)
        out.append((crack + low * 2.4 * rumble) * fade * 0.8)
    return out


def _sound(name, synth, loop):
    """A sound synthesised once into a WAV and imported (as the river does)."""
    path = "{}/{}".format(w.AUDIO_DESTINATION, name)
    sound = unreal.load_asset(path)
    if not sound:
        import os
        folder = os.path.join(unreal.Paths.project_intermediate_dir(), "WorldBuilderClimate")
        os.makedirs(folder, exist_ok=True)
        wav = os.path.join(folder, name + ".wav")
        river._write_wav(wav, synth())
        task = unreal.AssetImportTask()
        for key, value in (("filename", wav), ("destination_path", w.AUDIO_DESTINATION), ("destination_name", name),
                           ("automated", True), ("replace_existing", True), ("save", True)):
            task.set_editor_property(key, value)
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        sound = unreal.load_asset(path)
    if sound:
        sound.set_editor_property("looping", loop)
        unreal.EditorAssetLibrary.save_loaded_asset(sound, False)
    return sound


def _valley_height():
    """The lowest ground of the world, where the dawn mist settles."""
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if actor.get_actor_label() == "IWB_Ground":
            origin, extent = actor.get_actor_bounds(False)
            return origin.z - extent.z, max(extent.x, extent.y)
    return 0.0, 10000.0


def _precipitation(weather, tag):
    """The rain or snow actor; made (hidden, ready to fade in) when the World Builder did not."""
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if actor.get_actor_label() == "IWB_" + weather:
            return actor
    count, box, speed, sway, colour, opacity, _size = w.WEATHER_LOOKS[weather]
    actor = w._spawn_mesh(w._precipitation_mesh(weather), unreal.Vector(0, 0, 0), unreal.Vector(1, 1, 1),
                          unreal.Rotator(), tag, w.WEATHER_TAG, "IWB_" + weather)
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    component.set_collision_profile_name("NoCollision")
    component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    actor.set_actor_enable_collision(False)
    component.set_editor_property("cast_shadow", False)
    component.set_editor_property("bounds_scale", 200.0)
    material = component.create_dynamic_material_instance(0)
    if material:
        material.set_vector_parameter_value("Color", unreal.LinearColor(colour[0], colour[1], colour[2], 1.0))
        for name, value in (("Opacity", opacity), ("FallSpeed", speed), ("Sway", sway), ("Box", box)):
            material.set_scalar_parameter_value(name, value)
    # Not the weather chosen in the World Builder: invisible in the editor, the sky fades it in.
    actor.set_is_temporarily_hidden_in_editor(True)
    return actor


def update(data):
    """Keep the sky's starting hour, weather and season in step with the World Builder."""
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if w._has_tag(actor, SKY_TAG):
            actor.set_editor_property("day_minutes", DAY_MINUTES)
            actor.set_editor_property("start_hour", START_HOURS.get(data.get("time_of_day", "Golden hour"), 17.6))
            actor.set_editor_property("start_weather", START_WEATHER.get(data.get("weather", "Clear"), "Clear"))
            actor.set_editor_property("winter", data.get("theme") == "Winter")


def remove():
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if w._has_tag(actor, SKY_TAG):
            unreal.EditorLevelLibrary.destroy_actor(actor)


def dress(sky):
    """The sky's own pieces: stars, leaves, rainbow, lightning, sounds, valley height.
    Plant materials are brought up to date so they sway in the wind."""
    w._get_tint_material()
    import lowpoly_studio
    lowpoly_studio.vertex_color_material()
    sky.get_editor_property("stars").set_static_mesh(stars_mesh())
    sky.get_editor_property("leaves").set_static_mesh(leaf_mesh())
    sky.get_editor_property("rainbow").set_static_mesh(rainbow_mesh())
    sky.get_editor_property("bolt").set_static_mesh(bolt_mesh())
    sky.get_editor_property("rain_sound").set_sound(_sound("SW_IWB_Rain", _synth_rain, True))
    sky.get_editor_property("wind_sound").set_sound(_sound("SW_IWB_Wind", _synth_wind, True))
    sky.set_editor_property("thunder_sound", _sound("SW_IWB_Thunder", _synth_thunder, False))
    floor, radius = _valley_height()
    sky.set_editor_property("valley_height", floor)
    sky.set_editor_property("rainbow_distance", max(6000.0, radius * 0.4))


def install(data=None, tag=None, living=True, day_minutes=DAY_MINUTES):
    """Spawn the living sky for the current world (replaces the previous one)."""
    sky_class = w._gameplay_class("ForestSky")
    if not sky_class:
        raise RuntimeError("the ForestGame module must be rebuilt with ForestSky first")
    data = data or w._load_last_build() or {}
    tag = tag or w.GENERATED_TAG
    remove()
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    sun = next((a for a in actors if isinstance(a, unreal.DirectionalLight)), None)
    fog = next((a for a in actors if isinstance(a, unreal.ExponentialHeightFog)), None)
    sky = unreal.EditorLevelLibrary.spawn_actor_from_class(sky_class, unreal.Vector(0, 0, 0), unreal.Rotator())
    w._tag(sky, tag, w.WEATHER_TAG, "IWB_LivingSky")
    sky.tags = list(sky.tags) + [unreal.Name(SKY_TAG)]
    weather = START_WEATHER.get(data.get("weather", "Clear"), "Clear")
    for name, value in (
        ("start_hour", START_HOURS.get(data.get("time_of_day", "Golden hour"), 17.6)),
        ("day_minutes", float(day_minutes)),
        ("start_weather", weather),
        ("living_weather", bool(living)),
        ("winter", data.get("theme") == "Winter"),
        ("sun", sun),
        ("fog", fog),
        ("rain_actor", _precipitation("Rain", tag)),
        ("snow_actor", _precipitation("Snow", tag)),
        ("weather_parameters", ensure_parameters()),
    ):
        sky.set_editor_property(name, value)
    dress(sky)
    unreal.log("Living sky ready: starts at {:.1f}h, {} , a day lasts {} min.".format(
        sky.get_editor_property("start_hour"), weather, day_minutes))
    return sky
