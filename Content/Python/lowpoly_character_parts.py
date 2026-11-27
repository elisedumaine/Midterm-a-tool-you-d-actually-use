"""Low-Poly Character in pieces: the game's character creator dresses the player at run time.

Houdini's lpk_character HDA makes a whole figurine.  Here it is cut into
pieces the game can mix freely: the body (with the head and the hands), the
hair, the beard, the top, the bottom and the shoes.  Each piece is its own
Skeletal Mesh on Quinn's skeleton; in the game they all follow the body's
animation (leader pose).

Which face belongs to which piece: cook the same design twice with only one
colour changed (the top's, the hair's...), the faces that changed colour are
that piece.  Skin that only shows with some clothes goes with those clothes
(the legs with the shorts, the feet with the sandals).

The colours are chosen in the game, so the pieces are baked without them:
each vertex colour says how to paint its face (alpha 0 = keep this colour,
0.5 = the skin colour, 1 = the piece's colour) and its grey is the face's
shade.  M_LPS_CharacterTint does the painting.

    python Tools/ue.py "import lowpoly_character_parts as p; p.bake_all()"
    python Tools/ue.py "import lowpoly_character_parts as p; print(p.palettes())"
"""
import unreal

import lowpoly_character as character

FOLDER = "/Game/LowPolyStudio/CharacterParts"
MATERIAL_FOLDER = "/Game/LowPolyStudio/Materials"
MATERIAL_NAME = "M_LPS_CharacterTint"

BODIES = 3      # Average, Slim, Sturdy
HEADS = 3       # Normal, Big, Chibi
HAIRS = 8       # every hair but "Bald" (8), which is no piece at all
FACES = 5       # facial hair, 0 = none
TOPS = 5
BOTTOMS = 3
SHOES = 3

# The plain design every piece starts from: nothing on the head, no accessory.
BASE = {"hair": 8, "facial_hair": 0, "hat": 0, "extra": 0, "top": 0, "bottom": 0, "shoes": 0}

# Colour parameters, and another value to tell their faces apart.
SWAPS = {"top": "top_color", "bottom": "bottom_color", "shoes": "shoe_color", "hair": "hair_color", "skin": "skin"}

LEG_BONES = ("thigh", "calf", "pelvis")
FOOT_BONES = ("foot", "ball")
TOP_BONES = ("spine", "clavicle", "upperarm", "lowerarm")


def _region(bone):
    """Where a face that is not clothing goes, by the bone it hangs on."""
    if bone.startswith(LEG_BONES):
        return "bottom"
    if bone.startswith(FOOT_BONES):
        return "shoes"
    if bone.startswith(TOP_BONES):
        return "top"
    return "body"


def _design(**changes):
    design = character.default_design()
    design.update(BASE)
    design.update(changes)
    return design


def _other(design, key):
    """Another value of a colour parameter, so its faces change."""
    count = {"skin": 7, "hair_color": 8}.get(key, len(character.CLOTH))
    return (design.get(key, 0) + 3) % count


def classify(design):
    """Each face of the design: (face, piece, slot), slot 'fixed', 'skin' or 'tint'."""
    faces = character.cook(design)
    changed = {}
    for piece, key in SWAPS.items():
        other = dict(design)
        other[key] = _other(design, key)
        swapped = character.cook(other)
        if len(swapped) != len(faces):
            raise RuntimeError("changing %s changed the shape of the character" % key)
        changed[piece] = {i for i in range(len(faces)) if faces[i][0] != swapped[i][0]}
    result = []
    for i, face in enumerate(faces):
        bone = face[3]
        for piece in ("top", "bottom", "shoes", "hair"):
            if i in changed[piece]:
                result.append((face, piece, "tint"))
                break
        else:
            if i in changed["skin"]:
                region = _region(bone)
                # Arm skin is always there (sleeves cover it): it belongs to the body.
                result.append((face, "body" if region == "top" else region, "skin"))
            else:
                result.append((face, _region(bone), "fixed"))
    return result


def _luminance(color):
    return 0.2126 * color[0] + 0.7152 * color[1] + 0.0722 * color[2]


def _paint(entries):
    """Faces with their painting colour: the shade in grey and the slot in the alpha."""
    main = {}
    for slot in ("skin", "tint"):
        counts = {}
        for face, _piece, each in entries:
            if each == slot:
                key = tuple(face[0])
                counts[key] = counts.get(key, 0) + 1
        if counts:
            main[slot] = max(counts, key=counts.get)
    painted = []
    for face, _piece, slot in entries:
        color, normal, corners, bone = face
        if slot == "fixed":
            rgba = (color[0], color[1], color[2], 0.0)
        else:
            shade = min(1.0, _luminance(color) / max(0.001, _luminance(main[slot])))
            rgba = (shade, shade, shade, 0.5 if slot == "skin" else 1.0)
        painted.append((rgba, normal, corners, bone))
    return painted


def _buffers(faces):
    """core.mesh_buffers, keeping the alpha of the vertex colours."""
    vertices, normals, colors, triangles, uvs = [], [], [], [], []
    for rgba, (hx, hy, hz), corners, _bone in faces:
        if len(corners) < 3:
            continue
        points = [(p[0] * 100.0, p[2] * 100.0, p[1] * 100.0) for p in corners]
        normal = unreal.Vector(hx, hz, hy)
        tint = unreal.LinearColor(*rgba)
        for i in range(1, len(points) - 1):
            a, b, c = points[0], points[i], points[i + 1]
            ab = (b[0] - a[0], b[1] - a[1], b[2] - a[2])
            ac = (c[0] - a[0], c[1] - a[1], c[2] - a[2])
            cross = (ab[1] * ac[2] - ab[2] * ac[1], ab[2] * ac[0] - ab[0] * ac[2], ab[0] * ac[1] - ab[1] * ac[0])
            if cross[0] * hx + cross[1] * hz + cross[2] * hy > 0.0:
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
    return buffers


def tint_material():
    """Vertex colours painted with two colours of the game: Skin (alpha 0.5) and Tint (alpha 1)."""
    path = MATERIAL_FOLDER + "/" + MATERIAL_NAME
    material = unreal.load_asset(path)
    if material:
        return material
    lib = unreal.MaterialEditingLibrary
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        MATERIAL_NAME, MATERIAL_FOLDER, unreal.Material, unreal.MaterialFactoryNew())

    def node(cls, x, y, **props):
        item = lib.create_material_expression(material, cls, x, y)
        for key, value in props.items():
            item.set_editor_property(key, value)
        return item

    colors = node(unreal.MaterialExpressionVertexColor, -1000, 0)
    skin = node(unreal.MaterialExpressionVectorParameter, -1000, 200, parameter_name="Skin",
                default_value=unreal.LinearColor(0.72, 0.45, 0.32, 1.0))
    tint = node(unreal.MaterialExpressionVectorParameter, -1000, 400, parameter_name="Tint",
                default_value=unreal.LinearColor(0.86, 0.42, 0.18, 1.0))
    # Alpha 0.5 -> floor 0 (skin), 1 -> floor 1 (tint); ceil says whether to paint at all.
    which = node(unreal.MaterialExpressionFloor, -760, 300)
    lib.connect_material_expressions(colors, "A", which, "")
    paint = node(unreal.MaterialExpressionLinearInterpolate, -600, 300)
    lib.connect_material_expressions(skin, "", paint, "A")
    lib.connect_material_expressions(tint, "", paint, "B")
    lib.connect_material_expressions(which, "", paint, "Alpha")
    shaded = node(unreal.MaterialExpressionMultiply, -440, 200)
    lib.connect_material_expressions(colors, "", shaded, "A")
    lib.connect_material_expressions(paint, "", shaded, "B")
    painted = node(unreal.MaterialExpressionCeil, -440, 60)
    lib.connect_material_expressions(colors, "A", painted, "")
    result = node(unreal.MaterialExpressionLinearInterpolate, -260, 0)
    lib.connect_material_expressions(colors, "", result, "A")
    lib.connect_material_expressions(shaded, "", result, "B")
    lib.connect_material_expressions(painted, "", result, "Alpha")
    lib.connect_material_property(result, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = node(unreal.MaterialExpressionConstant, -260, 200, r=0.85)
    lib.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    material.set_editor_property("used_with_skeletal_mesh", True)
    try:
        import iwb_curved_world
        iwb_curved_world.curve_material(material, save=False)
    except Exception as error:
        unreal.log_warning("Round world: {}".format(error))
    lib.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material, False)
    return material


def _bake_piece(faces, name, physics=False):
    """One piece as a Skeletal Mesh on Quinn's skeleton, every vertex on its face's bone."""
    path = FOLDER + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    if not faces:
        return None
    quinn = unreal.load_asset(character.QUINN_MESH)
    skeleton = quinn.get_editor_property("skeleton")
    mesh = unreal.DynamicMesh()
    mesh.append_buffers_to_mesh(_buffers(faces))
    lib = unreal.GeometryScript_BoneWeights
    lib.copy_bones_from_skeleton(skeleton, mesh)
    lib.mesh_create_bone_weights(mesh)
    index = {}
    vertex = 0
    for _rgba, _normal, corners, bone in faces:
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
    options.set_editor_property("materials", {"Kit": tint_material()})
    asset, _outcome = unreal.GeometryScript_NewAssetUtils.create_new_skeletal_mesh_asset_from_mesh(mesh, skeleton, path, options)
    if not asset:
        raise RuntimeError("Unreal could not create " + name)
    if physics:
        asset.set_editor_property("physics_asset", quinn.get_editor_property("physics_asset"))
    unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
    return asset


def _piece(design, piece):
    return _paint([entry for entry in classify(design) if entry[1] == piece])


def bake_all(bodies=None):
    """Every piece the creator offers (about 150 small meshes, a few minutes); bodies=[0] for one body type."""
    count = 0
    for body in (range(BODIES) if bodies is None else bodies):
        for head in range(HEADS):
            _bake_piece(_piece(_design(body=body, head_size=head), "body"), "SKM_LPC_Body_b%d_h%d" % (body, head), physics=True)
            count += 1
            for hair in range(HAIRS):
                _bake_piece(_piece(_design(body=body, head_size=head, hair=hair), "hair"), "SKM_LPC_Hair_%d_b%d_h%d" % (hair, body, head))
                count += 1
            for face in range(1, FACES):
                # The beard is painted with the hair colour.
                _bake_piece(_piece(_design(body=body, head_size=head, facial_hair=face), "hair"), "SKM_LPC_Face_%d_b%d_h%d" % (face, body, head))
                count += 1
        for top in range(TOPS):
            _bake_piece(_piece(_design(body=body, top=top), "top"), "SKM_LPC_Top_%d_b%d" % (top, body))
            count += 1
        for bottom in range(BOTTOMS):
            _bake_piece(_piece(_design(body=body, bottom=bottom), "bottom"), "SKM_LPC_Bottom_%d_b%d" % (bottom, body))
            count += 1
        for shoes in range(SHOES):
            _bake_piece(_piece(_design(body=body, shoes=shoes), "shoes"), "SKM_LPC_Shoes_%d_b%d" % (shoes, body))
            count += 1
        unreal.log("Character parts: body %d done (%d pieces so far)" % (body, count))
    print("character parts baked: %d" % count)
    return count


def palettes():
    """The HDA's own colours for skin, hair and clothes (to copy into ForestLook.cpp)."""
    out = {}
    for key, piece, choices in (("skin", "skin", 7), ("hair_color", "hair", 8), ("top_color", "top", len(character.CLOTH))):
        colors = []
        for value in range(choices):
            design = _design(hair=0, **{key: value})
            entries = [entry for entry in classify(design) if entry[2] in ("skin", "tint") and (entry[1] == piece or (piece == "skin" and entry[2] == "skin"))]
            counts = {}
            for face, _piece, _slot in entries:
                counts[tuple(face[0])] = counts.get(tuple(face[0]), 0) + 1
            colors.append([round(x, 3) for x in max(counts, key=counts.get)])
        out[key] = colors
    return out


HATS = 6        # 0 = none: Cap, Beanie, Wizard, Flower crown, Bucket hat
EXTRAS = 4      # 0 = none: Backpack, Scarf, Glasses


def _face_key(face):
    return (face[3], tuple(tuple(round(x, 4) for x in point) for point in face[2]))


def _accessory(design):
    """The faces an accessory adds to the plain design, painted with the accent colour (or kept, like the flowers)."""
    plain = dict(design)
    plain["hat"] = 0
    plain["extra"] = 0
    known = {_face_key(face) for face in character.cook(plain)}
    faces = character.cook(design)
    other = dict(design)
    other["accent_color"] = _other(design, "accent_color")
    swapped = character.cook(other)
    entries = []
    for i, face in enumerate(faces):
        if _face_key(face) in known:
            continue
        tinted = len(swapped) == len(faces) and swapped[i][0] != face[0]
        entries.append((face, "accessory", "tint" if tinted else "fixed"))
    return _paint(entries)


def bake_accessories(bodies=None):
    """Hats and extras, bought in the game's shop (about 70 small meshes)."""
    count = 0
    for body in (range(BODIES) if bodies is None else bodies):
        for head in range(HEADS):
            for hat in range(1, HATS):
                _bake_piece(_accessory(_design(body=body, head_size=head, hat=hat)), "SKM_LPC_Hat_%d_b%d_h%d" % (hat, body, head))
                count += 1
            for extra in range(1, EXTRAS):
                _bake_piece(_accessory(_design(body=body, head_size=head, extra=extra)), "SKM_LPC_Extra_%d_b%d_h%d" % (extra, body, head))
                count += 1
        unreal.log("Character accessories: body %d done (%d so far)" % (body, count))
    print("character accessories baked: %d" % count)
    return count
