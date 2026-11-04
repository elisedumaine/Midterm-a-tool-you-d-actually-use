"""Low-Poly Studio: Unreal side of the Low-Poly Nature Kit (Research Project 1).

Houdini makes the assets (the kit's HDAs in Documents/Low-Poly-Nature-Kit/hda);
Unreal asks for them through the same local bridge as the river, then writes
each one into an ordinary static mesh with flat facets and vertex colors.

Test from a terminal, with Unreal and Houdini open:
    python Tools/ue.py "import lowpoly_studio as s; s.bake('tree', {'species': 1, 'seed': 4}, 'SM_LPS_Test')"
"""
import json
import os

import unreal

import interactive_world_builder as w
import iwb_river

KIT = os.path.join(os.path.expanduser("~"), "Documents", "Low-Poly-Nature-Kit").replace("\\", "/")
HDAS = ("lpk_style", "lpk_tree", "lpk_rock")
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
    code = (COOK.replace("HDA_NAMES", repr(HDAS)).replace("KIT", repr(KIT))
            .replace("KIND", repr(kind)).replace("PARMS", repr(dict(parms))))
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
    w._weather_shading(material, colors, "RGB")
    w._finish_material(material)
    return material


def write_mesh(faces, name):
    """One static mesh asset: flat-shaded facets, colors in the vertices, simple collision."""
    path = MESHES + "/" + name
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.duplicate_asset("/Engine/BasicShapes/Cube", path)
    asset = unreal.load_asset(path)
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
    return asset, len(triangles)


def bake(kind, parms, name):
    """Make one kit asset in Houdini and save it as a static mesh in /Game/LowPolyStudio/Meshes."""
    asset, count = write_mesh(cook(kind, parms), name)
    print("baked %s: %d triangles" % (asset.get_path_name(), count))
    return asset
