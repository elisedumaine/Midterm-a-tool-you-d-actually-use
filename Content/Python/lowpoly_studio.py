"""Low-Poly Studio: Unreal side of the Low-Poly Nature Kit (Research Project 1).

Houdini makes the assets (the kit's HDAs in Documents/Low-Poly-Nature-Kit/hda);
Unreal asks for them through the same local bridge as the river, then writes
each one into an ordinary static mesh with flat facets and vertex colors.

Test from a terminal, with Unreal and Houdini open:
    python Tools/ue.py "import lowpoly_studio as s; s.bake('tree', {'species': 1, 'seed': 4}, 'SM_LPS_Test')"
"""
import json
import math
import os
import time

import unreal

import interactive_world_builder as w
import iwb_river

KIT = os.path.join(os.path.expanduser("~"), "Documents", "Low-Poly-Nature-Kit").replace("\\", "/")
HDAS = ("lpk_style", "lpk_tree", "lpk_rock", "lpk_lowpolify")
MESHES = "/Game/LowPolyStudio/Meshes"
MATERIALS = "/Game/LowPolyStudio/Materials"
MATERIAL_PATH = MATERIALS + "/M_LPS_VertexColor"

# Runs inside Houdini: cook one kit HDA and print its faces as JSON.
COOK = """
import json
for name in HDA_NAMES:
    hou.hda.installFile(KIT + "/hda/" + name + ".hda")
old = hou.node("/obj/lps_cook")
if old:
    old.destroy()
work = hou.node("/obj").createNode("geo", "lps_cook")
asset = work.createNode("lpk_" + KIND, "asset")
for parm, value in PARMS.items():
    asset.parm(parm).set(value)
geometry = asset.geometry()
faces = []
for prim in geometry.prims():
    faces.append([list(prim.attribValue("Cd")), list(prim.normal()), [list(v.point().position()) for v in prim.vertices()]])
work.destroy()
print(json.dumps(faces))
"""


def cook(kind, parms):
    """Faces of one kit asset made by Houdini: [(color, normal, [corner, ...]), ...] in meters, Y up."""
    hda = KINDS[kind].get("hda", kind)
    code = (COOK.replace("HDA_NAMES", repr(HDAS)).replace("KIT", repr(KIT))
            .replace("KIND", repr(hda)).replace("PARMS", repr(dict(parms))))
    return json.loads(iwb_river._houdini(code).strip().splitlines()[-1])


def vertex_color_material():
    """Base color from the mesh's vertex colors, with the World Builder's snow and rain."""
    material = unreal.load_asset(MATERIAL_PATH)
    if material and w._material_is_current(material):
        return material
    lib = unreal.MaterialEditingLibrary
    if not material:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "M_LPS_VertexColor", MATERIALS, unreal.Material, unreal.MaterialFactoryNew())
    lib.delete_all_material_expressions(material)
    colors = lib.create_material_expression(material, unreal.MaterialExpressionVertexColor, -1100, 0)
    w._weather_shading(material, colors)
    w._finish_material(material)
    return material


def mesh_buffers(faces):
    """Triangles for Unreal from Houdini faces: flat facets, colors in the vertices."""
    vertices, normals, colors, triangles, uvs = [], [], [], [], []
    for color, (hx, hy, hz), corners in faces:
        if len(corners) < 3:
            continue
        # Houdini: meters, Y up.  Unreal: centimeters, Z up.
        points = [(c[0] * 100.0, c[2] * 100.0, c[1] * 100.0) for c in corners]
        normal = unreal.Vector(hx, hz, hy)
        tint = unreal.LinearColor(color[0], color[1], color[2], 1.0)
        for i in range(1, len(points) - 1):
            a, b, c = points[0], points[i], points[i + 1]
            cross = w._cross((b[0] - a[0], b[1] - a[1], b[2] - a[2]), (c[0] - a[0], c[1] - a[1], c[2] - a[2]))
            # Unreal is left-handed: a face is seen from the side its corners turn away from.
            if w._dot(cross, (hx, hz, hy)) > 0.0:
                b, c = c, b
            base = len(vertices)
            for point in (a, b, c):
                vertices.append(unreal.Vector(*point))
                normals.append(normal)
                colors.append(tint)
                uvs.append(unreal.Vector2D(point[0] / 200.0, point[2] / 200.0))
            triangles.append(unreal.IntVector(base, base + 1, base + 2))
    buffers = unreal.GeometryScriptSimpleMeshBuffers()
    for key, values in (("vertices", vertices), ("normals", normals), ("vertex_colors", colors),
                        ("uv0", uvs), ("triangles", triangles)):
        buffers.set_editor_property(key, values)
    return buffers, len(triangles)


def write_mesh(faces, name):
    """One static mesh asset: flat-shaded facets, colors in the vertices, simple collision."""
    path = MESHES + "/" + name
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.duplicate_asset("/Engine/BasicShapes/Cube", path)
    asset = unreal.load_asset(path)
    buffers, count = mesh_buffers(faces)
    mesh = unreal.DynamicMesh()
    mesh.append_buffers_to_mesh(buffers)
    subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    settings = subsystem.get_lod_build_settings(asset, 0)
    settings.set_editor_property("generate_lightmap_u_vs", False)
    settings.set_editor_property("recompute_normals", False)
    subsystem.set_lod_build_settings(asset, 0, settings)
    options = unreal.GeometryScriptCopyMeshToAssetOptions()
    options.set_editor_property("enable_recompute_normals", False)
    options.set_editor_property("replace_materials", True)
    options.set_editor_property("new_materials", [vertex_color_material()])
    options.set_editor_property("new_material_slot_names", ["Kit"])
    unreal.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(mesh, asset, options, unreal.GeometryScriptMeshWriteLOD())
    unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
    return asset, count


def bake(kind, parms, name):
    """Make one kit asset in Houdini and save it as a static mesh in /Game/LowPolyStudio/Meshes."""
    asset, count = write_mesh(cook(kind, parms), name)
    print("baked %s: %d triangles" % (asset.get_path_name(), count))
    return asset


# ==================================================================== designs
# What the Studio offers for each kind of asset.  Arrows pick among named
# looks (each sets several HDA parameters at once); sliders fine-tune.
KIT_PALETTES = ("Soft Dusk", "Emberwild", "Candy Cloud", "Neon Night", "Coral Lagoon", "Golden Hour", "Frost Violet", "Natural")
PALETTE_ARROW = ("palette", "Palette", tuple((name, {"palette": index}) for index, name in enumerate(KIT_PALETTES)))

KINDS = {
    "tree": {
        "label": "Trees", "icon": "forest", "noun": "tree",
        "arrows": (
            ("species", "Species", (("Broadleaf", {"species": 0}), ("Pine", {"species": 1}),
                                    ("Birch", {"species": 2}), ("Dead tree", {"species": 3}),
                                    ("Palm", {"species": 4}), ("Cherry", {"species": 5}),
                                    ("Willow", {"species": 6}), ("Cactus", {"species": 7}),
                                    ("Giant mushroom", {"species": 8}), ("Bamboo", {"species": 9}))),
            # Silhouette and foliage are starting points: they move the sliders below.
            ("silhouette", "Silhouette", (
                ("Classic", {"height": 6.0, "trunk_width": 0.45, "branch_length": 0.35, "lean": 0.25}),
                ("Baobab", {"height": 8.0, "trunk_width": 2.6, "branch_length": 0.28, "lean": 0.05, "crown_size": 0.6, "branches": 6}),
                ("Giant", {"height": 18.0, "trunk_width": 1.5, "branch_length": 0.4, "lean": 0.2}),
                ("Slender", {"height": 10.0, "trunk_width": 0.22, "branch_length": 0.16, "lean": 0.15}),
                ("Wide", {"height": 5.0, "trunk_width": 0.7, "branch_length": 0.8, "lean": 0.2}),
                ("Twisted", {"height": 6.5, "trunk_width": 0.6, "branch_length": 0.45, "lean": 1.4}),
                ("Bonsai", {"height": 2.0, "trunk_width": 0.35, "branch_length": 0.5, "lean": 0.9}))),
            ("foliage", "Foliage", (("Full", {"branches": 7, "lumpiness": 0.35, "crown_size": 1.0}),
                                    ("Lush", {"branches": 14, "lumpiness": 0.6, "crown_size": 1.3}),
                                    ("Puffy", {"branches": 5, "lumpiness": 0.25, "crown_size": 1.7}),
                                    ("Sparse", {"branches": 3, "lumpiness": 0.15, "crown_size": 0.7}),
                                    ("Wild", {"branches": 20, "lumpiness": 1.0, "crown_size": 0.9}))),
            ("bark", "Bark", (("Dark", {"bark_tone": 1}), ("Light", {"bark_tone": 0}), ("Mixed", {"bark_tone": 2}))),
            PALETTE_ARROW,
        ),
        "sliders": (
            ("height", "Height (m)", 1.5, 25.0, 6.0, True),
            ("trunk_width", "Trunk", 0.08, 3.0, 0.45, True),
            ("branches", "Branches", 0, 24, 7, False),
            ("branch_length", "Reach", 0.08, 1.0, 0.35, True),
            ("crown_size", "Crown", 0.3, 2.6, 1.0, True),
            ("lean", "Lean", 0.0, 1.8, 0.25, True),
            ("lumpiness", "Lumpy", 0.0, 1.2, 0.35, True),
            ("accent_amount", "Accents", 0.0, 1.0, 0.15, True),
            ("detail", "Detail", 1, 5, 2, False),
        ),
    },
    "rock": {
        "label": "Rocks", "icon": "rock", "noun": "rock",
        "arrows": (
            ("type", "Type", (("Boulder", {"type": 1}), ("Pebble", {"type": 0}), ("Slab", {"type": 2}),
                              ("Cliff", {"type": 3}), ("Cluster", {"type": 4}))),
            ("cut", "Cut", (("Chiseled", {"chisel": 9, "cut_depth": 0.45, "roughness": 0.5}),
                            ("Smooth", {"chisel": 2, "cut_depth": 0.15, "roughness": 0.15}),
                            ("Rough", {"chisel": 6, "cut_depth": 0.35, "roughness": 1.2}),
                            ("Shard", {"chisel": 20, "cut_depth": 0.8, "roughness": 0.4}),
                            ("Crystal", {"chisel": 24, "cut_depth": 0.9, "roughness": 0.0}))),
            ("moss", "Moss", (("None", {"moss": 0.0}), ("Light", {"moss": 0.35}), ("Mossy", {"moss": 0.75}),
                              ("Overgrown", {"moss": 1.0}))),
            PALETTE_ARROW,
        ),
        "sliders": (
            ("size", "Size", 0.2, 6.0, 1.0, True),
            ("tallness", "Tallness", 0.25, 3.5, 1.0, True),
            ("roughness", "Rough", 0.0, 1.6, 0.5, True),
            ("chisel", "Cuts", 0, 28, 9, False),
            ("cut_depth", "Cut depth", 0.05, 0.95, 0.45, True),
            ("moss", "Moss", 0.0, 1.0, 0.0, True),
            ("cluster_count", "Cluster", 2, 12, 4, False),
            ("detail", "Detail", 1, 5, 2, False),
        ),
    },
}
KINDS["import"] = {
    # Any model (a file, or made by the AI from a photo) through Low-Polify.
    "label": "Import", "icon": "load", "noun": "object", "hda": "lowpolify",
    "arrows": (
        ("grows_as", "Grows as", (("Landmark", {}), ("Tree", {}), ("Big rock", {}), ("Small stone", {}), ("Forest floor", {}))),
        ("colors", "Colors", (("Palette match", {"color_mode": 0}), ("Original", {"color_mode": 1}),
                              ("One color", {"color_mode": 2}))),
        ("single", "One color", (("Stone", {"single_part": 0}), ("Wood", {"single_part": 1}), ("Leaf", {"single_part": 2}),
                                 ("Accent", {"single_part": 3}), ("Grass", {"single_part": 4}), ("Ink", {"single_part": 5}),
                                 ("Chalk", {"single_part": 6}), ("Blossom", {"single_part": 8}), ("Glow", {"single_part": 9}))),
        ("upright", "Up axis", (("Y up", {"up_axis": 0}), ("Z up", {"up_axis": 1}))),
        ("solid", "Repair", (("Solidify", {"solidify": 1}), ("Keep as is", {"solidify": 0}))),
        PALETTE_ARROW,
    ),
    "sliders": (
        ("facets", "Facets", 40, 1500, 300, False),
        ("height", "Height (m)", 0.2, 10.0, 1.5, True),
        ("turn", "Turn", -180.0, 180.0, 0.0, True),
    ),
}
KIND_ORDER = ("tree", "rock", "import")
VARIANTS = 6               # meshes baked per design added to the forest
IMPORT_LAYERS = {"Landmark": "landmarks", "Tree": "trees", "Big rock": "rocks", "Small stone": "pebbles", "Forest floor": "floor"}


def design_parms(kind, design):
    """HDA parameters for a design: {"arrows": {key: index}, "sliders": {key: value}, "seed": n}."""
    spec = KINDS[kind]
    parms = {"seed": design["seed"]}
    if kind == "import":
        parms["file"] = design.get("file", "").replace("\\", "/")
    for key, _label, options in spec["arrows"]:
        parms.update(options[design["arrows"].get(key, 0) % len(options)][1])
    for key, _label, _low, _high, default, decimals in spec["sliders"]:
        value = design["sliders"].get(key, default)      # sliders win over the arrows' starting points
        parms[key] = float(value) if decimals else int(round(value))
    return parms


def design_title(kind, design):
    if kind == "import":
        name = os.path.splitext(os.path.basename(design.get("file", "")))[0] or "No model yet"
        return name.replace("_", " ")[:24] + " · " + _choice(kind, design, "grows_as")
    if KINDS[kind].get("title_keys"):
        return " · ".join(_choice(kind, design, key) for key in KINDS[kind]["title_keys"])
    names = [options[design["arrows"].get(key, 0) % len(options)][0] for key, _label, options in KINDS[kind]["arrows"]]
    return " · ".join(names[:2] + names[-1:])


def default_design(kind):
    design = {"arrows": {key: 0 for key, _label, _options in KINDS[kind]["arrows"]},
              "sliders": {key: default for key, _label, _low, _high, default, _decimals in KINDS[kind]["sliders"]},
              "seed": 7}
    design["arrows"].update(KINDS[kind].get("defaults", {}))
    if kind == "import":
        design["file"] = ""
    return design


def _choice(kind, design, key):
    options = {k: o for k, _label, o in KINDS[kind]["arrows"]}[key]
    return options[design["arrows"].get(key, 0) % len(options)][0]


# ================================================================= population
# The designs the World Builder grows instead of its stock trees and rocks.
def _population_path():
    folder = os.path.join(unreal.Paths.project_saved_dir(), "LowPolyStudio")
    os.makedirs(folder, exist_ok=True)
    return os.path.join(folder, "population.json")


def load_population():
    try:
        with open(_population_path(), "r", encoding="utf-8") as handle:
            data = json.load(handle)
    except (OSError, ValueError):
        data = {}
    return {"entries": data.get("entries", [])}


def save_population(population):
    with open(_population_path(), "w", encoding="utf-8") as handle:
        json.dump(population, handle, indent=2)


def _layer_of(kind, parms, design=None):
    """Which World Builder layer a design joins: trees, rocks, small stones, landmarks or forest floor."""
    if kind == "import":
        return IMPORT_LAYERS[_choice(kind, design, "grows_as")]
    if kind == "tree":
        return "trees"
    return "pebbles" if parms.get("type") in (0, 2) else "rocks"


def population_meshes():
    """{"trees": [mesh, ...], "rocks": [...], "pebbles": [...]} for the World Builder.

    A design's share repeats its meshes, so a share of 3 grows three times as often."""
    layers = {}
    for entry in load_population()["entries"]:
        meshes = [mesh for mesh in (unreal.load_asset(path) for path in entry["meshes"]) if isinstance(mesh, unreal.StaticMesh)]
        layers.setdefault(entry["layer"], []).extend(meshes * max(1, int(entry.get("share", 1))))
    return layers


def is_studio_mesh(mesh):
    return mesh is not None and mesh.get_path_name().startswith("/Game/LowPolyStudio/")


def add_to_forest(kind, design):
    """Bake VARIANTS seeded versions of a design and add them to the population."""
    population = load_population()
    number = 1 + max([int(entry["id"].split("_")[-1]) for entry in population["entries"]] or [0])
    entry_id = "%s_%02d" % (kind, number)
    parms = design_parms(kind, design)
    meshes = []
    if kind == "import" and not design.get("file"):
        raise RuntimeError("choose a model or a photo first")
    for variant in range(1 if kind == "import" else VARIANTS):
        variant_parms = dict(parms, seed=(design["seed"] + variant * 101) % 1000)
        asset, _count = write_mesh(cook(kind, variant_parms), "SM_LPS_%s_%d" % (entry_id, variant + 1))
        meshes.append(asset.get_path_name().split(".")[0])
    w._ensure_collision([unreal.load_asset(path) for path in meshes])
    population["entries"].append({
        "id": entry_id, "kind": kind, "title": design_title(kind, design), "layer": _layer_of(kind, parms, design),
        "design": design, "meshes": meshes, "share": 1,
    })
    save_population(population)
    return population["entries"][-1]


# ==================================================================== preview
# The design is shown on a live mesh high above the world, filmed by its own
# camera into a texture that the panel displays.  The live mesh changes in a
# few hundredths of a second; nothing is saved until "Add to forest".
BOOTH = unreal.Vector(0.0, 0.0, 100000.0)
BOOTH_TAG = "LPS_Booth"
PREVIEW_SIZE = (448, 220)
SPIN_SECONDS = 15.0        # how long the preview turns after each change
_booth = {}


def _actors_tagged(tag):
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    return [actor for actor in subsystem.get_all_level_actors() if tag in [str(t) for t in actor.tags]]


def ensure_booth():
    """The preview actor, its camera and the texture it films into (made once per session)."""
    try:
        if _booth and _booth["actor"].get_world() and _booth["camera"].get_world():
            return _booth
    except Exception:
        pass
    for old in _actors_tagged(BOOTH_TAG):
        old.destroy_actor()
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    actor = subsystem.spawn_actor_from_class(unreal.DynamicMeshActor, BOOTH)
    actor.tags = [BOOTH_TAG]
    actor.set_actor_label("LPS_Preview")
    camera = subsystem.spawn_actor_from_class(unreal.SceneCapture2D, BOOTH)
    camera.tags = [BOOTH_TAG]
    camera.set_actor_label("LPS_PreviewCamera")
    capture = camera.get_editor_property("capture_component2d")
    # 1.25x the panel size: sharp on a 125% screen, a third of the pixels of 2x.
    target = unreal.RenderingLibrary.create_render_target2d(world, int(PREVIEW_SIZE[0] * 1.25), int(PREVIEW_SIZE[1] * 1.25),
                                                            unreal.TextureRenderTargetFormat.RTF_RGBA8_SRGB)
    capture.set_editor_property("texture_target", target)
    capture.set_editor_property("primitive_render_mode", unreal.SceneCapturePrimitiveRenderMode.PRM_USE_SHOW_ONLY_LIST)
    capture.show_only_actor_components(actor)
    capture.set_editor_property("capture_source", unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
    capture.set_editor_property("fov_angle", 35.0)
    # Pictures are taken on demand (spin_preview), never queued every frame.
    capture.set_editor_property("capture_every_frame", False)
    # Alone against the bright sky, auto exposure washed every color out (dark bark
    # looked white).  One stop darker, and no slow eye adaptation between designs.
    look = capture.get_editor_property("post_process_settings")
    for name, value in (("auto_exposure_bias", -1.0), ("auto_exposure_speed_up", 100.0), ("auto_exposure_speed_down", 100.0)):
        look.set_editor_property("override_" + name, True)
        look.set_editor_property(name, value)
    capture.set_editor_property("post_process_settings", look)
    # A studio lamp beside the camera, so the object is never seen against the sun.
    # The booth is a kilometre above the world: the lamp's reach never touches it.
    lamp = subsystem.spawn_actor_from_class(unreal.PointLight, BOOTH)
    lamp.tags = [BOOTH_TAG]
    lamp.set_actor_label("LPS_PreviewLamp")
    light = lamp.get_editor_property("point_light_component")
    light.set_editor_property("intensity", 40000.0)
    light.set_editor_property("light_color", unreal.Color(r=255, g=236, b=214, a=255))
    light.set_editor_property("cast_shadows", False)
    _booth.clear()
    _booth.update({"actor": actor, "camera": camera, "capture": capture, "target": target, "lamp": lamp, "yaw": 0.0,
                   "clock": 0.0, "frame": -1})
    return _booth


def show_preview(faces):
    """Put a cooked design on the preview actor; returns its triangle count."""
    booth = ensure_booth()
    component = booth["actor"].get_editor_property("dynamic_mesh_component")
    buffers, count = mesh_buffers(faces)
    mesh = component.get_dynamic_mesh()
    mesh.reset()
    mesh.append_buffers_to_mesh(buffers)
    component.set_material(0, vertex_color_material())
    booth["actor"].set_actor_rotation(unreal.Rotator(0.0, 0.0, 0.0), False)
    _frame_preview()
    booth["capture"].capture_scene()
    booth["changed_at"] = time.monotonic()   # a new design: the turntable starts again
    return count


def _frame_preview():
    """Put the camera where the whole asset fits, a little above, looking at its middle."""
    origin, extent = _booth["actor"].get_actor_bounds(False)
    radius = max((extent.x ** 2 + extent.y ** 2 + extent.z ** 2) ** 0.5, 30.0)
    # The picture is wider than tall: fit the object to its height.
    half_vertical = math.atan(math.tan(math.radians(35.0 * 0.5)) * PREVIEW_SIZE[1] / PREVIEW_SIZE[0])
    distance = radius / math.tan(half_vertical) * 0.9
    eye = unreal.Vector(origin.x - distance * 0.96, origin.y - distance * 0.1, origin.z + distance * 0.26)
    _booth["camera"].set_actor_location(eye, False, False)
    _booth["camera"].set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(eye, origin), False)
    if _booth.get("lamp"):
        # From the camera side and a little higher; its reach grows with the object.
        _booth["lamp"].set_actor_location(unreal.Vector(eye.x, eye.y - distance * 0.3, eye.z + distance * 0.35), False, False)
        light = _booth["lamp"].get_editor_property("point_light_component")
        light.set_editor_property("attenuation_radius", distance * 3.0)
        # A soft fill (about half the sun), the same whatever the object's size.
        light.set_editor_property("intensity", 2.0 * (distance / 100.0) ** 2)


def spin_preview(delta):
    """Turn the preview slowly, like a shop turntable."""
    if not _booth:
        return
    try:
        # The turntable runs for a while after each change, then rests (no work at all)
        # until the next arrow or slider: the Studio costs nothing while you look away.
        if time.monotonic() - _booth.get("changed_at", 0.0) > SPIN_SECONDS:
            return
        _booth["yaw"] = (_booth["yaw"] + delta * 24.0) % 360.0
        _booth["clock"] = _booth.get("clock", 0.0) + delta
        # About 7 pictures a second, and only once Unreal has drawn the last one:
        # queuing a capture on every tick while the editor was not drawing filled
        # the memory until Unreal crashed.
        frame = unreal.SystemLibrary.get_frame_count()
        if _booth["clock"] < 0.15 or frame == _booth.get("frame"):
            return
        _booth["clock"], _booth["frame"] = 0.0, frame
        _booth["actor"].set_actor_rotation(unreal.Rotator(0.0, 0.0, _booth["yaw"]), False)
        _booth["capture"].capture_scene()
    except Exception:
        _booth.clear()


def clear_booth():
    for old in _actors_tagged(BOOTH_TAG):
        old.destroy_actor()
    _booth.clear()


# ===================================================== import: files, photos
PHOTO_PYTHON = KIT + "/photo3d/venv/Scripts/python.exe"
PHOTO_SCRIPT = KIT + "/scripts/photo_to_3d.py"
PHOTO_OUTPUT = KIT + "/photos"
MODEL_FILTER = "3D models (*.obj;*.fbx;*.glb;*.gltf;*.ply;*.stl;*.bgeo)|*.obj;*.fbx;*.glb;*.gltf;*.ply;*.stl;*.bgeo"
PHOTO_FILTER = "Photos (*.jpg;*.jpeg;*.png;*.webp)|*.jpg;*.jpeg;*.png;*.webp"


def pick_file(title, file_filter):
    """A Windows file dialog (through PowerShell); returns the chosen path or ''."""
    import subprocess
    script = ("Add-Type -AssemblyName System.Windows.Forms; $d = New-Object System.Windows.Forms.OpenFileDialog; "
              "$d.Title = '%s'; $d.Filter = '%s'; if ($d.ShowDialog() -eq 'OK') { $d.FileName }" % (title, file_filter))
    # Full path: Unreal's PATH does not always contain PowerShell.
    powershell = os.path.join(os.environ.get("SystemRoot", r"C:\Windows"), "System32", "WindowsPowerShell", "v1.0", "powershell.exe")
    result = subprocess.run([powershell, "-NoProfile", "-STA", "-Command", script],
                            capture_output=True, text=True, creationflags=0x08000000)
    return result.stdout.strip()


def photo_ready():
    return os.path.isfile(PHOTO_PYTHON) and os.path.isfile(PHOTO_SCRIPT)


def start_photo_job(image):
    """Run the photo -> 3D AI in its own process; returns (process, model path, log path)."""
    import subprocess
    import time
    os.makedirs(PHOTO_OUTPUT, exist_ok=True)
    name = "%s_%s" % (os.path.splitext(os.path.basename(image))[0][:24], time.strftime("%H%M%S"))
    model = "%s/%s.obj" % (PHOTO_OUTPUT, name)
    log = "%s/%s.log" % (PHOTO_OUTPUT, name)
    with open(log, "w") as handle:
        process = subprocess.Popen([PHOTO_PYTHON, PHOTO_SCRIPT, image, model], stdout=handle, stderr=subprocess.STDOUT,
                                   cwd=KIT, creationflags=0x08000000)
    return process, model, log
