"""A flat world that looks round: the "Round world" slider of the World Builder.

The game stays flat (walking, animals, building, collisions do not change);
only the picture bends.  Every surface material gets a small World Position
Offset that lowers each point by the square of its distance to the camera:

    drop = Curvature * distance^2

so the ground falls away toward a round horizon, like a small planet, and
trees rise over the curve as you walk toward them.  Curvature lives in
MPC_IWB_Weather (the collection every material already reads); 0 = flat.

    import iwb_curved_world as cw
    cw.apply_all()          # add the curve to every material (safe to run again)
    cw.set_roundness(40)    # 0 = flat ... 100 = a very small planet
"""
import unreal

COLLECTION_PATH = "/Game/WorldBuilder/Materials/MPC_IWB_Weather"
PARAMETER = "Curvature"
# Marks the node that carries the curve, so a material is never curved twice.
CURVE_TAG = "IWB curved world"
# Roundness 100 = a planet about 150 m across: 1 / (2 * radius), radius 150 m.
MAX_CURVATURE = 1.0 / 30000.0
# Where the materials of the world live.
FOLDERS = ("/Game/WorldBuilder", "/Game/LowPolyStudio", "/Game/Characters", "/Game/LevelPrototyping")
# The sky, the weather that falls around the camera, and the screen effects stay as they are.
SKIP_NAMES = ("M_IWB_Stars", "M_IWB_Rainbow", "M_IWB_Bolt", "M_IWB_Precipitation", "M_IWB_Weather")

lib = unreal.MaterialEditingLibrary


def _collection():
    """MPC_IWB_Weather, with a Curvature value (0 = flat) added the first time."""
    collection = unreal.load_asset(COLLECTION_PATH)
    if not collection:
        return None
    params = list(collection.get_editor_property("scalar_parameters"))
    if not any(str(param.get_editor_property("parameter_name")) == PARAMETER for param in params):
        param = unreal.CollectionScalarParameter()
        param.set_editor_property("parameter_name", PARAMETER)
        param.set_editor_property("default_value", 0.0)
        params.append(param)
        collection.set_editor_property("scalar_parameters", params)
        unreal.EditorAssetLibrary.save_loaded_asset(collection, False)
    return collection


def is_curved(material):
    node = lib.get_material_property_input_node(material, unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    return bool(node) and str(node.get_editor_property("desc")) == CURVE_TAG


def _wanted(material):
    if not isinstance(material, unreal.Material) or material.get_name() in SKIP_NAMES:
        return False
    try:
        if material.get_editor_property("material_domain") != unreal.MaterialDomain.MD_SURFACE:
            return False
    except Exception:
        pass
    return True


def ensure_instanced_usage(material, save=True):
    """Trees and plants are drawn as instanced meshes; without this flag saved on the
    material, the packaged game shows them in the grey default material."""
    if not _wanted(material) or material.get_editor_property("used_with_instanced_static_meshes"):
        return False
    material.set_editor_property("used_with_instanced_static_meshes", True)
    if save:
        lib.recompile_material(material)
        unreal.EditorAssetLibrary.save_loaded_asset(material, False)
    return True


def curve_material(material, save=True):
    """Adds the curve to one material, on top of whatever moves it already (wind sway...)."""
    if not _wanted(material) or is_curved(material):
        return False
    collection = _collection()
    if not collection:
        return False
    existing = lib.get_material_property_input_node(material, unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    existing_output = lib.get_material_property_input_node_output_name(material, unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET) if existing else ""

    x, y = -1900, 1500

    def node(kind, dx, dy):
        return lib.create_material_expression(material, kind, x + dx, y + dy)

    # How far (on the ground plane) this point is from the camera, squared.
    position = node(unreal.MaterialExpressionWorldPosition, 0, 0)
    try:
        position.set_editor_property("world_position_shader_offset", unreal.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS)
    except Exception:
        pass
    camera = node(unreal.MaterialExpressionCameraPositionWS, 0, 120)
    away = node(unreal.MaterialExpressionSubtract, 220, 40)
    lib.connect_material_expressions(position, "", away, "A")
    lib.connect_material_expressions(camera, "", away, "B")
    flat = node(unreal.MaterialExpressionComponentMask, 400, 40)
    flat.set_editor_property("r", True)
    flat.set_editor_property("g", True)
    flat.set_editor_property("b", False)
    lib.connect_material_expressions(away, "", flat, "")
    squared = node(unreal.MaterialExpressionDotProduct, 580, 40)
    lib.connect_material_expressions(flat, "", squared, "A")
    lib.connect_material_expressions(flat, "", squared, "B")
    # ... times how round the world is, pointing down.
    curvature = node(unreal.MaterialExpressionCollectionParameter, 580, 180)
    curvature.set_editor_property("collection", collection)
    curvature.set_editor_property("parameter_name", PARAMETER)
    drop = node(unreal.MaterialExpressionMultiply, 780, 80)
    lib.connect_material_expressions(squared, "", drop, "A")
    lib.connect_material_expressions(curvature, "", drop, "B")
    down = node(unreal.MaterialExpressionMultiply, 940, 80)
    down.set_editor_property("const_b", -1.0)
    lib.connect_material_expressions(drop, "", down, "A")
    zero = node(unreal.MaterialExpressionConstant2Vector, 940, 200)
    offset = node(unreal.MaterialExpressionAppendVector, 1100, 120)
    lib.connect_material_expressions(zero, "", offset, "A")
    lib.connect_material_expressions(down, "", offset, "B")

    # Added to the material's own movement, if it has one.
    total = node(unreal.MaterialExpressionAdd, 1280, 60)
    if existing:
        lib.connect_material_expressions(existing, existing_output, total, "A")
    else:
        total.set_editor_property("const_a", 0.0)
    lib.connect_material_expressions(offset, "", total, "B")
    total.set_editor_property("desc", CURVE_TAG)
    lib.connect_material_property(total, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    material.set_editor_property("used_with_instanced_static_meshes", True)
    if save:
        lib.recompile_material(material)
        unreal.EditorAssetLibrary.save_loaded_asset(material, False)
    return True


def apply_all():
    """Curves every surface material of the world; materials already curved are left alone."""
    if unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world():
        # Recompiling materials the game is drawing can crash the editor.
        unreal.log_warning("Round world: stop Play first, then try again.")
        return 0
    if not _collection():
        unreal.log_warning("Round world: MPC_IWB_Weather not found (build a world first).")
        return 0
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    count = 0
    for folder in FOLDERS:
        for data in registry.get_assets_by_path(folder, recursive=True):
            if str(data.asset_class_path.asset_name) != "Material":
                continue
            material = unreal.load_asset(str(data.package_name))
            try:
                if curve_material(material):
                    count += 1
                else:
                    ensure_instanced_usage(material)
            except Exception as error:
                unreal.log_warning("Round world: could not curve {}: {}".format(data.package_name, error))
    dress_mushroom_pickups()
    unreal.log("Round world: {} materials curved.".format(count))
    return count


MUSHROOM_BLUEPRINT = "/Game/WorldBuilder/Gameplay/BP_MushroomPickup"
MUSHROOM_MATERIAL = "/Game/WorldBuilder/Materials/MI_IWB_MushroomPickup_{}"


def dress_mushroom_pickups():
    """The mushrooms to pick up came with the kit's own imported materials, which
    cannot take the curve.  They get the forest's colour material instead, with
    the colours the World Builder gave the mushrooms growing in this world."""
    blueprint = unreal.load_asset(MUSHROOM_BLUEPRINT)
    tint = unreal.load_asset("/Game/WorldBuilder/Materials/M_IWB_Tint")
    if not blueprint or not tint:
        return
    colors = []
    for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
        for component in actor.get_components_by_class(unreal.InstancedStaticMeshComponent):
            mesh = component.static_mesh
            if mesh and mesh.get_name() == "mushroom_red" and not colors:
                for material in component.get_materials():
                    try:
                        colors.append(material.get_vector_parameter_value("Tint"))
                    except Exception:
                        colors.append(None)
    defaults = unreal.get_default_object(blueprint.generated_class())
    component = defaults.get_editor_property("mesh")
    mesh = component.static_mesh
    slots = mesh.get_num_sections(0) if mesh else 1
    fallback = (unreal.LinearColor(0.75, 0.12, 0.1, 1.0), unreal.LinearColor(0.9, 0.85, 0.75, 1.0))
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    for slot in range(max(1, slots)):
        path = MUSHROOM_MATERIAL.format(slot)
        instance = unreal.load_asset(path)
        if not instance:
            folder, name = path.rsplit("/", 1)
            instance = tools.create_asset(name, folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        instance.set_editor_property("parent", tint)
        color = colors[slot] if slot < len(colors) and colors[slot] else fallback[min(slot, 1)]
        unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(instance, "Tint", color)
        unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(instance, "Sway", 0.0)
        unreal.MaterialEditingLibrary.update_material_instance(instance)
        unreal.EditorAssetLibrary.save_loaded_asset(instance, False)
        component.set_material(slot, instance)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    unreal.EditorAssetLibrary.save_loaded_asset(blueprint, False)


def set_roundness(value):
    """0 = flat, 100 = a very small planet.  Saved in the collection, so Play sees it too."""
    collection = _collection()
    if not collection:
        return
    curvature = max(0.0, min(100.0, float(value))) / 100.0 * MAX_CURVATURE
    params = list(collection.get_editor_property("scalar_parameters"))
    for param in params:
        if str(param.get_editor_property("parameter_name")) == PARAMETER:
            param.set_editor_property("default_value", curvature)
    collection.set_editor_property("scalar_parameters", params)
    unreal.EditorAssetLibrary.save_loaded_asset(collection, False)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if world:
        try:
            unreal.KismetMaterialLibrary.set_scalar_parameter_value(world, collection, PARAMETER, curvature)
        except Exception:
            pass


def get_roundness():
    collection = _collection()
    if not collection:
        return 0.0
    for param in collection.get_editor_property("scalar_parameters"):
        if str(param.get_editor_property("parameter_name")) == PARAMETER:
            return round(float(param.get_editor_property("default_value")) / MAX_CURVATURE * 100.0)
    return 0.0
