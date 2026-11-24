"""Low-Poly Character: design your own playable low-poly character (Research Project 1).

Houdini's lpk_character HDA builds a figurine on the joints of Quinn, the
Third Person template's mannequin.  Every face knows its bone, so here each
vertex is bound 100% to that bone ("rigid skinning") and the result becomes a
real Skeletal Mesh on Quinn's skeleton: Quinn's own walk, run and jump
animations move it.  "Play as this character" puts it on the player.

Test from a terminal, with Unreal and Houdini open:
    python Tools/ue.py "import lowpoly_character as c; c.use_as_player(c.bake({'hair': 5}))"
"""
import json
import os

import unreal

import iwb_river
import lowpoly_studio as core

KIT = core.KIT
FOLDER = "/Game/LowPolyStudio/Character"
PLAYER_BLUEPRINT = "/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"
QUINN_MESH = "/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple"

# The arrows of the character creator: (parameter, label, choices).
CLOTH = ("Apricot", "Cream", "Blush", "Sage", "Forest", "Navy", "Denim", "Charcoal", "Lilac", "Mustard", "Red", "White")
CHOICES = (
    ("body", "Body", ("Average", "Slim", "Sturdy")),
    ("head_size", "Head", ("Normal", "Big", "Chibi")),
    ("skin", "Skin", ("Peach", "Light", "Tan", "Brown", "Deep", "Fairy blue", "Forest green")),
    ("hair", "Hair", ("Short", "Bob", "Long", "Ponytail", "Bun", "Spiky", "Mohawk", "Curly", "Bald")),
    ("hair_color", "Hair color", ("Brown", "Black", "Auburn", "Blonde", "Ginger", "Silver", "Pink", "Blue")),
    ("facial_hair", "Face", ("None", "Moustache", "Beard", "Goatee", "Full beard")),
    ("top", "Top", ("T-shirt", "Long sleeves", "Tank top", "Hoodie", "Striped")),
    ("top_color", "Top color", CLOTH),
    ("bottom", "Bottom", ("Trousers", "Shorts", "Skirt")),
    ("bottom_color", "Bottom color", CLOTH),
    ("shoes", "Shoes", ("Sneakers", "Boots", "Sandals")),
    ("shoe_color", "Shoe color", CLOTH),
    ("hat", "Hat", ("None", "Cap", "Beanie", "Wizard", "Flower crown", "Bucket hat")),
    ("extra", "Extra", ("None", "Backpack", "Scarf", "Glasses")),
    ("accent_color", "Extra color", CLOTH),
)
DEFAULTS = {"bottom_color": 6, "shoe_color": 7, "accent_color": 2}

# Runs inside Houdini: one character, its faces with their color, normal and bone.
COOK = """
import json
hou.hda.installFile(KIT + "/hda/lpk_character.hda")
old = hou.node("/obj/lpc_cook")
if old:
    old.destroy()
work = hou.node("/obj").createNode("geo", "lpc_cook")
person = work.createNode("lpk_character", "person")
for parm, value in PARMS.items():
    person.parm(parm).set(value)
faces = []
for prim in person.geometry().prims():
    faces.append([list(prim.attribValue("Cd")), list(prim.normal()), [list(v.point().position()) for v in prim.vertices()],
                  prim.attribValue("bone")])
work.destroy()
print(json.dumps(faces))
"""


def default_design():
    design = {key: 0 for key, _label, _options in CHOICES}
    design.update(DEFAULTS)
    return design


def cook(design):
    """[(color, normal, corners, bone), ...] in meters, Y up, as Houdini made them."""
    code = COOK.replace("KIT", repr(KIT)).replace("PARMS", repr(dict(design)))
    return json.loads(iwb_river._houdini(code).strip().splitlines()[-1])


def preview_faces(faces):
    """The same faces without their bones, for the Studio's turning preview."""
    return [face[:3] for face in faces]


def _skinned_material():
    """The kit's vertex-color material, allowed on skeletal meshes too."""
    material = core.vertex_color_material()
    if not material.get_editor_property("used_with_skeletal_mesh"):
        material.set_editor_property("used_with_skeletal_mesh", True)
        unreal.MaterialEditingLibrary.recompile_material(material)
        unreal.EditorAssetLibrary.save_loaded_asset(material, False)
    return material


def _next_asset_path():
    """A fresh asset name each time: the player may still use the previous one."""
    number = 1
    while unreal.EditorAssetLibrary.does_asset_exist("%s/SKM_LPS_Player_%02d" % (FOLDER, number)):
        number += 1
    return "%s/SKM_LPS_Player_%02d" % (FOLDER, number)


def bake(design):
    """Make the character in Houdini and save it as a Skeletal Mesh on Quinn's skeleton."""
    faces = cook(design)
    skeleton = unreal.load_asset(QUINN_MESH).get_editor_property("skeleton")
    buffers, _count = core.mesh_buffers(preview_faces(faces))
    mesh = unreal.DynamicMesh()
    mesh.append_buffers_to_mesh(buffers)
    lib = unreal.GeometryScript_BoneWeights
    lib.copy_bones_from_skeleton(skeleton, mesh)
    lib.mesh_create_bone_weights(mesh)
    # core.mesh_buffers writes three vertices per triangle, faces in order:
    # repeat each face's bone for all of its vertices.
    index = {}
    vertex = 0
    for _color, _normal, corners, bone in faces:
        if len(corners) < 3:
            continue
        if bone not in index:
            _mesh, valid, bone_index = lib.get_bone_index(mesh, bone)
            if not valid:
                raise RuntimeError("Quinn has no bone named " + bone)
            index[bone] = bone_index
        weight = unreal.GeometryScriptBoneWeight()
        weight.set_editor_property("bone_index", index[bone])
        weight.set_editor_property("weight", 1.0)
        for _ in range(3 * (len(corners) - 2)):
            lib.set_vertex_bone_weights(mesh, vertex, [weight])
            vertex += 1
    options = unreal.GeometryScriptCreateNewSkeletalMeshAssetOptions()
    options.set_editor_property("enable_recompute_normals", False)
    material = unreal.SkeletalMaterial()
    material.set_editor_property("material_interface", _skinned_material())
    material.set_editor_property("material_slot_name", "Kit")
    options.set_editor_property("materials", {"Kit": _skinned_material()})
    path = _next_asset_path()
    asset, outcome = unreal.GeometryScript_NewAssetUtils.create_new_skeletal_mesh_asset_from_mesh(mesh, skeleton, path, options)
    if not asset:
        raise RuntimeError("Unreal could not create the character mesh")
    asset.set_editor_property("physics_asset", unreal.load_asset(QUINN_MESH).get_editor_property("physics_asset"))
    unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
    _save_design(design, asset.get_path_name().split(".")[0])
    print("character baked: %s (%d bones used, %d vertices)" % (path, len(index), vertex))
    return asset


def _design_path():
    return os.path.join(unreal.Paths.project_saved_dir(), "LowPolyStudio", "character.json")


def _save_design(design, mesh_path):
    os.makedirs(os.path.dirname(_design_path()), exist_ok=True)
    with open(_design_path(), "w", encoding="utf-8") as handle:
        json.dump({"design": design, "mesh": mesh_path}, handle, indent=2)


def load_design():
    try:
        with open(_design_path(), "r", encoding="utf-8") as handle:
            return json.load(handle).get("design") or default_design()
    except (OSError, ValueError):
        return default_design()


def _player_mesh_component():
    blueprint = unreal.load_asset(PLAYER_BLUEPRINT)
    return blueprint, unreal.get_default_object(blueprint.generated_class()).get_editor_property("mesh")


def use_as_player(skeletal_mesh):
    """Put a skeletal mesh on the Third Person player (Quinn's animations keep working)."""
    blueprint, component = _player_mesh_component()
    component.set_editor_property("skeletal_mesh_asset", skeletal_mesh)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    unreal.EditorAssetLibrary.save_loaded_asset(blueprint, False)
    return skeletal_mesh


def back_to_quinn():
    return use_as_player(unreal.load_asset(QUINN_MESH))


def current_player_mesh():
    return _player_mesh_component()[1].get_editor_property("skeletal_mesh_asset")


# The Studio's Character tab: the same arrows as every other kind of asset.
core.KINDS["character"] = {
    "label": "Character", "icon": "character", "noun": "character", "hda": "character",
    "arrows": tuple((key, label, tuple((name, {key: index}) for index, name in enumerate(options)))
                    for key, label, options in CHOICES),
    "sliders": (),
    "defaults": DEFAULTS,
    "title_keys": ("hair", "top", "top_color"),
}
if "character" not in core.KIND_ORDER:
    core.KIND_ORDER = core.KIND_ORDER + ("character",)


def design_parms(design):
    """The HDA parameters of a Studio design: one index per arrow."""
    return {key: design["arrows"].get(key, 0) for key, _label, _options in CHOICES}


def play_as(design):
    """Bake the Studio's character design and put it on the player."""
    return use_as_player(bake(design_parms(design)))
