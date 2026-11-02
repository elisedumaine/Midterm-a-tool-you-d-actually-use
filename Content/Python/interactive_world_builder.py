"""Interactive World Builder Editor Utility Widget runtime."""

import builtins
import json
import math
import os
import random
import zipfile

import unreal


ASSET_PATH = "/Game/WorldBuilder/EUW_WorldBuilderPanel"
SOURCE_EUW_PATH = "/Landmass/Landscape/ErosionBrush/BP/Widgets/EUW_Landscape_Erosion"
HOST_PANEL = "VerticalBox_SettingsPreset"
GENERATED_TAG, PREVIEW_TAG = "IWB_Generated", "IWB_Preview"
TERRAIN_TAG, TREE_TAG, ROCK_TAG, GRASS_TAG = "IWB_Terrain", "IWB_Trees", "IWB_Rocks", "IWB_Grass"
FLOWER_TAG, MOUNTAIN_TAG, WEATHER_TAG = "IWB_Flowers", "IWB_Mountains", "IWB_Weather"
PATH_TAG, LIGHT_TAG, PLAYER_TAG = "IWB_Path", "IWB_Lighting", "IWB_PlayerStart"
LANDMARK_TAG = "IWB_Landmarks"
GAMEPLAY_TAG = "IWB_Gameplay"
AMBIENCE_TAG = "IWB_Ambience"
# CC0 recordings from OpenGameArt (see Downloads/forest_sounds_cc0/LICENSES.txt).
SOUNDS_FOLDER = os.path.join(os.path.expanduser("~"), "Downloads", "forest_sounds_cc0")
AUDIO_DESTINATION = "/Game/WorldBuilder/Audio"
SOUND_FILES = {
    "S_ForestAmbience": "Forest_Ambience_0.mp3.ogg",
    "S_BirdsAmbience": "birds-isaiah658_0.ogg",
    "S_BirdSong": "birdchirping071414.wav",
    "S_Chirp_A": os.path.join("chirps", "chirp_00.wav"),
    "S_Chirp_B": os.path.join("chirps", "chirp_01.wav"),
    "S_Crows": "Crows.ogg",
}
DEFAULT_PALETTE = "Soft Dusk"
DEFAULT_TIME_OF_DAY = "Golden hour"
# Sun pitch and yaw, sun colour, sun strength, and the fog that carries the
# light: a low sun through warm haze is what makes the forest feel poetic.
TIMES_OF_DAY = {
    "Golden hour": {"pitch": -22.0, "yaw": -40.0, "sun": (1.0, 0.72, 0.46), "strength": 0.85, "fog": 0.035, "haze": (0.48, 0.36, 0.26)},
    "Sunset": {"pitch": -10.0, "yaw": -60.0, "sun": (1.0, 0.50, 0.30), "strength": 0.7, "fog": 0.05, "haze": (0.50, 0.30, 0.22)},
    "Day": {"pitch": -50.0, "yaw": -25.0, "sun": (1.0, 0.95, 0.88), "strength": 1.0, "fog": 0.02, "haze": (0.26, 0.30, 0.36)},
    "Blue hour": {"pitch": -2.0, "yaw": -80.0, "sun": (0.58, 0.64, 1.0), "strength": 0.35, "fog": 0.06, "haze": (0.16, 0.19, 0.30)},
}
# strength, sun colour mix, sun colour, haze colour mix, haze colour, fog density
WEATHER_LIGHT = {
    "Rain": (0.35, 0.65, (0.62, 0.68, 0.78), 0.75, (0.36, 0.40, 0.46), 2.4),
    "Snow": (0.8, 0.45, (0.86, 0.92, 1.0), 0.65, (0.66, 0.72, 0.82), 1.7),
}
TEMPLATE_HIDDEN_TAG = "IWB_TemplateHidden"
PAINT_PREFIX, TRUNK_PREFIX, STEM_PREFIX = "IWB_Paint_", "IWB_Trunk_", "IWB_Stem_"
# Everything Refresh Plants is allowed to replace.  Terrain, path, cliffs,
# weather, lighting, and the player start are deliberately absent.
PLANT_LAYERS = (TREE_TAG, ROCK_TAG, GRASS_TAG, FLOWER_TAG, LANDMARK_TAG, GAMEPLAY_TAG)
# Instanced ground cover: (fade start, fade end) in cm, 0 = never fades.  All cast no shadow.
GROUND_COVER_FADE = (
    ("IWB_FineGrass", (2500.0, 4000.0)), ("IWB_Pebbles", (2500.0, 4000.0)),
    ("IWB_Flower", (3500.0, 5500.0)), ("IWB_Grass", (3500.0, 5500.0)), ("IWB_Plants", (3500.0, 5500.0)),
    ("IWB_Mushrooms", (3000.0, 4500.0)), ("IWB_SmallRocks", (4500.0, 7000.0)), ("IWB_Path", (0.0, 0.0)),
)
PRESET_FILENAME = "InteractiveWorldBuilderPreset.json"
LAST_BUILD_FILENAME = "InteractiveWorldBuilderLastBuild.json"
TEMPLATE_STATE_FILENAME = "InteractiveWorldBuilderTemplateState.json"
KENNEY_ZIP_FILENAME = "kenney_nature-kit.zip"
KENNEY_DESTINATION = "/Game/WorldBuilder/Imported/Kenney"
FANTASY_TOWN_ZIP_FILENAME = "kenney_fantasy-town-kit_2.0.zip"
FANTASY_TOWN_DESTINATION = "/Game/WorldBuilder/Imported/FantasyTown"
FANTASY_TOWN_IMPORT_VERSION = "Fantasy Town landmarks v1"
# Kenney Cube Pets (CC0): animated animals with idle, walk, run, eat,
# gesture, and dance animations, in the same low-poly style as the Nature Kit.
ANIMALS_ZIP_FILENAME = "kenney_cube-pets_1.0.zip"
ANIMALS_DESTINATION = "/Game/WorldBuilder/Imported/Animals"
GAMEPLAY_FOLDER = "/Game/WorldBuilder/Gameplay"
MUSHROOM_BLUEPRINT = GAMEPLAY_FOLDER + "/BP_MushroomPickup"
# One Blueprint per forest species.  Each entry: Kenney model, Blueprint,
# in-game name, height (cm), capsule radius, walk and run
# speed, how close an empty-handed player may come (shy), how far a player
# with a mushroom is noticed (curious), follow distance, and how common it is.
ANIMAL_SPECIES = (
    ("animal-deer", "BP_Deer", "the deer", 140.0, 45.0, 130.0, 480.0, 650.0, 1100.0, 380.0, 3),
    ("animal-bunny", "BP_Bunny", "the bunny", 45.0, 22.0, 110.0, 520.0, 450.0, 800.0, 220.0, 3),
    ("animal-fox", "BP_Fox", "the fox", 70.0, 30.0, 150.0, 560.0, 800.0, 1000.0, 300.0, 2),
    ("animal-hog", "BP_Boar", "the boar", 85.0, 45.0, 110.0, 380.0, 350.0, 900.0, 330.0, 1),
    ("animal-beaver", "BP_Beaver", "the beaver", 55.0, 28.0, 90.0, 300.0, 400.0, 800.0, 260.0, 1),
)
ANIMAL_FILES = tuple(species[0] for species in ANIMAL_SPECIES)
# These terms deliberately favour landmarks over generic modular pieces.  The
# importer discovers the exact names in the official archive, so it remains
# resilient to small naming changes between kit revisions.
FANTASY_TOWN_KEYWORDS = (
    "ruin", "tower", "house", "tavern", "well", "fountain", "arch",
    "gate", "bridge", "lantern", "lamp", "market", "wall",
)
# The generator and its terrain-height queries both use this exact visible
# surface.  Keeping the dimensions here prevents a later terrain tweak from
# silently putting every prop a few centimetres above or below the ground.
GROUND_SURFACE_Z = 0.0
TERRAIN_THICKNESS = 60.0
KENNEY_IMPORT_VERSION = "Z-up import v3"
# A focused but varied forest selection.  Every FBX is imported with its Y-up
# source axis converted to Unreal's Z-up axis by the repair/import routine.
KENNEY_FILES = (
    "tree_default", "tree_default_fall", "tree_detailed", "tree_detailed_fall",
    "tree_oak", "tree_oak_fall", "tree_pineRoundA", "tree_pineRoundC",
    "tree_pineTallA", "tree_pineTallB", "tree_small", "tree_tall",
    "rock_largeA", "rock_largeB", "rock_largeD", "rock_largeE", "rock_largeF",
    "rock_smallA", "rock_smallC", "rock_smallF", "rock_smallH", "rock_tallC",
    "grass", "grass_large", "grass_leafs", "grass_leafsLarge",
    "plant_bush", "plant_bushDetailed", "plant_bushLarge", "plant_bushSmall",
    "plant_bushTriangle", "plant_flatShort", "plant_flatTall",
    "flower_purpleA", "flower_purpleC", "flower_redA", "flower_redB", "flower_redC",
    "flower_yellowA", "flower_yellowB", "flower_yellowC",
    "mushroom_redGroup", "mushroom_tanGroup", "stump_old", "stump_roundDetailed",
    "log", "log_large", "log_stack",
    "cliff_blockSlope_rock", "cliff_blockDiagonal_rock", "cliff_cornerLarge_rock",
    "cliff_large_rock", "cliff_top_rock", "cliff_steps_rock",
    # Wider forest variety and the ground-cover layer: pebbles, low plants,
    # mushrooms, and stumps fill the space between trees.
    "tree_blocks", "tree_blocks_fall", "tree_cone", "tree_cone_fall", "tree_fat", "tree_fat_fall",
    "tree_plateau", "tree_plateau_fall", "tree_simple", "tree_simple_fall", "tree_thin", "tree_thin_fall",
    "tree_small_fall", "tree_tall_fall", "tree_pineDefaultA", "tree_pineDefaultB", "tree_pineRoundB",
    "tree_pineRoundD", "tree_pineSmallA", "tree_pineSmallC", "tree_pineTallC",
    "rock_largeC", "rock_smallB", "rock_smallD", "rock_smallE", "rock_smallG", "rock_smallI",
    "rock_smallFlatA", "rock_smallFlatB", "rock_smallFlatC", "rock_tallA", "rock_tallB", "rock_tallE", "rock_tallG",
    "stone_smallA", "stone_smallB", "stone_smallC", "stone_smallD", "stone_smallE", "stone_smallF",
    "stone_smallFlatA", "stone_smallFlatB", "stone_smallFlatC", "stone_smallG", "stone_smallH", "stone_smallI",
    "plant_bushLargeTriangle", "flower_purpleB",
    "mushroom_red", "mushroom_tan", "mushroom_redTall", "mushroom_tanTall",
    "stump_round", "stump_oldTall", "stump_square",
)
MENU_OWNER = "InteractiveWorldBuilderMenu"
_watch_clock = 0.0


def _register_tick(callback):
    """Register a Slate tick callback; a reloaded module drops the old module's ones."""
    handles = getattr(builtins, "_iwb_tick_handles", [])
    handle = unreal.register_slate_post_tick_callback(callback)
    handles.append(handle)
    builtins._iwb_tick_handles = handles
    return handle


for _old_handle in getattr(builtins, "_iwb_tick_handles", []):
    try:
        unreal.unregister_slate_post_tick_callback(_old_handle)
    except Exception:
        pass
builtins._iwb_tick_handles = []

# Every World Builder panel instance keeps its own widgets and callbacks.
# Unreal can hold several instances (one per opened tab); sharing one
# callback list between them left the older panel with dead tab buttons.
_panels = {}
_ui_generation = 0
_watch_handle = None
_action_handle = None
_pending_actions = []
_gameplay_blueprints = None
_widget_asset = None
_tint_material = None
_weather_material = None
_roboto_font = None


# The panel wears the game's soft dusk: warm dark wood, cream text, apricot
# for what is selected, sage and blush as quiet accents.  Linear colours.
UI = {
    "bg": (0.011, 0.009, 0.008), "card": (0.021, 0.017, 0.015), "raised": (0.040, 0.032, 0.027),
    "hover": (0.065, 0.052, 0.043), "pressed": (0.028, 0.022, 0.019), "line": (0.07, 0.058, 0.05),
    "text": (0.90, 0.82, 0.70), "muted": (0.45, 0.39, 0.33), "faint": (0.24, 0.205, 0.175),
    "accent": (0.86, 0.42, 0.18), "accent_hover": (0.95, 0.52, 0.25), "on_accent": (0.03, 0.02, 0.015),
    "sage": (0.36, 0.52, 0.30), "blush": (0.70, 0.27, 0.23),
}
ICON_FOLDER = "/Game/WorldBuilder/UI/Icons"
SEASON_ICONS = {"Spring": "spring", "Summer": "summer", "Autumn": "autumn", "Winter": "winter"}
TIME_ICONS = {"Golden hour": "golden_hour", "Sunset": "sunset", "Day": "sun", "Blue hour": "moon"}
WEATHER_ICONS = {"Clear": "clear", "Rain": "rain", "Snow": "snow"}
PATH_ICONS = {"None": "none", "Trail": "trail", "Network": "network"}
SLIDER_ICONS = {
    "Terrain": "scale", "Hills": "hills", "Vegetation": "vegetation", "ForestCover": "forest", "Wildlife": "wildlife",
    "Lighting": "sun", "LightTint": "warmth", "SkyTint": "cloud", "GroundTint": "ground", "Haze": "haze", "Sparkles": "sparkles",
}


def _color(key, alpha=1.0):
    r, g, b = UI[key] if isinstance(key, str) else key
    return unreal.LinearColor(r, g, b, alpha)


def _rounded_brush(color, radius=6.0, outline=None, outline_width=0.0):
    """A flat rounded box, optionally with a thin outline."""
    brush = unreal.SlateBrush()
    brush.set_editor_property("draw_as", unreal.SlateBrushDrawType.ROUNDED_BOX)
    brush.set_editor_property("tint_color", unreal.SlateColor(color))
    settings = brush.get_editor_property("outline_settings")
    settings.set_editor_property("corner_radii", unreal.Vector4(radius, radius, radius, radius))
    settings.set_editor_property("rounding_type", unreal.SlateBrushRoundingType.FIXED_RADIUS)
    settings.set_editor_property("color", unreal.SlateColor(outline or color))
    settings.set_editor_property("width", outline_width)
    brush.set_editor_property("outline_settings", settings)
    return brush


def _style_button(button, base="raised", hover="hover", pressed="pressed", radius=6.0, outline=None, padding=(8, 4)):
    """Rounded, flat button colours with a visible hover state."""
    try:
        style = button.get_editor_property("widget_style")
        style.set_editor_property("normal", _rounded_brush(_color(base), radius, _color(outline) if outline else None, 1.5 if outline else 0.0))
        style.set_editor_property("hovered", _rounded_brush(_color(hover), radius, _color(outline) if outline else None, 1.5 if outline else 0.0))
        style.set_editor_property("pressed", _rounded_brush(_color(pressed), radius))
        margin = unreal.Margin(padding[0], padding[1], padding[0], padding[1])
        style.set_editor_property("normal_padding", margin)
        style.set_editor_property("pressed_padding", margin)
        button.set_editor_property("widget_style", style)
        button.set_background_color(unreal.LinearColor(1, 1, 1, 1))
    except Exception:
        button.set_background_color(_color(base))


def _ensure_icons():
    """Draw the icon set (Tools/make_icons.py) and import it once."""
    if unreal.EditorAssetLibrary.does_asset_exist(ICON_FOLDER + "/ICO_sun"):
        return
    import importlib.util
    script = os.path.join(unreal.Paths.project_dir(), "Tools", "make_icons.py")
    spec = importlib.util.spec_from_file_location("iwb_make_icons", script)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    folder = os.path.join(unreal.Paths.project_intermediate_dir(), "WorldBuilderIcons")
    os.makedirs(folder, exist_ok=True)
    tasks = []
    for name, (adds, subtracts) in module.ICONS.items():
        path = os.path.join(folder, "ICO_{}.png".format(name))
        module.write_png(path, module.render(adds, subtracts))
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", path)
        task.set_editor_property("destination_path", ICON_FOLDER)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("save", True)
        tasks.append(task)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    for name in module.ICONS:
        texture = unreal.load_asset("{}/ICO_{}".format(ICON_FOLDER, name))
        if texture:
            for prop, value in (("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON),
                                ("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI),
                                ("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)):
                try:
                    texture.set_editor_property(prop, value)
                except Exception:
                    pass
            unreal.EditorAssetLibrary.save_loaded_asset(texture, False)


def _icon(owner, name, icon, size=16.0, color="muted"):
    """A tinted icon image; falls back to an empty spacer when the icon is missing."""
    image = _new(unreal.Image, owner, name)
    texture = unreal.load_asset("{}/ICO_{}".format(ICON_FOLDER, icon))
    if texture:
        image.set_brush_from_texture(texture, False)
    # UE 5.8 stores brush sizes as DeprecateSlateVector2D; the desired-size
    # override works on every version, so prefer it.
    try:
        image.set_desired_size_override(unreal.Vector2D(size, size))
    except Exception:
        brush = image.get_editor_property("brush")
        brush.set_editor_property("image_size", unreal.DeprecateSlateVector2D(size, size))
        image.set_brush(brush)
    image.set_color_and_opacity(_color(color))
    # The size set here is lost once the texture brush syncs to Slate, so the
    # panel applies it again after the build (see _resize_icons).
    _panel(owner).setdefault("icons", []).append((image, size))
    return image


def _resize_icons(owner):
    for image, size in _panel(owner).get("icons", []):
        try:
            image.set_desired_size_override(unreal.Vector2D(size, size))
        except Exception:
            pass


def _size_box(owner, name, child, width=None, height=None):
    box = _new(unreal.SizeBox, owner, name)
    if width:
        box.set_width_override(width)
    if height:
        box.set_height_override(height)
    box.add_child(child)
    return box


def _margin(left=8.0, top=3.0, right=8.0, bottom=3.0):
    return unreal.Margin(left, top, right, bottom)


def _panel(widget):
    return _panels.setdefault(widget.get_path_name(), {"names": {}, "callbacks": [], "built": False, "bound": False})


def _new(cls, owner, name):
    """Create a widget with a fresh object name, and remember it under its plain name.

    Rebuilt panels used to create new widgets with the same names as the
    removed ones; looking a name up could then return the old, detached
    widget (a tab switcher nobody could see), so tabs stopped responding.
    """
    item = cls(outer=owner, name="{}_{}".format(name, _ui_generation))
    _panel(owner)["names"][name] = item
    return item


def _add(panel, child, padding=None, fill=False):
    slot = panel.add_child(child)
    if padding is not None and hasattr(slot, "set_padding"):
        slot.set_padding(padding)
    if fill and hasattr(slot, "set_size"):
        size = unreal.SlateChildSize()
        size.set_editor_property("size_rule", unreal.SlateSizeRule.FILL)
        size.set_editor_property("value", 1.0)
        slot.set_size(size)
    return slot


def _font(size, bold=False):
    """Use Unreal's shipped Roboto face; a size-only font renders as missing glyphs in this host EUW."""
    global _roboto_font
    if _roboto_font is None:
        _roboto_font = unreal.load_asset("/Engine/EngineFonts/Roboto")
    if _roboto_font:
        return unreal.SlateFontInfo(
            font_object=_roboto_font,
            typeface_font_name="Bold" if bold else "Regular",
            size=size,
        )
    return unreal.SlateFontInfo(size=size)


WRAP_WIDTH = 270.0
PANEL_WIDTH = 340.0


def _text(owner, name, text, strong=False, color=None, size=13, wrap=False):
    """A text block.  wrap=True breaks lines at a fixed width: automatic
    wrapping remembered the widest size the panel ever had and pushed the
    whole panel wider than its window."""
    item = _new(unreal.TextBlock, owner, name)
    item.set_text(text)
    item.set_auto_wrap_text(False)
    if wrap:
        item.set_editor_property("wrap_text_at", WRAP_WIDTH)
    item.set_font(_font(size, strong))
    if color:
        item.set_color_and_opacity(unreal.SlateColor(color))
    return item


def _button(owner, name, text, tone="soft", size=10, icon=None, stacked=False):
    """A small rounded button with an optional icon; tone picks its colour family."""
    item = _new(unreal.Button, owner, name)
    tones = {
        "primary": ("accent", "accent_hover", "accent", "on_accent"),
        "soft": ("raised", "hover", "pressed", "text"),
        "danger": ("blush", (0.80, 0.34, 0.29), (0.55, 0.2, 0.17), "text"),
    }
    base, hover, pressed, ink = tones.get(tone, tones["soft"])
    _style_button(item, base, hover, pressed)
    content = _new(unreal.VerticalBox if stacked else unreal.HorizontalBox, owner, name + "Content")
    if icon:
        icon_slot = content.add_child(_icon(owner, name + "Icon", icon, size + 4, ink))
        icon_slot.set_padding(_margin(0, 0, 0, 2) if stacked else _margin(0, 0, 5, 0))
        if stacked:
            icon_slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)
        else:
            icon_slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
    label = _text(owner, name + "Label", text, True, _color(ink), size)
    label_slot = content.add_child(label)
    if stacked:
        label_slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)
    else:
        label_slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
    _center(item.add_child(content))
    return item


def _center(slot):
    """Centre a child inside its button."""
    try:
        slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)
        slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
        slot.set_padding(_margin(0, 0, 0, 0))
    except Exception:
        pass
    return slot


def _tint(item, color):
    """Text takes a SlateColor; images take a LinearColor."""
    if isinstance(item, unreal.TextBlock):
        item.set_color_and_opacity(unreal.SlateColor(color))
    else:
        item.set_color_and_opacity(color)


def _section(owner, parent, name, title, caption=None, icon=None):
    """A compact rounded card: a small icon and title; the hint is a tooltip."""
    card = _new(unreal.Border, owner, name + "Card")
    card.set_brush(_rounded_brush(_color("card"), 8.0, _color("line"), 1.0))
    card.set_padding(_margin(10, 7, 10, 8))
    content = _new(unreal.VerticalBox, owner, name + "Content")
    card.add_child(content)
    heading = _new(unreal.HorizontalBox, owner, name + "Heading")
    if icon:
        icon_slot = heading.add_child(_icon(owner, name + "TitleIcon", icon, 13, "accent"))
        icon_slot.set_padding(_margin(0, 0, 6, 0))
        icon_slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
    _add(heading, _text(owner, name + "Title", title, True, _color("text"), 10), _margin(0, 0, 0, 0), fill=True)
    if caption:
        card.set_tool_tip_text(caption)
    _add(content, heading, _margin(0, 0, 0, 4))
    _add(parent, card, _margin(8, 3, 8, 3))
    return content


def _field_row(owner, panel, key, label, default, trailing=None):
    row = _new(unreal.HorizontalBox, owner, key + "Row")
    caption = _text(owner, key + "Label", label, False, _color("text"), 9)
    caption.set_min_desired_width(64.0)
    caption_slot = row.add_child(caption)
    caption_slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
    field = _new(unreal.EditableTextBox, owner, key + "Box")
    field.set_text(default)
    try:
        style = field.get_editor_property("widget_style")
        for prop, color in (("background_image_normal", "raised"), ("background_image_hovered", "hover"),
                            ("background_image_focused", "hover"), ("background_image_read_only", "raised")):
            style.set_editor_property(prop, _rounded_brush(_color(color), 5.0, _color("line"), 1.0))
        style.set_editor_property("foreground_color", unreal.SlateColor(_color("text")))
        style.set_editor_property("focused_foreground_color", unreal.SlateColor(_color("text")))
        text_style = style.get_editor_property("text_style")
        text_style.set_editor_property("font", _font(9))
        text_style.set_editor_property("color_and_opacity", unreal.SlateColor(_color("text")))
        style.set_editor_property("text_style", text_style)
        style.set_editor_property("padding", unreal.Margin(6, 3, 6, 3))
        field.set_editor_property("widget_style", style)
    except Exception:
        pass
    _add(row, field, _margin(0, 1, 0, 1), fill=True)
    if trailing:
        _add(row, trailing, _margin(4, 1, 0, 1))
    _add(panel, row, _margin(0, 1, 0, 1))


def _slider_row(owner, panel, key, label, value, minimum, maximum, decimals=False):
    """Icon, label, a warm slider, and its value in a small chip, on one tight line."""
    row = _new(unreal.HorizontalBox, owner, key + "Row")
    if key in SLIDER_ICONS:
        icon_slot = row.add_child(_icon(owner, key + "Icon", SLIDER_ICONS[key], 12, "muted"))
        icon_slot.set_padding(_margin(0, 0, 6, 0))
        icon_slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
    caption = _text(owner, key + "Label", label, False, _color("text"), 9)
    caption.set_min_desired_width(86.0)
    caption_slot = row.add_child(caption)
    caption_slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
    slider = _new(unreal.Slider, owner, key + "Slider")
    slider.set_min_value(minimum)
    slider.set_max_value(maximum)
    slider.set_value(value)
    try:
        slider.set_slider_bar_color(_color("faint"))
        slider.set_slider_handle_color(_color("accent"))
        style = slider.get_editor_property("widget_style")
        style.set_editor_property("bar_thickness", 3.0)
        slider.set_editor_property("widget_style", style)
    except Exception:
        pass
    # A Fill slot gives the Slider a visible rail instead of a tiny isolated handle.
    _add(row, slider, _margin(0, 0, 6, 0), fill=True)
    chip = _new(unreal.Border, owner, key + "Chip")
    chip.set_brush(_rounded_brush(_color("raised"), 4.0))
    chip.set_padding(_margin(4, 1, 4, 1))
    readout = _text(owner, key + "Readout", f"{value:.2f}" if decimals else str(round(value)), True, _color("text"), 8)
    readout.set_editor_property("justification", unreal.TextJustify.CENTER)
    chip.add_child(readout)
    chip_slot = row.add_child(_size_box(owner, key + "ChipBox", chip, width=40.0))
    chip_slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
    _add(panel, _size_box(owner, key + "RowBox", row, height=22.0), _margin(0, 0, 0, 0))


SLIDERS = (
    ("Terrain", "terrain", "World scale", 20000.0, 3000.0, 30000.0, False),
    ("Hills", "hills", "Hills", 50.0, 0.0, 100.0, False),
    ("Vegetation", "vegetation", "Vegetation", 50.0, 0.0, 100.0, False),
    ("ForestCover", "forest_cover", "Forest cover", 50.0, 0.0, 100.0, False),
    ("Wildlife", "wildlife", "Wildlife", 50.0, 0.0, 100.0, False),
    ("Lighting", "lighting", "Sun strength", 8.0, 1.0, 15.0, True),
    ("LightTint", "light_tint", "Sun warmth", 0.12, 0.0, 1.0, True),
    ("SkyTint", "sky_tint", "Sky warmth", 0.55, 0.0, 1.0, True),
    ("GroundTint", "ground_tint", "Ground richness", 0.32, 0.0, 1.0, True),
    ("Haze", "haze", "Haze", 50.0, 0.0, 100.0, False),
    ("Sparkles", "sparkles", "Sparkles", 50.0, 0.0, 100.0, False),
)
SLIDER_SPEC = {spec[0]: spec for spec in SLIDERS}
# Sliders that relight the existing world as soon as they are released.
LIVE_MOOD_SLIDERS = ("Lighting", "LightTint", "SkyTint", "GroundTint", "Haze", "Sparkles")
PATH_OPTIONS = ("None", "Trail", "Network")
WEATHER_OPTIONS = ("Clear", "Rain", "Snow")


class _Choice:
    """A set of buttons that behaves like a dropdown (and replaces it).

    Unreal's dropdown list rendered black-on-black in this panel, so every
    choice is shown as tiles.  It answers the same calls as a ComboBoxString
    (get/set_selected_option, on_selection_changed), so the rest of the tool
    did not have to change.
    """

    def __init__(self, options, default):
        self.options, self.value = list(options), default
        self.tiles, self.value_text, self.listeners = [], None, []
        self.on_selection_changed = self

    def add_callable(self, callback):
        self.listeners.append(callback)

    def get_selected_option(self):
        return self.value

    def set_selected_option(self, option):
        self._set(option, unreal.SelectInfo.DIRECT)

    def choose(self, option):
        self._set(option, unreal.SelectInfo.ON_MOUSE_CLICK)

    def step(self, delta):
        index = self.options.index(self.value) if self.value in self.options else 0
        self.choose(self.options[(index + delta) % len(self.options)])

    def _set(self, option, how):
        if option not in self.options:
            return
        self.value = option
        self.refresh()
        for callback in list(self.listeners):
            callback(option, how)

    def refresh(self):
        for option, button, parts in self.tiles:
            selected = option == self.value
            if parts == "swatch":
                _style_button(button, "raised", "hover", "pressed", 6.0, "accent" if selected else "line", padding=(3, 3))
                continue
            _style_button(button, "accent" if selected else "raised", "accent_hover" if selected else "hover", "pressed", 6.0, padding=(3, 3))
            ink = _color("on_accent" if selected else "text")
            for part in parts:
                _tint(part, ink)
        if self.value_text:
            self.value_text.set_text(self.value)


def _choose_callback(choice, option):
    # Unreal counts default arguments, so each button gets its own closure.
    def callback():
        choice.choose(option)
    return callback


def _choice_row(owner, panel, key, label, options, default, stepper=False, icons=None):
    """Compact icon-and-label pills for a choice, registered under `key`."""
    state = _panel(owner)
    choice = _Choice(options, default)
    row = _new(unreal.HorizontalBox, owner, key + "Row")
    caption = _text(owner, key + "Label", label, False, _color("text"), 9)
    caption.set_min_desired_width(64.0)
    caption_slot = row.add_child(caption)
    caption_slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
    for index, option in enumerate(options):
        button = _new(unreal.Button, owner, "{}Option{}".format(key, index))
        button.set_tool_tip_text(option)
        tile = _new(unreal.VerticalBox, owner, "{}Tile{}".format(key, index))
        parts = []
        if icons and option in icons:
            image = _icon(owner, "{}OptionIcon{}".format(key, index), icons[option], 15, "text")
            image_slot = tile.add_child(image)
            image_slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)
            parts.append(image)
        name_text = _text(owner, "{}OptionLabel{}".format(key, index), option, True, _color("text"), 7)
        text_slot = tile.add_child(name_text)
        text_slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)
        parts.append(name_text)
        _center(button.add_child(tile))
        button.on_clicked.add_callable(_keep(state, _choose_callback(choice, option)))
        choice.tiles.append((option, button, parts))
        _add(row, button, _margin(0 if index == 0 else 3, 0, 0, 0), fill=True)
    _add(panel, row, _margin(0, 2, 0, 2))
    state["names"][key] = choice
    choice.refresh()
    return choice


def _keep(state, callback):
    """Unreal holds Python callbacks weakly: keep each one alive with its panel."""
    state["callbacks"].append(callback)
    return callback


def _palette_preview(name):
    """Four colours that tell a palette's story: canopy, meadow, ground, sky."""
    colors = _palette({"palette": name, "theme": "Summer", "ground_tint": 0.32})
    return [colors[key] for key in ("leaf", "grass", "ground_mound", "flower_b")]


def _palette_row(owner, panel, key, default):
    """One row of small swatches; each shows its palette's four colours."""
    state = _panel(owner)
    choice = _Choice(PALETTE_NAMES, default)
    row = _new(unreal.HorizontalBox, owner, key + "Row")
    for index, option in enumerate(PALETTE_NAMES):
        button = _new(unreal.Button, owner, "{}Swatch{}".format(key, index))
        button.set_tool_tip_text(option)
        quad = _new(unreal.VerticalBox, owner, "{}Quad{}".format(key, index))
        colours = _palette_preview(option)
        for half in range(2):
            pair = _new(unreal.HorizontalBox, owner, "{}Pair{}_{}".format(key, index, half))
            for side in range(2):
                chip = _new(unreal.Border, owner, "{}Chip{}_{}_{}".format(key, index, half, side))
                chip.set_brush(_rounded_brush(colours[half * 2 + side], 2.0))
                _add(pair, _size_box(owner, "{}ChipBox{}_{}_{}".format(key, index, half, side), chip, 10.0, 10.0), _margin(1, 1, 1, 1))
            quad.add_child(pair)
        _center(button.add_child(quad))
        button.on_clicked.add_callable(_keep(state, _choose_callback(choice, option)))
        choice.tiles.append((option, button, "swatch"))
        _add(row, button, _margin(0 if index == 0 else 3, 0, 0, 0), fill=True)
    _add(panel, row, _margin(0, 2, 0, 0))
    state["names"][key] = choice
    choice.refresh()
    return choice


class _Toggle:
    """An On / Off pill that answers like a CheckBox (is_checked / set_is_checked)."""

    def __init__(self, button, label, image, checked):
        self.button, self.label, self.image, self.checked = button, label, image, checked
        self.refresh()

    def is_checked(self):
        return self.checked

    def set_is_checked(self, checked):
        self.checked = bool(checked)
        self.refresh()

    def toggle(self):
        self.set_is_checked(not self.checked)

    def refresh(self):
        _style_button(self.button, "sage" if self.checked else "raised", "hover", "pressed", 10.0, padding=(4, 2))
        self.label.set_text("On" if self.checked else "Off")


def _toggle_callback(toggle):
    def callback():
        toggle.toggle()
    return callback


def _toggle_row(owner, panel, key, label, icon, checked=True):
    state = _panel(owner)
    row = _new(unreal.HorizontalBox, owner, key + "Row")
    image = _icon(owner, key + "Icon", icon, 12, "muted")
    image_slot = row.add_child(image)
    image_slot.set_padding(_margin(0, 0, 6, 0))
    image_slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
    caption = _text(owner, key + "Label", label, False, _color("text"), 9)
    caption_slot = row.add_child(caption)
    caption_slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
    caption_slot.set_size(_fill_size())
    button = _new(unreal.Button, owner, key + "Button")
    text = _text(owner, key + "State", "On", True, _color("text"), 8)
    _center(button.add_child(text))
    _add(row, _size_box(owner, key + "SizeBox", button, width=44.0, height=20.0), _margin(0, 1, 0, 1))
    toggle = _Toggle(button, text, image, checked)
    button.on_clicked.add_callable(_keep(state, _toggle_callback(toggle)))
    state["names"][key] = toggle
    _add(panel, row, _margin(0, 2, 0, 0))
    return toggle


def _fill_size():
    size = unreal.SlateChildSize()
    size.set_editor_property("size_rule", unreal.SlateSizeRule.FILL)
    size.set_editor_property("value", 1.0)
    return size


def _notice(owner, parent, name, text, tone="muted"):
    """A quiet one-line hint in small muted text."""
    hint = _text(owner, name + "Text", text, False, _color("muted"), 8, wrap=True)
    _add(parent, hint, _margin(0, 3, 0, 0))
    _panel(owner)["names"][name] = hint
    return hint


def _vegetation_counts(vegetation, terrain, cover=50.0):
    """One Vegetation slider drives every layer in natural proportions.

    Counts scale with the world's area, so a larger world keeps the same
    density.  Ground cover (grass, pebbles, low plants) has a floor that does
    not depend on the slider: even a sparse world is never bare.  Caps keep
    generation responsive.
    """
    area = (terrain / 12000.0) ** 2
    lush = vegetation / 50.0
    woods = 0.4 + 1.2 * cover / 100.0
    return {
        "trees": min(420, round(70 * lush * area * woods)),
        "shrubs": min(180, round((6 + 20 * lush) * area)),
        "details": min(160, round((4 + 22 * lush) * area)),
        # Stone is geology, not growth: it follows the area more than the slider.
        "rocks": min(160, round(30 * area * (0.6 + vegetation / 125.0))),
        "pebbles": min(3600, round(520 * area)),
        "grass": min(7000, round(1500 * area * (0.5 + vegetation / 100.0))),
        "plants": min(2200, round(320 * area * (0.4 + vegetation / 80.0))),
        "flowers": min(2600, round(area * (140 + 220 * lush))),
    }


def _summary(widget):
    """Turn the Vegetation slider into a quick, useful build estimate."""
    try:
        terrain = round(_find(widget, "TerrainSlider").get_value())
        counts = _vegetation_counts(_find(widget, "VegetationSlider").get_value(), terrain, _find(widget, "ForestCoverSlider").get_value())
        total = counts["trees"] + counts["shrubs"] + counts["rocks"]
        terrain_label = "compact" if terrain <= 4500 else "wide" if terrain >= 10500 else "balanced"
        density_label = "open" if counts["trees"] < 15 else "light" if total < 120 else "balanced" if total < 300 else "lush"
        message = "{} scene | {} trees, {} shrubs, {} rocks | ground cover: {} grass, {} flowers, {} pebbles | {} scale".format(
            density_label.title(), counts["trees"], counts["shrubs"], counts["rocks"], counts["grass"] + counts["plants"],
            counts["flowers"], counts["pebbles"], terrain_label)
        _find(widget, "BuildSummaryText").set_text(message)
    except Exception:
        # The UI can be rebuilding when this is called; the next slider change
        # will refresh the label normally.
        pass


def _clean_host_graph(asset):
    # The duplicated utility supplies a durable UMG root. Its original logic is not needed.
    try:
        unreal.BlueprintEditorLibrary.reparent_blueprint(asset, unreal.EditorUtilityWidget)
        for graph in unreal.BlueprintEditorLibrary.list_graphs(asset):
            unreal.BlueprintEditorLibrary.remove_graph(asset, graph)
        unreal.BlueprintEditorLibrary.compile_blueprint(asset)
        unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
    except Exception as error:
        unreal.log_warning("World Builder host graph cleanup skipped: {}".format(error))


def build_widget_asset():
    if unreal.EditorAssetLibrary.does_asset_exist(ASSET_PATH):
        asset = unreal.load_asset(ASSET_PATH)
        _clean_host_graph(asset)
        return asset
    if not unreal.load_asset(SOURCE_EUW_PATH):
        raise RuntimeError("Landmass must be enabled to create the World Builder panel")
    if not unreal.EditorAssetLibrary.duplicate_asset(SOURCE_EUW_PATH, ASSET_PATH):
        raise RuntimeError("Could not create the World Builder Editor Utility Widget")
    asset = unreal.load_asset(ASSET_PATH)
    _clean_host_graph(asset)
    return asset


def _find(widget, name):
    item = _panel(widget)["names"].get(name) or widget.find_child_widget_by_name(name)
    if not item:
        raise RuntimeError("World Builder UI element is missing: " + name)
    return item


def _build_runtime_ui(widget, force=False):
    global _ui_generation
    state = _panel(widget)
    if state["built"] and not force:
        return
    _ensure_icons()
    _ui_generation += 1
    # The old controls go away with their callbacks; other panels keep theirs.
    state.update({"names": {}, "callbacks": [], "built": False, "bound": False, "icons": []})
    host = _find(widget, HOST_PANEL)
    host.clear_children()
    # This asset started as the Landmass erosion utility. Hide every remaining
    # Erosion control so it reads as one deliberate World Builder tool.
    for name in (
        "HorizontalBox_PresetButtons", "HorizontalBox_MoveDelete", "HorizontalBox_ErosionKeys",
        "Button_AddErosionKey", "Button_Simulate", "Button_Reset", "Text_Iterations", "TextBlock_13",
        "Button_LoadSettings", "Button_SaveToDataAsset", "Button_NewDataAsset",
        "Button_DataAssetCategory", "Button_Delete", "Button_SwapBack", "Button_SwapFwd",
        "HorizontalBox_34", "Slider_ErosionTimeline",
    ):
        old = widget.find_child_widget_by_name(name)
        if old:
            old.set_visibility(unreal.SlateVisibility.COLLAPSED)

    # One warm background behind a fixed, narrow tool column, so the world
    # stays visible beside it.  Every tab fits without scrolling.
    root = _new(unreal.Border, widget, "IWBRoot")
    root.set_brush(_rounded_brush(_color("bg"), 0.0))
    root.set_padding(_margin(0, 0, 0, 6))
    root.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_LEFT)
    panel = _new(unreal.VerticalBox, widget, "IWBPanel")
    root.add_child(_size_box(widget, "IWBColumn", panel, width=PANEL_WIDTH))
    _add(host, root, _margin(0, 0, 0, 0))

    header = _new(unreal.HorizontalBox, widget, "IWBHeader")
    logo_slot = header.add_child(_icon(widget, "IWBLogo", "world", 20, "accent"))
    logo_slot.set_padding(_margin(0, 0, 7, 0))
    logo_slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
    title = _text(widget, "IWBTitle", "World Builder", True, _color("text"), 14)
    title.set_tool_tip_text("Grow a quiet, playable forest from a seed.")
    title_slot = header.add_child(title)
    title_slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
    _add(panel, header, _margin(10, 8, 10, 6))

    tabs = _new(unreal.HorizontalBox, widget, "IWBTabBar")
    for index, (name, label, icon) in enumerate((
        ("WorldTabButton", "World", "world"), ("NatureTabButton", "Nature", "leaf"),
        ("MoodTabButton", "Mood", "golden_hour"), ("BuildTabButton", "Build", "build"),
    )):
        _add(tabs, _button(widget, name, label, "soft", 9, icon), _margin(0 if index == 0 else 3, 0, 0, 0), fill=True)
    _add(panel, tabs, _margin(8, 0, 8, 4))
    switcher = _new(unreal.WidgetSwitcher, widget, "IWBTabSwitcher")
    pages = []
    for name in ("IWBWorldTab", "IWBNatureTab", "IWBMoodTab", "IWBBuildTab"):
        page = _new(unreal.VerticalBox, widget, name)
        switcher.add_child(page)
        pages.append(page)
    world_tab, nature_tab, mood_tab, build_tab = pages
    switcher.set_active_widget_index(0)
    _add(panel, switcher, _margin(0, 0, 0, 0))

    def slider(section, key):
        _key, _value_key, label, default, minimum, maximum, decimals = SLIDER_SPEC[key]
        _slider_row(widget, section, key, label, default, minimum, maximum, decimals)

    # WORLD
    world = _section(widget, world_tab, "World", "Your world", "Same seed and settings give the same world.", "world")
    _field_row(widget, world, "WorldName", "Name", "Whispering Pines")
    seed_button = _button(widget, "NewSeedButton", "New", "soft", 8, "seed")
    seed_button.set_tool_tip_text("Pick a new random seed")
    _field_row(widget, world, "Seed", "Seed", "20261018", seed_button)
    _choice_row(widget, world, "ThemeBox", "Season", SEASONS, "Summer", icons=SEASON_ICONS)
    _notice(widget, world, "ThemeHint", "The season picks the tree species.")

    colors = _section(widget, world_tab, "Colors", "Colors", "Click a palette: the world is repainted at once.", "palette")
    _palette_row(widget, colors, "PaletteBox", DEFAULT_PALETTE)
    _notice(widget, colors, "PaletteHint", DEFAULT_PALETTE + ": " + _palette_description(DEFAULT_PALETTE) + ".")

    land = _section(widget, world_tab, "Land", "Land & paths", "Size, hills, and the trails through the forest.", "mountains")
    slider(land, "Terrain")
    slider(land, "Hills")
    _choice_row(widget, land, "PathsBox", "Paths", PATH_OPTIONS, "Network", icons=PATH_ICONS)
    _toggle_row(widget, land, "MountainsBox", "Mountains around the world", "mountains", True)

    # NATURE
    life = _section(widget, nature_tab, "Life", "Plants", "How much grows, and how much of the land is forest.", "vegetation")
    slider(life, "Vegetation")
    slider(life, "ForestCover")
    density_tools = _new(unreal.HorizontalBox, widget, "DensityTools")
    for index, (name, label) in enumerate((("CompactProfileButton", "Compact"), ("BalancedProfileButton", "Balanced"), ("LushProfileButton", "Lush"))):
        _add(density_tools, _button(widget, name, label, "soft", 8), _margin(0 if index == 0 else 3, 3, 0, 0), fill=True)
    _add(life, density_tools)
    estimate = _new(unreal.Border, widget, "BuildEstimate")
    estimate.set_brush(_rounded_brush(_color("raised"), 6.0))
    estimate.set_padding(_margin(7, 4, 7, 4))
    estimate.add_child(_text(widget, "BuildSummaryText", "Balanced scene", False, _color("text"), 8, wrap=True))
    _add(life, estimate, _margin(0, 5, 0, 0))
    wildlife = _section(widget, nature_tab, "Wildlife", "Wildlife", "Animals to feed, birds, butterflies, dragonflies.", "wildlife")
    slider(wildlife, "Wildlife")

    # MOOD
    light = _section(widget, mood_tab, "Light", "Light", "Changes relight the world as soon as you let go.", "sun")
    _choice_row(widget, light, "TimeBox", "Time", tuple(TIMES_OF_DAY), DEFAULT_TIME_OF_DAY, icons=TIME_ICONS)
    slider(light, "Lighting")
    slider(light, "LightTint")
    slider(light, "SkyTint")
    slider(light, "GroundTint")
    air = _section(widget, mood_tab, "Air", "Air & weather", "Haze softens the distance; sparkles drift around you.", "haze")
    slider(air, "Haze")
    slider(air, "Sparkles")
    _choice_row(widget, air, "WeatherBox", "Weather", WEATHER_OPTIONS, "Clear", icons=WEATHER_ICONS)
    _toggle_row(widget, air, "PlayableCharacterBox", "Playable character", "character", True)

    # BUILD
    actions = _section(widget, build_tab, "Actions", "Build",
                       "Build World makes the whole world from every tab. New plants keeps the ground and regrows the life.", "build")
    build_button = _button(widget, "GenerateButton", "Build World", "primary", 11, "build")
    _add(actions, _size_box(widget, "GenerateSize", build_button, height=32.0), _margin(0, 0, 0, 4))
    for row_name, buttons in (
        ("PrimaryButtons", (("RefreshPlantsButton", "New plants", "refresh",
                             "Same ground, a new layout of trees, plants, rocks, mushrooms and animals"),)),
    ):
        line = _new(unreal.HorizontalBox, widget, row_name)
        for index, (name, label, icon, tip) in enumerate(buttons):
            button = _button(widget, name, label, "soft", 8, icon)
            button.set_tool_tip_text(tip)
            _add(line, button, _margin(0 if index == 0 else 3, 0, 0, 3), fill=True)
        _add(actions, line)
    presets_card = _section(widget, build_tab, "Presets", "Presets",
                            "Save keeps every setting; Load brings them back; Reset puts the defaults back (the world is untouched).", "save")
    presets = _new(unreal.HorizontalBox, widget, "PresetButtons")
    for index, (name, label, icon) in enumerate((("SavePresetButton", "Save", "save"), ("LoadPresetButton", "Load", "load"), ("ResetButton", "Reset", "reset"))):
        _add(presets, _button(widget, name, label, "soft", 8, icon), _margin(0 if index == 0 else 3, 0, 0, 0), fill=True)
    _add(presets_card, presets)
    cleanup = _section(widget, build_tab, "Cleanup", "Start again", "Remove the generated world and bring back the empty level.", "delete")
    danger = _new(unreal.HorizontalBox, widget, "DangerButtons")
    _add(danger, _button(widget, "DeleteButton", "Delete world", "danger", 8, "delete"), _margin(0, 0, 0, 0), fill=True)
    _add(cleanup, danger)

    # Status, always visible under the tabs.
    status = _new(unreal.Border, widget, "StatusCard")
    status.set_brush(_rounded_brush(_color("card"), 6.0, _color("line"), 1.0))
    status.set_padding(_margin(8, 4, 8, 4))
    status_row = _new(unreal.HorizontalBox, widget, "StatusRow")
    status_icon = status_row.add_child(_icon(widget, "StatusIcon", "sparkles", 10, "accent"))
    status_icon.set_padding(_margin(0, 0, 6, 0))
    status_icon.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
    _add(status_row, _text(widget, "StatusText", "Ready to grow a forest.", False, _color("text"), 8, wrap=True), _margin(0, 0, 0, 0), fill=True)
    status.add_child(status_row)
    _add(panel, status, _margin(8, 4, 8, 0))
    state["built"] = True
    _resize_icons(widget)
    _make_tab_callback(widget, 0)()
    _summary(widget)
    widget.invalidate_layout_and_volatility()
    widget.force_layout_prepass()


def _status(widget, message):
    _find(widget, "StatusText").set_text(message)
    unreal.log("Interactive World Builder: " + message)


def _refresh_readouts(widget):
    for key, _value_key, _label, _default, _minimum, _maximum, decimals in SLIDERS:
        value = _find(widget, key + "Slider").get_value()
        _find(widget, key + "Readout").set_text(f"{value:.2f}" if decimals else str(round(value)))
    _summary(widget)


def _values(widget):
    _refresh_readouts(widget)
    try:
        seed = int(str(_find(widget, "SeedBox").get_text()).strip())
    except ValueError:
        seed = 20261018
    data = {
        "world_name": str(_find(widget, "WorldNameBox").get_text()).strip() or "Untitled Forest",
        "seed": seed, "theme": str(_find(widget, "ThemeBox").get_selected_option()),
        "palette": str(_find(widget, "PaletteBox").get_selected_option()),
        "paths": str(_find(widget, "PathsBox").get_selected_option()),
        "mountains": _find(widget, "MountainsBox").is_checked(),
        "playable_character": _find(widget, "PlayableCharacterBox").is_checked(),
        "weather": str(_find(widget, "WeatherBox").get_selected_option()),
        "time_of_day": str(_find(widget, "TimeBox").get_selected_option()),
    }
    for key, value_key, _label, _default, _minimum, _maximum, decimals in SLIDERS:
        value = _find(widget, key + "Slider").get_value()
        data[value_key] = value if decimals or key == "Terrain" else round(value)
    data.update(_vegetation_counts(data["vegetation"], data["terrain"], data["forest_cover"]))
    return data


def new_seed(widget):
    """Choose a legible random seed without changing the current art direction."""
    seed = random.SystemRandom().randint(10000000, 99999999)
    _find(widget, "SeedBox").set_text(str(seed))
    _status(widget, "New seed {} is ready. Build World to grow a different layout.".format(seed))


def apply_density_profile(widget, profile):
    """Give new users useful starting points, while preserving their name, seed, and mood."""
    profiles = {
        "compact": {"Terrain": 6000.0, "Vegetation": 40.0},
        "balanced": {"Terrain": 14000.0, "Vegetation": 50.0},
        "lush": {"Terrain": 20000.0, "Vegetation": 80.0},
    }
    values = profiles[profile]
    for key, value in values.items():
        _find(widget, key + "Slider").set_value(value)
    _refresh_readouts(widget)
    _status(widget, "{} profile applied. Your seed and color choices are unchanged.".format(profile.title()))


def compact_profile(widget):
    apply_density_profile(widget, "compact")


def balanced_profile(widget):
    apply_density_profile(widget, "balanced")


def lush_profile(widget):
    apply_density_profile(widget, "lush")


def _preset_path():
    return os.path.join(unreal.Paths.project_saved_dir(), PRESET_FILENAME)


def _has_tag(actor, tag):
    return tag in [str(value) for value in actor.get_editor_property("tags")]


def _clear_world(preview_only=False, layers=None):
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        remove = preview_only and _has_tag(actor, PREVIEW_TAG)
        if not preview_only and _has_tag(actor, GENERATED_TAG):
            remove = layers is None or any(_has_tag(actor, layer) for layer in layers)
        if remove:
            unreal.EditorLevelLibrary.destroy_actor(actor)


def _set_template_visible(actor, visible):
    """Hide or show template geometry in a way that survives saving and reopening.

    "Temporarily hidden in editor" is not saved, so the template floor and
    walls used to come back over the generated world after a restart, and
    their collision stayed active as invisible walls in Play.
    """
    for component in actor.get_components_by_class(unreal.PrimitiveComponent):
        component.set_visibility(visible, True)
    actor.set_actor_hidden_in_game(not visible)
    actor.set_actor_enable_collision(visible)
    actor.set_is_temporarily_hidden_in_editor(not visible)


def _is_sky(actor):
    return "sky" in actor.get_actor_label().lower()


def _hide_template_environment():
    """Hide the Third Person template geometry while keeping its sky, light, and player start."""
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if any(_has_tag(actor, tag) for tag in (GENERATED_TAG, PREVIEW_TAG)) or not isinstance(actor, unreal.StaticMeshActor):
            continue
        if _is_sky(actor):
            # Older versions hid the sky sphere too; give the sky back.
            if _has_tag(actor, TEMPLATE_HIDDEN_TAG):
                actor.set_editor_property("tags", [tag for tag in actor.get_editor_property("tags") if str(tag) != TEMPLATE_HIDDEN_TAG])
                _set_template_visible(actor, True)
            continue
        if not _has_tag(actor, TEMPLATE_HIDDEN_TAG):
            tags = list(actor.get_editor_property("tags"))
            tags.append(unreal.Name(TEMPLATE_HIDDEN_TAG))
            actor.set_editor_property("tags", tags)
        _set_template_visible(actor, False)


def _restore_template_environment():
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if _has_tag(actor, TEMPLATE_HIDDEN_TAG):
            actor.set_editor_property("tags", [tag for tag in actor.get_editor_property("tags") if str(tag) != TEMPLATE_HIDDEN_TAG])
            _set_template_visible(actor, True)


def _tag(actor, generated_tag, layer, label, paint=None):
    """paint = (main color key, optional trunk key, optional stem key)."""
    tags = [unreal.Name(generated_tag), unreal.Name(layer)]
    # Paint tags remember which palette colors an actor wears, so a palette
    # change can recolor the world in place instead of rebuilding it.
    if paint:
        tags.append(unreal.Name(PAINT_PREFIX + paint[0]))
        if len(paint) > 1 and paint[1]:
            tags.append(unreal.Name(TRUNK_PREFIX + paint[1]))
        if len(paint) > 2 and paint[2]:
            tags.append(unreal.Name(STEM_PREFIX + paint[2]))
    actor.set_editor_property("tags", tags)
    actor.set_actor_label(label)
    return actor


def _tag_value(actor, prefix):
    for tag in actor.get_editor_property("tags"):
        text = str(tag)
        if text.startswith(prefix):
            return text[len(prefix):]
    return None


MATERIAL_VERSION = "weather-1"
WEATHER_COLLECTION_PATH = "/Game/WorldBuilder/Materials/MPC_IWB_Weather"


def _weather_collection():
    """Two world-wide values every surface reads: Snow (white tops) and Wet (dark, glossy)."""
    collection = unreal.load_asset(WEATHER_COLLECTION_PATH)
    if collection:
        return collection
    collection = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "MPC_IWB_Weather", "/Game/WorldBuilder/Materials", unreal.MaterialParameterCollection,
        unreal.MaterialParameterCollectionFactoryNew())
    params = []
    for name in ("Snow", "Wet"):
        param = unreal.CollectionScalarParameter()
        param.set_editor_property("parameter_name", name)
        param.set_editor_property("default_value", 0.0)
        params.append(param)
    collection.set_editor_property("scalar_parameters", params)
    unreal.EditorAssetLibrary.save_loaded_asset(collection, False)
    return collection


def _set_weather_values(snow, wet):
    """Saved in the asset, so the editor and Play mode both see the weather."""
    collection = _weather_collection()
    params = list(collection.get_editor_property("scalar_parameters"))
    for param in params:
        name = str(param.get_editor_property("parameter_name"))
        param.set_editor_property("default_value", snow if name == "Snow" else wet)
    collection.set_editor_property("scalar_parameters", params)
    unreal.EditorAssetLibrary.save_loaded_asset(collection, False)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if world:
        for name, value in (("Snow", snow), ("Wet", wet)):
            try:
                unreal.KismetMaterialLibrary.set_scalar_parameter_value(world, collection, name, value)
            except Exception:
                pass


def _weather_shading(material, base, base_output=""):
    """Snow settles on every upward face; rain darkens surfaces and makes them shine."""
    lib = unreal.MaterialEditingLibrary
    collection = _weather_collection()

    def node(cls, x, y, **props):
        item = lib.create_material_expression(material, cls, x, y)
        for key, value in props.items():
            item.set_editor_property(key, value)
        return item

    snow = node(unreal.MaterialExpressionCollectionParameter, -700, 420, collection=collection, parameter_name="Snow")
    wet = node(unreal.MaterialExpressionCollectionParameter, -700, 560, collection=collection, parameter_name="Wet")
    normal = node(unreal.MaterialExpressionVertexNormalWS, -900, 300)
    up = node(unreal.MaterialExpressionComponentMask, -760, 300, r=False, g=False, b=True, a=False)
    lib.connect_material_expressions(normal, "", up, "")
    lifted = node(unreal.MaterialExpressionSubtract, -620, 300, const_b=0.3)
    lib.connect_material_expressions(up, "", lifted, "A")
    steep = node(unreal.MaterialExpressionMultiply, -500, 300, const_b=2.6)
    lib.connect_material_expressions(lifted, "", steep, "A")
    clamp = node(unreal.MaterialExpressionSaturate, -380, 300)
    lib.connect_material_expressions(steep, "", clamp, "")
    snow_mask = node(unreal.MaterialExpressionMultiply, -260, 340)
    lib.connect_material_expressions(clamp, "", snow_mask, "A")
    lib.connect_material_expressions(snow, "", snow_mask, "B")
    soak = node(unreal.MaterialExpressionMultiply, -500, 560, const_b=0.5)
    lib.connect_material_expressions(wet, "", soak, "A")
    dry = node(unreal.MaterialExpressionOneMinus, -380, 560)
    lib.connect_material_expressions(soak, "", dry, "")
    darkened = node(unreal.MaterialExpressionMultiply, -260, 160)
    lib.connect_material_expressions(base, base_output, darkened, "A")
    lib.connect_material_expressions(dry, "", darkened, "B")
    white = node(unreal.MaterialExpressionConstant3Vector, -260, 480, constant=unreal.LinearColor(0.88, 0.92, 1.0, 1.0))
    colour = node(unreal.MaterialExpressionLinearInterpolate, -100, 200)
    lib.connect_material_expressions(darkened, "", colour, "A")
    lib.connect_material_expressions(white, "", colour, "B")
    lib.connect_material_expressions(snow_mask, "", colour, "Alpha")
    gloss = node(unreal.MaterialExpressionLinearInterpolate, -260, 640, const_a=0.86, const_b=0.07)
    lib.connect_material_expressions(wet, "", gloss, "Alpha")
    rough = node(unreal.MaterialExpressionLinearInterpolate, -100, 560, const_b=0.75)
    lib.connect_material_expressions(gloss, "", rough, "A")
    lib.connect_material_expressions(snow_mask, "", rough, "Alpha")
    specular = node(unreal.MaterialExpressionLinearInterpolate, -100, 700, const_a=0.4, const_b=0.9)
    lib.connect_material_expressions(wet, "", specular, "Alpha")
    lib.connect_material_property(colour, "", unreal.MaterialProperty.MP_BASE_COLOR)
    lib.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    lib.connect_material_property(specular, "", unreal.MaterialProperty.MP_SPECULAR)


def _material_is_current(material):
    return unreal.EditorAssetLibrary.get_metadata_tag(material, "IWBVersion") == MATERIAL_VERSION


def _finish_material(material):
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.set_metadata_tag(material, "IWBVersion", MATERIAL_VERSION)
    unreal.EditorAssetLibrary.save_loaded_asset(material, False)


def _get_tint_material():
    """The shared colour material of every prop, with snow and rain built in."""
    global _tint_material
    if _tint_material:
        return _tint_material
    material_path = "/Game/WorldBuilder/Materials/M_IWB_Tint"
    material = unreal.load_asset(material_path)
    if not material:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "M_IWB_Tint", "/Game/WorldBuilder/Materials", unreal.Material, unreal.MaterialFactoryNew())
        if not material:
            raise RuntimeError("Could not create the World Builder tint material")
    if not _material_is_current(material):
        # Rebuilt in place: every painted actor keeps its colour and gains the weather.
        unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
        tint = unreal.MaterialEditingLibrary.create_material_expression(
            material, unreal.MaterialExpressionVectorParameter, -900, 0)
        tint.set_editor_property("parameter_name", "Tint")
        tint.set_editor_property("default_value", unreal.LinearColor(0.12, 0.42, 0.12, 1.0))
        _weather_shading(material, tint)
        _finish_material(material)
    _tint_material = material
    return material


def _get_weather_material():
    """A tiny world-position-offset material makes simple rain/snow meshes fall in Play."""
    global _weather_material
    if _weather_material:
        return _weather_material
    material_path = "/Game/WorldBuilder/Materials/M_IWB_Weather"
    material = unreal.load_asset(material_path)
    if material:
        _weather_material = material
        return material
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    material = tools.create_asset(
        "M_IWB_Weather",
        "/Game/WorldBuilder/Materials",
        unreal.Material,
        unreal.MaterialFactoryNew(),
    )
    if not material:
        raise RuntimeError("Could not create the World Builder weather material")
    tint = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionVectorParameter, -520, -80
    )
    tint.set_editor_property("parameter_name", "Tint")
    tint.set_editor_property("default_value", unreal.LinearColor(0.72, 0.86, 1.0, 1.0))
    roughness = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant, -520, 80
    )
    roughness.set_editor_property("r", 0.15)
    time = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTime, -520, 230
    )
    speed = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionScalarParameter, -330, 230
    )
    speed.set_editor_property("parameter_name", "FallSpeed")
    speed.set_editor_property("default_value", 0.45)
    time_speed = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -130, 230
    )
    frac = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionFrac, 40, 230
    )
    distance = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionScalarParameter, -130, 350
    )
    distance.set_editor_property("parameter_name", "FallDistance")
    distance.set_editor_property("default_value", -3600.0)
    fall = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, 220, 230
    )
    zero_x = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant, 40, 370
    )
    zero_y = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant, 40, 430
    )
    zero_x.set_editor_property("r", 0.0)
    zero_y.set_editor_property("r", 0.0)
    xy = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionAppendVector, 220, 390
    )
    offset = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionAppendVector, 400, 300
    )
    unreal.MaterialEditingLibrary.connect_material_property(tint, "", unreal.MaterialProperty.MP_BASE_COLOR)
    unreal.MaterialEditingLibrary.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    unreal.MaterialEditingLibrary.connect_material_expressions(time, "", time_speed, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(speed, "", time_speed, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(time_speed, "", frac, "")
    unreal.MaterialEditingLibrary.connect_material_expressions(frac, "", fall, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(distance, "", fall, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(zero_x, "", xy, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(zero_y, "", xy, "B")
    unreal.MaterialEditingLibrary.connect_material_expressions(xy, "", offset, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(fall, "", offset, "B")
    unreal.MaterialEditingLibrary.connect_material_property(offset, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    _weather_material = material
    return material


def _source_slot_name(component, index):
    """Read the mesh's own material name; the slot itself may already be our tint."""
    try:
        source = component.static_mesh.get_material(index) if component.static_mesh else None
        return source.get_name().lower() if source else ""
    except Exception:
        return ""


def _apply_mesh_tint(actor, tint, trunk_tint=None, stem_tint=None):
    """Tint every slot, while preserving a separate bark/trunk color on trees.

    Kenney trees use leaf and wood material slots.  Replacing only slot zero
    made the imported foliage inconsistent and could never produce the ivory
    trunk / orange canopy contrast of the artistic palette.  Calling this again
    on a tinted actor reuses its dynamic materials (used by palette recolor).
    stem_tint colors the Kenney "grass" slot: flower stems and rock moss.
    """
    if not tint:
        return
    for component in actor.get_components_by_class(unreal.StaticMeshComponent):
        if component.static_mesh:
            _tint_component(actor, component, tint, trunk_tint, stem_tint)


def _tint_component(actor, component, tint, trunk_tint, stem_tint):
    try:
        slot_count = max(1, component.get_num_materials())
    except Exception:
        slot_count = 1
    for index in range(slot_count):
        slot_name = _source_slot_name(component, index)
        color = tint
        if trunk_tint and any(token in slot_name for token in ("bark", "wood", "trunk", "stump", "log")):
            color = trunk_tint
        elif stem_tint and "grass" in slot_name:
            color = stem_tint
        try:
            material = component.get_material(index)
            if not isinstance(material, unreal.MaterialInstanceDynamic):
                component.set_material(index, _get_tint_material())
                material = component.create_dynamic_material_instance(index)
            if material:
                material.set_vector_parameter_value("Tint", color)
        except Exception as error:
            unreal.log_warning("World Builder tint skipped on {} slot {}: {}".format(actor.get_name(), index, error))


def _paint(palette, key, trunk=None, stem=None):
    """Spawn keyword arguments for one palette role, remembered as paint tags."""
    return {
        "tint": palette[key],
        "trunk_tint": palette[trunk] if trunk else None,
        "stem_tint": palette[stem] if stem else None,
        "paint": (key, trunk, stem),
    }


def _spawn_mesh(mesh, location, scale, rotation, generated_tag, layer, label, tint=None, trunk_tint=None, paint=None, stem_tint=None):
    actor = unreal.EditorLevelLibrary.spawn_actor_from_object(mesh, location, rotation)
    actor.set_actor_scale3d(scale)
    _tag(actor, generated_tag, layer, label, paint)
    _apply_mesh_tint(actor, tint, trunk_tint, stem_tint)
    return actor


def _enable_world_collision(actor):
    """Generated terrain must be a solid walking surface in Play mode."""
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    if not component:
        return
    try:
        component.set_collision_profile_name("BlockAll")
        component.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS)
        component.set_collision_object_type(unreal.CollisionChannel.ECC_WORLD_STATIC)
        component.set_collision_response_to_all_channels(unreal.CollisionResponseType.ECR_BLOCK)
        actor.set_actor_enable_collision(True)
    except Exception as error:
        unreal.log_warning("World Builder collision setup skipped for {}: {}".format(actor.get_name(), error))
    # UE 5.8 does not expose this optional component method in Python on every
    # BasicShape component.  Collision is already configured above, so simply
    # skip the cosmetic overlap setting when the binding is unavailable.
    try:
        component.set_generate_overlap_events(False)
    except AttributeError:
        pass


def _kenney_mesh(name):
    """Load one of the small, intentionally chosen imported Nature Kit assets."""
    return unreal.load_asset(KENNEY_DESTINATION + "/" + name)


def _kenney_meshes(names):
    # Some FBX files expose a material with the same name as their mesh.
    # Never pass such an asset to SpawnActor; only real StaticMesh assets belong
    # in the generator.
    return [mesh for mesh in (_kenney_mesh(name) for name in names) if isinstance(mesh, unreal.StaticMesh)]


def _kenney_zip_path():
    return os.path.join(os.path.expanduser("~"), "Downloads", KENNEY_ZIP_FILENAME)


def _kenney_import_marker_path():
    return os.path.join(unreal.Paths.project_saved_dir(), "WorldBuilderNature_ZUp_v3.ready")


def _kenney_is_imported():
    """Return true only for the complete, current Z-up Nature Kit import.

    Earlier versions trusted one tree and a marker, which allowed old Y-up
    meshes to survive alongside freshly imported ones.  A build now repairs
    the complete selection once when this version changes.
    """
    marker_path = _kenney_import_marker_path()
    if not os.path.isfile(marker_path):
        return False
    try:
        with open(marker_path, "r", encoding="utf-8") as marker:
            if marker.read().strip() != KENNEY_IMPORT_VERSION:
                return False
    except OSError:
        return False
    return all(
        unreal.EditorAssetLibrary.does_asset_exist(KENNEY_DESTINATION + "/" + name)
        for name in KENNEY_FILES
    )


def _import_kenney_assets(force=False):
    """Extract and import only the models the generator uses, from the user's zip.

    A normal build imports only the models that are missing; Repair Nature
    Kit (force) re-imports the whole selection.
    """
    if _kenney_is_imported() and not force:
        return True, "Nature Kit is already ready."
    names = KENNEY_FILES if force else [
        name for name in KENNEY_FILES
        if not unreal.EditorAssetLibrary.does_asset_exist(KENNEY_DESTINATION + "/" + name)
    ]
    archive_path = _kenney_zip_path()
    if not os.path.isfile(archive_path):
        return False, "Nature Kit zip not found in Downloads."
    source_dir = os.path.join(unreal.Paths.project_intermediate_dir(), "WorldBuilderNatureSource")
    os.makedirs(source_dir, exist_ok=True)
    try:
        with zipfile.ZipFile(archive_path, "r") as archive:
            for name in names:
                member = "Models/FBX format/{}.fbx".format(name)
                destination = os.path.join(source_dir, name + ".fbx")
                with archive.open(member) as source, open(destination, "wb") as target:
                    target.write(source.read())
    except (OSError, KeyError, zipfile.BadZipFile) as error:
        return False, "Could not unpack Nature Kit: {}".format(error)

    tasks = []
    for name in names:
        options = unreal.FbxImportUI()
        options.set_editor_property("import_mesh", True)
        options.set_editor_property("import_as_skeletal", False)
        options.set_editor_property("import_materials", True)
        options.set_editor_property("import_textures", True)
        # Kenney FBX files are Y-up.  Convert each source scene to Unreal's
        # Z-up scene *during* import; do not compensate with a per-instance
        # rotation, which would make bounds and ground snapping unreliable.
        static_data = options.get_editor_property("static_mesh_import_data")
        if static_data:
            for property_name, value in (
                ("convert_scene", True),
                ("force_front_x_axis", False),
                ("import_rotation", unreal.Rotator(0, 0, 0)),
                ("import_translation", unreal.Vector(0, 0, 0)),
                ("import_uniform_scale", 1.0),
            ):
                try:
                    static_data.set_editor_property(property_name, value)
                except Exception:
                    # Engine versions expose the axis conversion option on
                    # different FBX import structs.  Their normal FBX axis
                    # conversion remains enabled; never abort the whole kit.
                    pass
            # Imported cliffs must have actual collision geometry, not merely a
            # component collision profile.  That is what stops the player from
            # walking through a decorative FBX mountain in Play mode.
            try:
                static_data.set_editor_property("auto_generate_collision", True)
            except Exception:
                pass
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", os.path.join(source_dir, name + ".fbx"))
        task.set_editor_property("destination_path", KENNEY_DESTINATION)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        # Reapply the Z-up settings instead of retaining any settings from a
        # previous sideways import of the same asset.
        try:
            task.set_editor_property("replace_existing_settings", True)
        except Exception:
            pass
        task.set_editor_property("save", True)
        task.set_editor_property("options", options)
        tasks.append(task)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    unreal.EditorAssetLibrary.save_directory(KENNEY_DESTINATION, only_if_is_dirty=False, recursive=True)
    if _kenney_is_imported():
        return True, "Nature Kit imported: trees, flowers, grass, rocks and cliffs are ready."
    # An import runs synchronously here.  Write the orientation version only
    # after save so the next Build can trust its asset axes.
    if all(unreal.EditorAssetLibrary.does_asset_exist(KENNEY_DESTINATION + "/" + name) for name in KENNEY_FILES):
        try:
            with open(_kenney_import_marker_path(), "w", encoding="utf-8") as marker:
                marker.write(KENNEY_IMPORT_VERSION + "\n")
            return True, "Nature Kit imported: trees, flowers, grass, rocks and cliffs are ready."
        except OSError as error:
            return False, "Nature Kit imported but could not save its orientation marker: {}".format(error)
    return False, "Unreal did not finish importing the Nature Kit. Check the Output Log."


def import_nature_kit(widget):
    _status(widget, "Importing selected Nature Kit models...")
    success, message = _import_kenney_assets(force=True)
    _status(widget, message if success else "Import failed: " + message)


def _fantasy_town_zip_path():
    return os.path.join(os.path.expanduser("~"), "Downloads", FANTASY_TOWN_ZIP_FILENAME)


def _fantasy_town_import_marker_path():
    return os.path.join(unreal.Paths.project_saved_dir(), "WorldBuilderFantasyTown_v1.ready")


def _fantasy_town_meshes():
    """Return only the imported static meshes, never their materials or textures."""
    try:
        paths = unreal.EditorAssetLibrary.list_assets(
            FANTASY_TOWN_DESTINATION, recursive=False, include_folder=False
        )
    except Exception:
        return []
    return [
        mesh for mesh in (unreal.load_asset(path) for path in paths)
        if isinstance(mesh, unreal.StaticMesh)
    ]


def _fantasy_town_is_imported():
    marker_path = _fantasy_town_import_marker_path()
    try:
        with open(marker_path, "r", encoding="utf-8") as marker:
            return marker.read().strip() == FANTASY_TOWN_IMPORT_VERSION and bool(_fantasy_town_meshes())
    except OSError:
        return False


def _fantasy_town_members(archive):
    """Pick a compact, varied landmark selection from Kenney's archive."""
    fbx_members = [
        member for member in archive.namelist()
        if member.lower().endswith(".fbx") and "fbx" in member.lower()
    ]
    selected = []
    selected_names = set()
    for keyword in FANTASY_TOWN_KEYWORDS:
        matches = [member for member in fbx_members if keyword in os.path.basename(member).lower()]
        for member in matches[:2]:
            base_name = os.path.basename(member).lower()
            if base_name not in selected_names:
                selected.append(member)
                selected_names.add(base_name)
    return selected[:20]


def _import_fantasy_town_assets(force=False):
    """Import a small set of authored landmarks, not the full 160-model kit."""
    if _fantasy_town_is_imported() and not force:
        return True, "Fantasy Town landmarks are already ready."
    archive_path = _fantasy_town_zip_path()
    if not os.path.isfile(archive_path):
        return False, "Fantasy Town Kit zip not found in Downloads."
    source_dir = os.path.join(unreal.Paths.project_intermediate_dir(), "WorldBuilderFantasyTownSource")
    os.makedirs(source_dir, exist_ok=True)
    try:
        with zipfile.ZipFile(archive_path, "r") as archive:
            members = _fantasy_town_members(archive)
            if not members:
                return False, "No FBX landmarks were found in the Fantasy Town Kit archive."
            source_files = []
            for member in members:
                destination = os.path.join(source_dir, os.path.basename(member))
                with archive.open(member) as source, open(destination, "wb") as target:
                    target.write(source.read())
                source_files.append(destination)
    except (OSError, KeyError, zipfile.BadZipFile) as error:
        return False, "Could not unpack Fantasy Town Kit: {}".format(error)

    tasks = []
    for source_file in source_files:
        options = unreal.FbxImportUI()
        options.set_editor_property("import_mesh", True)
        options.set_editor_property("import_as_skeletal", False)
        options.set_editor_property("import_materials", True)
        options.set_editor_property("import_textures", True)
        static_data = options.get_editor_property("static_mesh_import_data")
        if static_data:
            for property_name, value in (
                ("convert_scene", True),
                ("force_front_x_axis", False),
                ("import_rotation", unreal.Rotator(0, 0, 0)),
                ("import_translation", unreal.Vector(0, 0, 0)),
                ("import_uniform_scale", 1.0),
                ("auto_generate_collision", True),
            ):
                try:
                    static_data.set_editor_property(property_name, value)
                except Exception:
                    pass
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", source_file)
        task.set_editor_property("destination_path", FANTASY_TOWN_DESTINATION)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        try:
            task.set_editor_property("replace_existing_settings", True)
        except Exception:
            pass
        task.set_editor_property("save", True)
        task.set_editor_property("options", options)
        tasks.append(task)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    unreal.EditorAssetLibrary.save_directory(FANTASY_TOWN_DESTINATION, only_if_is_dirty=False, recursive=True)
    imported_count = len(_fantasy_town_meshes())
    if not imported_count:
        return False, "Unreal did not finish importing the Fantasy Town Kit. Check the Output Log."
    try:
        with open(_fantasy_town_import_marker_path(), "w", encoding="utf-8") as marker:
            marker.write(FANTASY_TOWN_IMPORT_VERSION + "\n")
    except OSError as error:
        return False, "Town Kit imported but could not save its marker: {}".format(error)
    return True, "Fantasy Town Kit imported: {} hilltop landmarks are ready.".format(imported_count)


def import_town_kit(widget):
    _status(widget, "Importing selected Fantasy Town landmarks...")
    success, message = _import_fantasy_town_assets(force=True)
    _status(widget, message if success else "Import failed: " + message)


def _pcg_tree_meshes():
    """Use the real, bundled PCG sample tree models before falling back to primitives."""
    return [mesh for mesh in (
        unreal.load_asset("/PCG/SampleContent/SimpleForest/Meshes/PCG_Tree_01"),
        unreal.load_asset("/PCG/SampleContent/SimpleForest/Meshes/PCG_Tree_02"),
        unreal.load_asset("/PCG/SampleContent/SimpleForest/Meshes/PCG_Tree_03"),
    ) if mesh]


def _snap_actor_to_ground(actor, ground_z, penetration=0.0):
    """Use the spawned actor's real, rotated bounds instead of trusting FBX pivots."""
    try:
        bounds_origin, bounds_extent = actor.get_actor_bounds(False)
        current = actor.get_actor_location()
        actor.set_actor_location(
            unreal.Vector(current.x, current.y, current.z + ground_z - penetration - (bounds_origin.z - bounds_extent.z)),
            False,
            # This is an exact placement, not player movement.  Sweeping can
            # stop a freshly spawned prop against the terrain before its base
            # reaches the calculated surface, leaving it visibly floating.
            False,
        )
    except Exception as error:
        unreal.log_warning("World Builder could not ground {}: {}".format(actor.get_name(), error))


def _rot(yaw=0.0, pitch=0.0, roll=0.0):
    """Always name the axes: unreal.Rotator's positional order in Python is
    (roll, pitch, yaw), not C++'s (pitch, yaw, roll).  Passing a yaw as the
    second positional argument tips every prop over by a random pitch."""
    return unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw)


def _normalize(v):
    length = math.sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]) or 1.0
    return (v[0] / length, v[1] / length, v[2] / length)


def _cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def _dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def _rotate_about(v, axis, angle):
    """Rodrigues rotation of vector v around a unit axis."""
    c, s = math.cos(angle), math.sin(angle)
    kxv, kdv = _cross(axis, v), _dot(axis, v)
    return tuple(v[i] * c + kxv[i] * s + axis[i] * kdv * (1.0 - c) for i in range(3))


def _tilt_axis_angle(normal):
    """World rotation that carries straight up onto the given unit normal."""
    axis = (-normal[1], normal[0], 0.0)
    s = math.sqrt(axis[0] * axis[0] + axis[1] * axis[1])
    if s < 1e-6:
        return None, 0.0
    return (axis[0] / s, axis[1] / s, 0.0), math.acos(max(-1.0, min(1.0, normal[2])))


def _rotator_from_axes(x_axis, z_axis):
    """Unreal's FMatrix::Rotator, so a tilted prop keeps its exact heading."""
    y_axis = _cross(z_axis, x_axis)
    pitch = math.atan2(x_axis[2], math.sqrt(x_axis[0] * x_axis[0] + x_axis[1] * x_axis[1]))
    yaw = math.atan2(x_axis[1], x_axis[0])
    side = (-math.sin(yaw), math.cos(yaw), 0.0)
    roll = math.atan2(_dot(z_axis, side), _dot(y_axis, side))
    return _rot(math.degrees(yaw), math.degrees(pitch), math.degrees(roll))


def _ground_normal(x, y, features):
    """Surface normal of the generated knolls (straight up on flat ground)."""
    sample = 60.0
    gx = (_ground_height(x + sample, y, features) - _ground_height(x - sample, y, features)) / (sample * 2.0)
    gy = (_ground_height(x, y + sample, features) - _ground_height(x, y - sample, features)) / (sample * 2.0)
    return _normalize((-gx, -gy, 1.0))


def _surface_placement(x, y, features, follow, footprint, max_tilt=28.0, rng=None, wobble=0.0):
    """How far a prop leans into the slope, and how deep it sinks to hide its base.

    follow 0 keeps a prop perfectly upright (buildings), 1 lays it flat on the
    hill (rocks).  Whatever slope is *not* followed is covered by sinking the
    prop by footprint * tan(remaining angle), so no edge floats downhill.
    """
    ground = _ground_normal(x, y, features)
    target = _normalize(tuple(ground[i] * follow + (0.0, 0.0, 1.0)[i] * (1.0 - follow) for i in range(3)))
    if rng and wobble:
        target = _normalize((target[0] + rng.uniform(-wobble, wobble), target[1] + rng.uniform(-wobble, wobble), target[2]))
    limit = math.radians(max_tilt)
    if math.acos(max(-1.0, min(1.0, target[2]))) > limit:
        horizontal = math.sqrt(target[0] * target[0] + target[1] * target[1]) or 1.0
        target = (target[0] / horizontal * math.sin(limit), target[1] / horizontal * math.sin(limit), math.cos(limit))
    remaining = math.acos(max(-1.0, min(1.0, _dot(ground, target))))
    return target, min(footprint * math.tan(min(remaining, math.radians(60.0))), footprint * 0.8)


def _tilt_actor(actor, normal, yaw):
    """Lean an upright, grounded actor onto a surface normal around its base."""
    axis, angle = _tilt_axis_angle(normal)
    if axis is None or angle < math.radians(0.5):
        return
    bounds_origin, bounds_extent = actor.get_actor_bounds(False)
    location = actor.get_actor_location()
    base = (bounds_origin.x, bounds_origin.y, bounds_origin.z - bounds_extent.z)
    offset = _rotate_about((location.x - base[0], location.y - base[1], location.z - base[2]), axis, angle)
    heading = _rotate_about((math.cos(math.radians(yaw)), math.sin(math.radians(yaw)), 0.0), axis, angle)
    actor.set_actor_location_and_rotation(
        unreal.Vector(base[0] + offset[0], base[1] + offset[1], base[2] + offset[2]),
        _rotator_from_axes(heading, normal),
        False,
        False,
    )


def _spawn_grounded_mesh(mesh, location_xy, height, rotation, generated_tag, layer, label, tint=None, penetration=0.0, trunk_tint=None, normal=None, paint=None, stem_tint=None, ground_at=None, center_xy=False, solid=True, max_width=None):
    """Scale and ground a Z-up imported mesh using its actual transformed bounds.

    The mesh is grounded upright first, then (when a normal is given) leaned
    around its own base so it rests on the slope instead of a corner.
    ground_at(world_x, world_y) lets off-centre meshes (buildings, whose pivot
    is not under their middle) sit on the ground beneath their real centre.
    center_xy moves such a mesh so its visible middle, not its pivot, lands on
    the requested spot.  solid=False lets the player walk through (grass).
    max_width caps the footprint and stretches the mesh upward instead, which
    turns a wide boulder into a slender low-poly peak.
    """
    # A malformed FBX import can expose a material under a mesh name. Skipping
    # it is always better than leaving the user with a half-generated world.
    if not isinstance(mesh, unreal.StaticMesh):
        unreal.log_warning("World Builder skipped a non-mesh asset in {}".format(label))
        return None
    bounds = mesh.get_bounds()
    # A flat tile (no height) would be scaled by thousands into a floating
    # sheet over the whole world.  Never scale a mesh that has no real height.
    if bounds.box_extent.z * 2.0 < 0.08 * max(bounds.box_extent.x, bounds.box_extent.y, 1.0):
        unreal.log_warning("World Builder skipped the flat mesh {} in {}".format(mesh.get_name(), label))
        return None
    mesh_height = bounds.box_extent.z * 2.0
    scale = height / mesh_height
    scale_xy = scale
    if max_width:
        scale_xy = min(scale, max_width / (2.0 * max(bounds.box_extent.x, bounds.box_extent.y, 1.0)))
    actor = _spawn_mesh(
        mesh,
        unreal.Vector(location_xy.x, location_xy.y, location_xy.z),
        unreal.Vector(scale_xy, scale_xy, scale),
        rotation,
        generated_tag,
        layer,
        label,
        tint,
        trunk_tint,
        paint,
        stem_tint,
    )
    if center_xy:
        bounds_origin, _ = actor.get_actor_bounds(False)
        current = actor.get_actor_location()
        actor.set_actor_location(
            unreal.Vector(current.x + location_xy.x - bounds_origin.x, current.y + location_xy.y - bounds_origin.y, current.z),
            False, False,
        )
    if solid:
        _enable_world_collision(actor)
    else:
        actor.set_actor_enable_collision(False)
    ground_z = location_xy.z
    if ground_at:
        bounds_origin, _ = actor.get_actor_bounds(False)
        ground_z = ground_at(bounds_origin.x, bounds_origin.y)
    _snap_actor_to_ground(actor, ground_z, penetration)
    if normal:
        try:
            _tilt_actor(actor, normal, rotation.yaw)
        except Exception as error:
            unreal.log_warning("World Builder could not lean {}: {}".format(actor.get_name(), error))
    return actor


class _Footprints:
    """Keeps solid props (trunks, rocks, shrubs, landmarks, cliffs) from interpenetrating."""

    def __init__(self, cell=400.0):
        self.cell, self.grid, self.largest = cell, {}, 0.0

    def is_free(self, x, y, radius):
        reach = int(math.ceil((radius + self.largest) / self.cell))
        cx, cy = int(math.floor(x / self.cell)), int(math.floor(y / self.cell))
        for gx in range(cx - reach, cx + reach + 1):
            for gy in range(cy - reach, cy + reach + 1):
                for ox, oy, other in self.grid.get((gx, gy), ()):
                    if (x - ox) ** 2 + (y - oy) ** 2 < (radius + other) ** 2:
                        return False
        return True

    def reserve(self, x, y, radius):
        key = (int(math.floor(x / self.cell)), int(math.floor(y / self.cell)))
        self.grid.setdefault(key, []).append((x, y, radius))
        self.largest = max(self.largest, radius)




def _hash_noise(ix, iy, seed):
    """A repeatable pseudo-random value in [-1, 1] for one lattice point."""
    n = (ix * 374761393 + iy * 668265263 + seed * 2246822519) & 0xFFFFFFFF
    n = ((n ^ (n >> 13)) * 1274126177) & 0xFFFFFFFF
    return ((n ^ (n >> 16)) & 0xFFFF) / 32767.5 - 1.0


def _value_noise(x, y, seed):
    ix, iy = math.floor(x), math.floor(y)
    fx, fy = x - ix, y - iy
    fx, fy = fx * fx * (3.0 - 2.0 * fx), fy * fy * (3.0 - 2.0 * fy)
    a, b = _hash_noise(ix, iy, seed), _hash_noise(ix + 1, iy, seed)
    c, d = _hash_noise(ix, iy + 1, seed), _hash_noise(ix + 1, iy + 1, seed)
    return a + (b - a) * fx + (c - a) * fy + (a - b - c + d) * fx * fy


def _fbm(x, y, seed, octaves=4):
    """Layered noise in about [-1, 1]: broad shapes with smaller details on top."""
    total, amplitude, norm = 0.0, 1.0, 0.0
    for octave in range(octaves):
        total += _value_noise(x, y, seed + octave * 101) * amplitude
        norm += amplitude
        x, y, amplitude = x * 2.03 + 17.1, y * 2.03 - 9.7, amplitude * 0.5
    return total / norm


class _Relief(list):
    """The sculpted ground: a height grid that exactly matches the ground mesh.

    The list itself holds the inner massifs as (x, y, radius, peak), so the
    trails still walk around them and hilltop landmarks still crown them.
    """

    def __init__(self, massifs, half_extent, cells, heights):
        super().__init__(massifs)
        self.half_extent, self.cells, self.heights = half_extent, cells, heights
        self.cell = half_extent * 2.0 / cells

    def height(self, x, y):
        n = self.cells
        gx = min(max((x + self.half_extent) / self.cell, 0.0), n - 1e-6)
        gy = min(max((y + self.half_extent) / self.cell, 0.0), n - 1e-6)
        i, j = int(gx), int(gy)
        fx, fy = gx - i, gy - j
        row, next_row = self.heights[j], self.heights[j + 1]
        # Same diagonal as the mesh triangles, so props sit exactly on the facets.
        if fx + fy <= 1.0:
            h00 = row[i]
            return h00 + fx * (row[i + 1] - h00) + fy * (next_row[i] - h00)
        h11 = next_row[i + 1]
        return h11 + (1.0 - fx) * (next_row[i] - h11) + (1.0 - fy) * (row[i + 1] - h11)


_relief_cache = {}


WALKABLE_SLOPE = 38.0   # degrees; the character climbs up to 50


def _soften_walkable_slopes(heights, half_extent, step, half):
    """Relax every slope steeper than WALKABLE_SLOPE inside the playable world.

    Valleys and hills keep their shape, but no wall is too steep to walk out
    of.  The far mountains (outside the invisible boundary) are left alone.
    """
    limit = step * math.tan(math.radians(WALKABLE_SLOPE))
    n = len(heights) - 1
    weight = []
    for j in range(n + 1):
        y = -half_extent + j * step
        row = []
        for i in range(n + 1):
            r = math.hypot(-half_extent + i * step, y) / half
            row.append(1.0 - _smoothstep(0.95, 1.12, r))
        weight.append(row)
    for _ in range(40):
        changed = False
        for j in range(1, n):
            for i in range(1, n):
                k = weight[j][i]
                if k <= 0.0:
                    continue
                h = heights[j][i]
                around = (heights[j][i - 1], heights[j][i + 1], heights[j - 1][i], heights[j + 1][i])
                if max(abs(h - a) for a in around) > limit:
                    heights[j][i] = h + (sum(around) / 4.0 - h) * 0.5 * k
                    changed = True
        if not changed:
            break


def _terrain_features(data):
    """Rolling ground, winding valleys, a few inner massifs and tall far ranges."""
    seed, terrain = int(data["seed"]), float(data["terrain"])
    hills = max(0.0, min(100.0, float(data.get("hills", 50.0)))) / 100.0
    mountains = bool(data.get("mountains", True))
    key = (seed, terrain, hills, mountains)
    if key in _relief_cache:
        return _relief_cache[key]
    rng = random.Random(seed + 2718)
    half = terrain * 0.5
    relief = 0.25 + 1.5 * hills
    # Inner massifs: real hills to climb, kept away from the trail start.
    start = (-terrain * 0.36, 0.0)
    massifs = []
    for attempt in range(40):
        if len(massifs) >= round(1 + 3 * hills):
            break
        angle, distance = rng.uniform(0.0, math.tau), half * rng.uniform(0.3, 0.68)
        x, y = math.cos(angle) * distance, math.sin(angle) * distance
        radius = terrain * rng.uniform(0.07, 0.13)
        if math.hypot(x - start[0], y - start[1]) < radius + terrain * 0.1:
            continue
        if any(math.hypot(x - mx, y - my) < radius + mr for mx, my, mr, _ in massifs):
            continue
        tall = 1.8 if not massifs else 1.0
        massifs.append((x, y, radius, terrain * rng.uniform(0.05, 0.10) * tall * (0.3 + 1.2 * hills)))
    # The mesh reaches far past the playable rim so the ranges stand on the horizon.
    half_extent = terrain * 2.0
    cells = int(max(80, min(140, half_extent * 2.0 / 420.0)))
    step = half_extent * 2.0 / cells
    heights = []
    for j in range(cells + 1):
        y = -half_extent + j * step
        row = []
        for i in range(cells + 1):
            x = -half_extent + i * step
            r = math.hypot(x, y) / half
            h = _fbm(x / (terrain * 0.28), y / (terrain * 0.28), seed + 3) * terrain * 0.045 * relief
            h += _fbm(x / (terrain * 0.07), y / (terrain * 0.07), seed + 7, 3) * terrain * 0.010 * relief
            # Valleys follow the zero lines of a broad noise: long, winding hollows.
            ridge = 1.0 - abs(_fbm(x / (terrain * 0.24), y / (terrain * 0.24), seed + 13, 3))
            h -= _smoothstep(0.78, 0.97, ridge) * terrain * 0.035 * relief
            for mx, my, mr, peak in massifs:
                d2 = ((x - mx) ** 2 + (y - my) ** 2) / (mr * mr)
                if d2 < 9.0:
                    h += peak * math.exp(-d2 * 1.6) * (0.85 + 0.3 * _fbm(x / mr, y / mr, seed + 17, 2))
            # Foothills start at the edge of the playable world; the high ranges
            # stand much further away, so the forest feels open, not walled in.
            rim = _smoothstep(0.95, 2.6, r + 0.35 * _fbm(x / (terrain * 0.2), y / (terrain * 0.2), seed + 21, 3))
            if mountains:
                crest = 1.0 - abs(_fbm(x / (terrain * 0.13), y / (terrain * 0.13), seed + 29, 4))
                h += rim ** 1.6 * terrain * 0.42 * (0.25 + 0.75 * crest) + rim * terrain * 0.03
            else:
                h += rim * terrain * 0.05
            row.append(h)
        heights.append(row)
    _soften_walkable_slopes(heights, half_extent, step, half)
    relief_shape = _Relief(massifs, half_extent, cells, heights)
    _relief_cache.clear()
    _relief_cache[key] = relief_shape
    return relief_shape


def _ground_height(x, y, features):
    """Height of the sculpted ground (or of the old domes, for a plain list)."""
    if isinstance(features, _Relief):
        return features.height(x, y)
    height = 0.0
    for center_x, center_y, radius, peak in features:
        dx, dy = (x - center_x) / radius, (y - center_y) / radius
        distance_squared = dx * dx + dy * dy
        if distance_squared < 1.0:
            height = max(height, peak * math.sqrt(1.0 - distance_squared))
    return height


def _ground_slope(x, y, features):
    """Small finite-difference slope test used to keep tall foliage off steep domes."""
    sample = 90.0
    dx = _ground_height(x + sample, y, features) - _ground_height(x - sample, y, features)
    dy = _ground_height(x, y + sample, features) - _ground_height(x, y - sample, features)
    return math.sqrt(dx * dx + dy * dy) / (sample * 2.0)


def _terrain_noise(x, y, seed, scale):
    """A deterministic, broad noise field used to soften group edges."""
    scale = max(300.0, scale)
    a = math.sin((x + seed * 17.0) / scale * 2.19 + y / scale * 0.73)
    b = math.cos((y - seed * 11.0) / scale * 1.61 - x / scale * 0.47)
    c = math.sin((x + y + seed * 5.0) / scale * 4.07)
    return max(0.0, min(1.0, 0.5 + a * 0.24 + b * 0.19 + c * 0.07))




def _finish_palette(colors):
    """Supply shared paint treatments without flattening each world direction."""
    colors.setdefault("trunk", unreal.LinearColor(0.34, 0.18, 0.08, 1.0))
    # Bark is intentionally a two-tone design choice, rather than the usual
    # natural brown.  The seeded selection in _build_vegetation gives every
    # world a stable mix of chalk-white and ink-black trunks.
    colors.setdefault("trunk_light", unreal.LinearColor(0.94, 0.89, 0.76, 1.0))
    colors.setdefault("trunk_dark", unreal.LinearColor(0.018, 0.012, 0.035, 1.0))
    colors.setdefault("path", unreal.LinearColor(0.18, 0.10, 0.055, 1.0))
    ground = colors["ground"]
    # A second, muted grass tone so a meadow never reads as one flat color.
    grass = colors["grass"]
    colors["grass_dry"] = unreal.LinearColor(
        grass.r * 0.5 + ground.r * 0.5, grass.g * 0.5 + ground.g * 0.5, grass.b * 0.5 + ground.b * 0.5, 1.0)
    # Trails are trodden ground: mostly the ground color, nudged toward the
    # palette's path color, so they guide the eye without shouting.
    path = colors["path"]
    rock = colors.get("rock", ground)
    colors["path_stone"] = unreal.LinearColor(
        rock.r * 0.7 + path.r * 0.3, rock.g * 0.7 + path.g * 0.3, rock.b * 0.7 + path.b * 0.3, 1.0)
    colors["path"] = unreal.LinearColor(
        (ground.r * 0.72 + path.r * 0.28) * 0.82, (ground.g * 0.72 + path.g * 0.28) * 0.82, (ground.b * 0.72 + path.b * 0.28) * 0.82, 1.0)
    return colors


# Artist palettes, independent of the season.  Each one is a deliberate,
# non-natural color story; "Natural (season)" keeps the realistic greens.
# Values are linear RGB, as Unreal materials expect.
NATURAL_PALETTE = "Natural (season)"
DESIGNED_PALETTES = {
    "Soft Dusk": {
        "description": "misty sage meadows, apricot canopies, blush and cream flowers, soft lilac-grey hills",
        "ground": (0.20, 0.22, 0.16), "ground_mound": (0.30, 0.31, 0.22),
        "leaf": (0.78, 0.46, 0.26), "accent_foliage": (0.62, 0.36, 0.34), "grass": (0.34, 0.40, 0.24),
        "mountain": (0.30, 0.29, 0.36), "rock": (0.48, 0.46, 0.42),
        "flower_a": (0.95, 0.88, 0.72), "flower_b": (0.90, 0.60, 0.58),
        "sky": (0.80, 0.62, 0.52), "sun": (1.0, 0.70, 0.45),
        "trunk_light": (0.88, 0.84, 0.76), "trunk_dark": (0.16, 0.13, 0.12), "path": (0.50, 0.44, 0.34),
    },
    "Emberwild": {
        "description": "crimson grass, orange canopies, ivory and ink bark, blue-violet stone",
        "ground": (0.13, 0.04, 0.21), "ground_mound": (0.26, 0.075, 0.34),
        "leaf": (1.0, 0.18, 0.025), "accent_foliage": (0.96, 0.055, 0.22), "grass": (0.78, 0.025, 0.10),
        "mountain": (0.095, 0.075, 0.27), "rock": (0.075, 0.27, 0.43),
        "flower_a": (0.05, 0.84, 0.74), "flower_b": (1.0, 0.73, 0.17),
        "sky": (0.23, 0.10, 0.48), "sun": (1.0, 0.45, 0.22),
        "trunk_light": (0.98, 0.92, 0.78), "trunk_dark": (0.012, 0.006, 0.028), "path": (0.31, 0.035, 0.09),
    },
    "Candy Cloud": {
        "description": "bubblegum canopies on periwinkle hills, mint shrubs, cream bark",
        "ground": (0.24, 0.22, 0.62), "ground_mound": (0.38, 0.36, 0.82),
        "leaf": (1.0, 0.36, 0.62), "accent_foliage": (0.36, 0.92, 0.74), "grass": (0.95, 0.62, 0.86),
        "mountain": (0.36, 0.30, 0.70), "rock": (0.80, 0.78, 0.98),
        "flower_a": (1.0, 0.93, 0.40), "flower_b": (0.40, 0.80, 1.0),
        "sky": (0.80, 0.58, 0.92), "sun": (1.0, 0.84, 0.90),
        "trunk_light": (1.0, 0.96, 0.90), "trunk_dark": (0.36, 0.16, 0.42), "path": (0.98, 0.84, 0.66),
    },
    "Neon Night": {
        "description": "electric cyan trees, magenta shrubs, ultraviolet grass on a midnight floor",
        "ground": (0.012, 0.010, 0.050), "ground_mound": (0.030, 0.018, 0.110),
        "leaf": (0.0, 0.90, 1.0), "accent_foliage": (1.0, 0.0, 0.55), "grass": (0.38, 0.0, 1.0),
        "mountain": (0.040, 0.020, 0.150), "rock": (0.12, 0.0, 0.30),
        "flower_a": (0.85, 1.0, 0.0), "flower_b": (1.0, 0.36, 0.0),
        "sky": (0.03, 0.0, 0.12), "sun": (0.55, 0.42, 1.0),
        "trunk_light": (0.92, 0.95, 1.0), "trunk_dark": (0.0, 0.0, 0.0), "path": (0.26, 0.0, 0.36),
    },
    "Coral Lagoon": {
        "description": "coral canopies over turquoise ground, sandy stone, aqua grass",
        "ground": (0.02, 0.30, 0.33), "ground_mound": (0.04, 0.46, 0.45),
        "leaf": (1.0, 0.34, 0.28), "accent_foliage": (1.0, 0.72, 0.18), "grass": (0.04, 0.70, 0.72),
        "mountain": (0.04, 0.10, 0.30), "rock": (0.92, 0.80, 0.62),
        "flower_a": (1.0, 0.22, 0.55), "flower_b": (0.58, 0.38, 1.0),
        "sky": (0.24, 0.64, 0.86), "sun": (1.0, 0.90, 0.74),
        "trunk_light": (1.0, 0.95, 0.86), "trunk_dark": (0.10, 0.05, 0.16), "path": (0.90, 0.74, 0.48),
    },
    "Golden Hour": {
        "description": "gilded canopies over burgundy earth, amber grass, red shrubs",
        "ground": (0.20, 0.045, 0.035), "ground_mound": (0.34, 0.085, 0.055),
        "leaf": (1.0, 0.68, 0.04), "accent_foliage": (0.82, 0.10, 0.04), "grass": (0.96, 0.52, 0.12),
        "mountain": (0.34, 0.09, 0.07), "rock": (0.58, 0.22, 0.13),
        "flower_a": (1.0, 0.98, 0.84), "flower_b": (0.62, 0.04, 0.34),
        "sky": (0.95, 0.44, 0.24), "sun": (1.0, 0.60, 0.30),
        "trunk_light": (0.98, 0.92, 0.80), "trunk_dark": (0.08, 0.03, 0.02), "path": (0.84, 0.62, 0.32),
    },
    "Frost Violet": {
        "description": "violet canopies on icy blue ground, glacier stone, pale silver grass",
        "ground": (0.08, 0.13, 0.38), "ground_mound": (0.17, 0.26, 0.58),
        "leaf": (0.52, 0.26, 0.95), "accent_foliage": (0.26, 0.84, 1.0), "grass": (0.74, 0.80, 1.0),
        "mountain": (0.14, 0.14, 0.34), "rock": (0.84, 0.90, 1.0),
        "flower_a": (1.0, 0.52, 0.84), "flower_b": (0.96, 0.94, 0.52),
        "sky": (0.20, 0.24, 0.55), "sun": (0.72, 0.76, 1.0),
        "trunk_light": (0.95, 0.97, 1.0), "trunk_dark": (0.05, 0.03, 0.12), "path": (0.50, 0.44, 0.70),
    },
}
PALETTE_NAMES = tuple(DESIGNED_PALETTES) + (NATURAL_PALETTE,)
SEASONS = ("Spring", "Summer", "Autumn", "Winter")


def _designed_palette(name, ground_richness):
    """Turn a palette table into Unreal colors; Ground richness deepens or lifts the floor."""
    richness = 0.7 + ground_richness * 0.6
    colors = {}
    for key, value in DESIGNED_PALETTES[name].items():
        if key == "description":
            continue
        if key == "ground":
            value = tuple(min(1.0, channel * richness) for channel in value)
        colors[key] = unreal.LinearColor(value[0], value[1], value[2], 1.0)
    return colors


def _palette_description(name):
    if name in DESIGNED_PALETTES:
        return DESIGNED_PALETTES[name]["description"]
    return "realistic greens and browns that follow the chosen season"


def _palette(data):
    """Art directions applied across terrain, plants, stone, sky, and sunlight."""
    ground_richness = data["ground_tint"]
    name = data.get("palette", DEFAULT_PALETTE)
    if name in DESIGNED_PALETTES:
        return _finish_palette(_designed_palette(name, ground_richness))
    theme = data["theme"]
    if theme == "Spring":
        return _finish_palette({
            "ground": unreal.LinearColor(0.10 + ground_richness * 0.16, 0.28 + ground_richness * 0.26, 0.08, 1.0),
            "ground_mound": unreal.LinearColor(0.17, 0.42, 0.13, 1.0),
            "leaf": unreal.LinearColor(0.24, 0.52, 0.10, 1.0),
            "grass": unreal.LinearColor(0.20, 0.55, 0.14, 1.0),
            "mountain": unreal.LinearColor(0.24, 0.31, 0.20, 1.0),
            "rock": unreal.LinearColor(0.30, 0.36, 0.30, 1.0),
            "accent_foliage": unreal.LinearColor(0.54, 0.78, 0.23, 1.0),
            "flower_a": unreal.LinearColor(0.98, 0.46, 0.66, 1.0),
            "flower_b": unreal.LinearColor(0.96, 0.82, 0.26, 1.0),
            "sky": unreal.LinearColor(0.44, 0.70, 0.96, 1.0),
            "sun": unreal.LinearColor(1.0, 0.88, 0.68, 1.0),
            "trunk": unreal.LinearColor(0.41, 0.24, 0.10, 1.0),
        })
    if theme == "Autumn":
        return _finish_palette({
            "ground": unreal.LinearColor(0.20 + ground_richness * 0.18, 0.105 + ground_richness * 0.12, 0.035, 1.0),
            "ground_mound": unreal.LinearColor(0.38, 0.17, 0.045, 1.0),
            "leaf": unreal.LinearColor(0.74, 0.19, 0.035, 1.0),
            "grass": unreal.LinearColor(0.47, 0.26, 0.045, 1.0),
            "mountain": unreal.LinearColor(0.29, 0.17, 0.10, 1.0),
            "rock": unreal.LinearColor(0.33, 0.22, 0.15, 1.0),
            "accent_foliage": unreal.LinearColor(0.90, 0.31, 0.035, 1.0),
            "flower_a": unreal.LinearColor(0.95, 0.31, 0.08, 1.0),
            "flower_b": unreal.LinearColor(0.98, 0.68, 0.08, 1.0),
            "sky": unreal.LinearColor(0.56, 0.29, 0.16, 1.0),
            "sun": unreal.LinearColor(1.0, 0.49, 0.18, 1.0),
            "trunk": unreal.LinearColor(0.55, 0.33, 0.12, 1.0),
        })
    if theme == "Winter":
        return _finish_palette({
            "ground": unreal.LinearColor(0.13 + ground_richness * 0.10, 0.20 + ground_richness * 0.14, 0.22 + ground_richness * 0.14, 1.0),
            "ground_mound": unreal.LinearColor(0.32, 0.46, 0.49, 1.0),
            "leaf": unreal.LinearColor(0.10, 0.27, 0.24, 1.0),
            "grass": unreal.LinearColor(0.20, 0.38, 0.36, 1.0),
            "mountain": unreal.LinearColor(0.25, 0.33, 0.37, 1.0),
            "rock": unreal.LinearColor(0.29, 0.36, 0.40, 1.0),
            "accent_foliage": unreal.LinearColor(0.38, 0.68, 0.76, 1.0),
            "flower_a": unreal.LinearColor(0.47, 0.68, 0.98, 1.0),
            "flower_b": unreal.LinearColor(0.75, 0.86, 1.0, 1.0),
            "sky": unreal.LinearColor(0.22, 0.39, 0.58, 1.0),
            "sun": unreal.LinearColor(0.62, 0.78, 1.0, 1.0),
            "trunk": unreal.LinearColor(0.67, 0.79, 0.80, 1.0),
        })
    return _finish_palette({
        "ground": unreal.LinearColor(0.055 + ground_richness * 0.18, 0.17 + ground_richness * 0.34, 0.04 + ground_richness * 0.10, 1.0),
        "ground_mound": unreal.LinearColor(0.12, 0.34, 0.07, 1.0),
        "leaf": unreal.LinearColor(0.025, 0.29, 0.06, 1.0),
        "grass": unreal.LinearColor(0.06, 0.37, 0.06, 1.0),
        "mountain": unreal.LinearColor(0.32, 0.19, 0.12, 1.0),
        "rock": unreal.LinearColor(0.25, 0.29, 0.24, 1.0),
        "accent_foliage": unreal.LinearColor(0.10, 0.48, 0.13, 1.0),
        "flower_a": unreal.LinearColor(0.98, 0.57, 0.65, 1.0),
        "flower_b": unreal.LinearColor(0.96, 0.84, 0.56, 1.0),
        "sky": unreal.LinearColor(0.30, 0.62, 0.96, 1.0),
        "sun": unreal.LinearColor(1.0, 0.84, 0.56, 1.0),
        "trunk": unreal.LinearColor(0.34, 0.18, 0.08, 1.0),
    })


def _ensure_collision(meshes, hulls=6):
    """Give imported props real collision so the player cannot walk through them.

    Kenney FBX files arrive without simple collision, and Unreal's player
    capsule only collides with simple shapes.  A small convex decomposition is
    generated once per mesh and saved with the asset.
    """
    subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    for mesh in meshes:
        try:
            if subsystem.get_convex_collision_count(mesh) or subsystem.get_simple_collision_count(mesh):
                continue
            if subsystem.set_convex_decomposition_collisions(mesh, hulls, 16, 100000):
                unreal.EditorAssetLibrary.save_loaded_asset(mesh, False)
        except Exception as error:
            unreal.log_warning("World Builder could not add collision to {}: {}".format(mesh.get_name(), error))


def _push_out_of_knolls(x, y, features, margin=1.12):
    """Trails go around hills, like real footpaths, instead of over their steep sides."""
    for _ in range(3):
        moved = False
        for center_x, center_y, radius, _peak in features:
            dx, dy = x - center_x, y - center_y
            distance = math.sqrt(dx * dx + dy * dy)
            if distance < radius * margin:
                if distance < 1.0:
                    dx, dy, distance = 1.0, 0.0, 1.0
                x, y = center_x + dx / distance * radius * margin, center_y + dy / distance * radius * margin
                moved = True
        if not moved:
            break
    return x, y


def _catmull_rom(points, spacing):
    """Resample a polyline into an evenly spaced, smoothly curving trail."""
    if len(points) < 2:
        return list(points)
    padded = [points[0]] + list(points) + [points[-1]]
    samples = []
    for index in range(1, len(padded) - 2):
        p0, p1, p2, p3 = padded[index - 1], padded[index], padded[index + 1], padded[index + 2]
        steps = max(1, int(math.hypot(p2[0] - p1[0], p2[1] - p1[1]) / spacing))
        for step in range(steps):
            t = step / float(steps)
            t2, t3 = t * t, t * t * t
            samples.append(tuple(
                0.5 * ((2 * p1[i]) + (-p0[i] + p2[i]) * t + (2 * p0[i] - 5 * p1[i] + 4 * p2[i] - p3[i]) * t2 + (-p0[i] + 3 * p1[i] - 3 * p2[i] + p3[i]) * t3)
                for i in range(2)
            ))
    samples.append(points[-1])
    return samples


def _trail_network(seed, terrain, features, paths="Network"):
    """A few narrow, winding footpaths: start -> clearing -> far side, plus side trails.

    Pure function of the ground seed and scale, so Refresh Plants and the
    player start always agree with the trails already on the ground.
    """
    rng = random.Random(seed + 3301)
    start = (-terrain * 0.36, terrain * rng.uniform(-0.03, 0.03))
    hub = (terrain * rng.uniform(-0.08, 0.08), terrain * rng.uniform(-0.10, 0.10))
    routes = [
        (125.0, [start, hub, (terrain * 0.36, terrain * rng.uniform(-0.16, 0.16))]),
        (85.0, [hub, (terrain * rng.uniform(-0.22, 0.22), terrain * 0.33)]),
        (85.0, [hub, (terrain * rng.uniform(-0.22, 0.22), -terrain * 0.33)]),
    ]
    routes = {"None": [], "Trail": routes[:1]}.get(paths, routes)
    trails = []
    for width, legs in routes:
        controls = [_push_out_of_knolls(legs[0][0], legs[0][1], features)]
        for (ax, ay), (bx, by) in zip(legs, legs[1:]):
            length = math.hypot(bx - ax, by - ay)
            pieces = max(2, int(length / 1100.0))
            nx, ny = -(by - ay) / max(1.0, length), (bx - ax) / max(1.0, length)
            for piece in range(1, pieces + 1):
                t = piece / float(pieces)
                wander = 0.0 if piece == pieces else rng.uniform(-1.0, 1.0) * length / pieces * 0.38
                x, y = ax + (bx - ax) * t + nx * wander, ay + (by - ay) * t + ny * wander
                controls.append(_push_out_of_knolls(x, y, features))
        points = [_push_out_of_knolls(x, y, features, 1.06) for x, y in _catmull_rom(controls, width * 0.5)]
        trails.append({"width": width, "points": points})
    return trails


def _trail_start(data, features):
    """Where the player begins: the first step of the main trail, facing along it."""
    trails = _trail_network(data["seed"], data["terrain"], features, data.get("paths", "Network"))
    if not trails:
        # No paths: begin at the western edge of the clearing, facing east.
        x, y = _push_out_of_knolls(-data["terrain"] * 0.36, 0.0, features)
        return x, y, 0.0
    points = trails[0]["points"]
    (x0, y0), (x1, y1) = points[0], points[min(4, len(points) - 1)]
    return x0, y0, math.degrees(math.atan2(y1 - y0, x1 - x0))


def _knoll_mesh():
    """A sphere whose collision is its own surface.

    The engine sphere collides as a perfect sphere, and a sphere cannot be
    squashed: on a wide, low knoll it shrank to the knoll's height, so the
    player walked through the outer half of every hill.
    """
    path = "/Game/WorldBuilder/Meshes/SM_IWB_Knoll"
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.duplicate_asset("/Engine/BasicShapes/Sphere", path)
    mesh = unreal.load_asset(path)
    if not isinstance(mesh, unreal.StaticMesh):
        return unreal.load_asset("/Engine/BasicShapes/Sphere")
    body = mesh.get_editor_property("body_setup")
    if body.get_editor_property("collision_trace_flag") != unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE:
        body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        unreal.EditorAssetLibrary.save_loaded_asset(mesh, False)
    return mesh


def _ground_material():
    """Three palette colours blended by the vertex colours of each facet:
    R = hill, G = rock, B = brightness (0.5 is neutral); snow and rain on top."""
    path = "/Game/WorldBuilder/Materials/M_IWB_Ground"
    material = unreal.load_asset(path)
    if material and _material_is_current(material):
        return material
    lib = unreal.MaterialEditingLibrary
    if not material:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "M_IWB_Ground", "/Game/WorldBuilder/Materials", unreal.Material, unreal.MaterialFactoryNew())
    lib.delete_all_material_expressions(material)
    params = {}
    for index, name in enumerate(("Low", "Mid", "High")):
        node = lib.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -1500, index * 160)
        node.set_editor_property("parameter_name", name)
        params[name] = node
    colors = lib.create_material_expression(material, unreal.MaterialExpressionVertexColor, -1500, 520)
    hill = lib.create_material_expression(material, unreal.MaterialExpressionLinearInterpolate, -1250, 80)
    lib.connect_material_expressions(params["Low"], "", hill, "A")
    lib.connect_material_expressions(params["Mid"], "", hill, "B")
    lib.connect_material_expressions(colors, "R", hill, "Alpha")
    rock = lib.create_material_expression(material, unreal.MaterialExpressionLinearInterpolate, -1100, 160)
    lib.connect_material_expressions(hill, "", rock, "A")
    lib.connect_material_expressions(params["High"], "", rock, "B")
    lib.connect_material_expressions(colors, "G", rock, "Alpha")
    offset = lib.create_material_expression(material, unreal.MaterialExpressionAdd, -1250, 520)
    offset.set_editor_property("const_b", 0.5)
    lib.connect_material_expressions(colors, "B", offset, "A")
    shade = lib.create_material_expression(material, unreal.MaterialExpressionMultiply, -950, 240)
    lib.connect_material_expressions(rock, "", shade, "A")
    lib.connect_material_expressions(offset, "", shade, "B")
    _weather_shading(material, shade)
    _finish_material(material)
    return material


def _paint_ground(actor, palette):
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    if not component:
        return
    material = component.get_material(0)
    if not isinstance(material, unreal.MaterialInstanceDynamic):
        component.set_material(0, _ground_material())
        material = component.create_dynamic_material_instance(0)
    for name, key in (("Low", "ground"), ("Mid", "ground_mound"), ("High", "mountain")):
        material.set_vector_parameter_value(name, palette[key])


def _ground_mesh(relief, terrain, tag):
    """Write the relief into a faceted static mesh asset with walkable collision."""
    path = "/Game/WorldBuilder/Meshes/SM_IWB_Ground" + ("_Preview" if tag == PREVIEW_TAG else "")
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.duplicate_asset("/Engine/BasicShapes/Plane", path)
    asset = unreal.load_asset(path)
    # Set before the geometry is written, so the rebuild cooks the collision.
    body = asset.get_editor_property("body_setup")
    body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    n, step, half_extent, heights = relief.cells, relief.cell, relief.half_extent, relief.heights
    rng = random.Random(n * 7919 + int(terrain))
    vertices, triangles, normals, colours, uvs = [], [], [], [], []

    def facet(a, b, c):
        ux, uy, uz = b[0] - a[0], b[1] - a[1], b[2] - a[2]
        vx, vy, vz = c[0] - a[0], c[1] - a[1], c[2] - a[2]
        nx, ny, nz = uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx
        if nz < 0.0:
            nx, ny, nz = -nx, -ny, -nz
        else:
            # Unreal is left-handed: a face is seen from above when its corners
            # turn the "other" way, so the order is flipped for every facet.
            b, c = c, b
        length = math.sqrt(nx * nx + ny * ny + nz * nz) or 1.0
        normal = unreal.Vector(nx / length, ny / length, nz / length)
        mean = (a[2] + b[2] + c[2]) / 3.0
        slope = 1.0 - nz / length
        hill = _smoothstep(-terrain * 0.005, terrain * 0.05, mean)
        rock = max(_smoothstep(0.22, 0.5, slope), _smoothstep(terrain * 0.07, terrain * 0.24, mean))
        light = 0.5 + rng.uniform(-0.07, 0.07) + 0.35 * _smoothstep(terrain * 0.25, terrain * 0.5, mean)
        light -= 0.12 * _smoothstep(0.0, -terrain * 0.03, mean)
        colour = unreal.LinearColor(hill, rock, max(0.0, min(1.0, light)), 1.0)
        base = len(vertices)
        for point in (a, b, c):
            vertices.append(unreal.Vector(point[0], point[1], point[2]))
            uvs.append(unreal.Vector2D(point[0] / 1000.0, point[1] / 1000.0))
            normals.append(normal)
            colours.append(colour)
        triangles.append(unreal.IntVector(base, base + 1, base + 2))

    for j in range(n):
        y0, y1 = -half_extent + j * step, -half_extent + (j + 1) * step
        for i in range(n):
            x0, x1 = -half_extent + i * step, -half_extent + (i + 1) * step
            p00, p10 = (x0, y0, heights[j][i]), (x1, y0, heights[j][i + 1])
            p01, p11 = (x0, y1, heights[j + 1][i]), (x1, y1, heights[j + 1][i + 1])
            facet(p00, p10, p01)
            facet(p10, p11, p01)
    buffers = unreal.GeometryScriptSimpleMeshBuffers()
    buffers.set_editor_property("vertices", vertices)
    buffers.set_editor_property("normals", normals)
    buffers.set_editor_property("vertex_colors", colours)
    buffers.set_editor_property("uv0", uvs)
    buffers.set_editor_property("triangles", triangles)
    mesh = unreal.DynamicMesh()
    mesh.append_buffers_to_mesh(buffers)
    options = unreal.GeometryScriptCopyMeshToAssetOptions()
    options.set_editor_property("enable_recompute_normals", False)
    options.set_editor_property("enable_recompute_tangents", True)
    options.set_editor_property("replace_materials", True)
    options.set_editor_property("new_materials", [_ground_material()])
    options.set_editor_property("new_material_slot_names", ["Ground"])
    # No baked lighting here: skip lightmap UVs, which this mesh does not need.
    subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    try:
        settings = subsystem.get_lod_build_settings(asset, 0)
        settings.set_editor_property("generate_lightmap_u_vs", False)
        settings.set_editor_property("recompute_normals", False)
        settings.set_editor_property("recompute_tangents", True)
        subsystem.set_lod_build_settings(asset, 0, settings)
    except Exception as error:
        unreal.log_warning("World Builder ground build settings skipped: {}".format(error))
    unreal.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(mesh, asset, options, unreal.GeometryScriptMeshWriteLOD())
    body = asset.get_editor_property("body_setup")
    body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
    return asset


def _path_stone_meshes():
    """Three flat, irregular low-poly stepping stones (made once, then instanced)."""
    meshes = []
    for variant in range(3):
        path = "/Game/WorldBuilder/Meshes/SM_IWB_PathStone{}".format(variant)
        asset = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
        if asset and unreal.EditorAssetLibrary.get_metadata_tag(asset, "IWBVersion") == "stone-1":
            meshes.append(asset)
            continue
        if not asset:
            unreal.EditorAssetLibrary.duplicate_asset("/Engine/BasicShapes/Plane", path)
            asset = unreal.load_asset(path)
        rng = random.Random(9100 + variant)
        sides = 6 + variant
        rim = []
        for k in range(sides):
            angle = math.tau * (k + rng.uniform(-0.25, 0.25)) / sides
            radius = rng.uniform(0.75, 1.0) * 50.0
            rim.append((math.cos(angle) * radius * 1.15, math.sin(angle) * radius * 0.9))
        # A slightly domed top on a short, sunk base: unit size is 100 cm across.
        top = [(x * 0.82, y * 0.82, 6.0 + rng.uniform(-0.8, 0.8)) for x, y in rim]
        base = [(x, y, 0.0) for x, y in rim]
        bottom = [(x, y, -6.0) for x, y in rim]
        centre = (0.0, 0.0, 7.0)
        faces = [(centre, top[k], top[(k + 1) % sides]) for k in range(sides)]
        for ring_a, ring_b in ((top, base), (base, bottom)):
            for k in range(sides):
                a, b = ring_a[k], ring_a[(k + 1) % sides]
                c, d = ring_b[(k + 1) % sides], ring_b[k]
                faces += [(a, d, c), (a, c, b)]
        vertices, normals, triangles, uvs = [], [], [], []
        for a, b, c in faces:
            cross = _cross((b[0] - a[0], b[1] - a[1], b[2] - a[2]), (c[0] - a[0], c[1] - a[1], c[2] - a[2]))
            middle = ((a[0] + b[0] + c[0]) / 3.0, (a[1] + b[1] + c[1]) / 3.0, (a[2] + b[2] + c[2]) / 3.0 - 2.0)
            outward = _normalize(middle)
            if _dot(cross, outward) > 0.0:
                b, c = c, b
            first = len(vertices)
            for point in (a, b, c):
                vertices.append(unreal.Vector(*point))
                normals.append(unreal.Vector(*outward))
                uvs.append(unreal.Vector2D(point[0] / 100.0, point[1] / 100.0))
            triangles.append(unreal.IntVector(first, first + 1, first + 2))
        buffers = unreal.GeometryScriptSimpleMeshBuffers()
        for name, values in (("vertices", vertices), ("normals", normals), ("triangles", triangles), ("uv0", uvs)):
            buffers.set_editor_property(name, values)
        mesh = unreal.DynamicMesh()
        mesh.append_buffers_to_mesh(buffers)
        mesh.compute_split_normals(unreal.GeometryScriptSplitNormalsOptions(), unreal.GeometryScriptCalculateNormalsOptions())
        subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
        settings = subsystem.get_lod_build_settings(asset, 0)
        settings.set_editor_property("generate_lightmap_u_vs", False)
        subsystem.set_lod_build_settings(asset, 0, settings)
        options = unreal.GeometryScriptCopyMeshToAssetOptions()
        options.set_editor_property("replace_materials", True)
        options.set_editor_property("new_materials", [_get_tint_material()])
        options.set_editor_property("new_material_slot_names", ["Stone"])
        unreal.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(mesh, asset, options, unreal.GeometryScriptMeshWriteLOD())
        unreal.EditorAssetLibrary.set_metadata_tag(asset, "IWBVersion", "stone-1")
        unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
        meshes.append(asset)
    return meshes


def _trail_steps(points, rng, width):
    """Stepping stones along a trail: irregular gaps, sizes, and small sideways offsets."""
    steps, carry = [], 0.0
    gap = rng.uniform(0.75, 1.05) * width
    for (ax, ay), (bx, by) in zip(points, points[1:]):
        length = math.hypot(bx - ax, by - ay)
        if length < 1e-3:
            continue
        heading = math.atan2(by - ay, bx - ax)
        travelled = gap - carry
        while travelled <= length:
            t = travelled / length
            side = rng.uniform(-0.28, 0.28) * width
            x = ax + (bx - ax) * t - math.sin(heading) * side
            y = ay + (by - ay) * t + math.cos(heading) * side
            steps.append((x, y, heading, rng.uniform(0.38, 0.62) * width))
            # Now and then a small companion stone beside the main one.
            if rng.random() < 0.22:
                other = -1.0 if side > 0 else 1.0
                steps.append((x - math.sin(heading) * other * width * 0.42, y + math.cos(heading) * other * width * 0.42,
                              heading + rng.uniform(-1.0, 1.0), rng.uniform(0.2, 0.32) * width))
            gap = rng.uniform(0.75, 1.05) * width
            travelled += gap
        carry = length - (travelled - gap)
    return steps


def _build_base(data, tag, origin_x):
    cube = unreal.load_asset("/Engine/BasicShapes/Cube")
    cylinder = unreal.load_asset("/Engine/BasicShapes/Cylinder")
    terrain = data["terrain"]
    palette = _palette(data)
    features = _terrain_features(data)
    ground = _spawn_mesh(
        _ground_mesh(features, terrain, tag), unreal.Vector(origin_x, 0, GROUND_SURFACE_Z), unreal.Vector(1, 1, 1),
        unreal.Rotator(), tag, TERRAIN_TAG, "IWB_Ground", paint=("ground", None, None),
    )
    _paint_ground(ground, palette)
    _enable_world_collision(ground)
    # A distance field this large is very coarse: it printed big, pixelated
    # black blotches across the meadows.  Regular shadows stay on.
    component = ground.get_component_by_class(unreal.StaticMeshComponent)
    for prop, value in (("affect_distance_field_lighting", False), ("affect_dynamic_indirect_lighting", True)):
        try:
            component.set_editor_property(prop, value)
        except Exception:
            pass
    # Footpaths are stepping stones: flat, irregular stones of different
    # sizes with grass between them, following the trail and lying on the
    # slope.  They belong to the ground layer so New plants never duplicates them.
    rng = random.Random(data["seed"] + 77)
    batches = _Batches(tag)
    stones = _path_stone_meshes()
    for trail in _trail_network(data["seed"], terrain, features, data.get("paths", "Network")):
        for x, y, heading, size in _trail_steps(trail["points"], rng, trail["width"]):
            yaw = heading + rng.uniform(-0.6, 0.6)
            normal = _ground_normal(x, y, features)
            along = (math.cos(yaw), math.sin(yaw), 0.0)
            along = _normalize(tuple(along[i] - normal[i] * _dot(along, normal) for i in range(3)))
            batches.add(stones[rng.randrange(len(stones))], unreal.Transform(
                location=unreal.Vector(origin_x + x, y, _ground_height(x, y, features) - 2.0),
                rotation=_rotator_from_axes(along, normal),
                scale=unreal.Vector(size / 100.0, size / 100.0, rng.uniform(0.8, 1.3)),
            ), PATH_TAG, "IWB_Path", _paint(palette, "path_stone"), solid=False)
    # An invisible ring keeps the player inside the world, even where the
    # mountains are switched off.
    radius, segments = terrain * 0.5 * 0.975, 40
    length = math.tau * radius / segments * 1.08
    for index in range(segments):
        angle = math.tau * index / segments
        batches.add(cube, unreal.Transform(
            location=unreal.Vector(origin_x + math.cos(angle) * radius, math.sin(angle) * radius, 2000.0),
            rotation=_rot(math.degrees(angle) + 90.0),
            scale=unreal.Vector(length / 100.0, 1.2, 40.0),
        ), TERRAIN_TAG, "IWB_WorldBoundary", solid=True, visible=False)
    batches.flush()
    return features


def _build_mountains(data, tag, origin_x, terrain_features):
    """A continuous, solid mountain range around the world: no gaps to slip through."""
    # The ranges are sculpted in the ground itself now (see _terrain_features);
    # loose crags on their steep slopes only looked like floating boulders.
    if True:
        return 0
    front_meshes = _kenney_meshes(("cliff_large_rock", "cliff_top_rock", "cliff_large_rock"))
    # Kenney's big boulders, scaled up, read as low-poly peaks with real
    # silhouettes; the flat cliff blocks behind the wall looked like buildings.
    back_meshes = _kenney_meshes(("rock_largeA", "rock_largeB", "rock_largeD", "rock_largeE", "rock_largeF"))
    if not front_meshes:
        return 0
    _ensure_collision(set(front_meshes + back_meshes))
    rng, terrain, palette = random.Random(data["seed"] + 415), data["terrain"], _palette(data)
    half, total = terrain * 0.5, 0
    batches = _Batches(tag)
    # A lower front wall and a taller range behind it.  Each piece is a long
    # wall turned along the rim, and the next one starts before it ends.
    peak_width = half * 0.2
    # The ranges themselves are sculpted in the ground; these are only scattered
    # low-poly crags on their slopes, never a continuous wall.
    for ring_radius, low, high, meshes in ((1.05, 600.0, 1500.0, back_meshes or front_meshes),):
        angle = rng.uniform(0.0, 0.2)
        stop = angle + math.tau
        while angle < stop and total < 420:
            mesh = meshes[rng.randrange(len(meshes))]
            extent = mesh.get_bounds().box_extent
            height = rng.uniform(low, high)
            peak = meshes is back_meshes
            width = peak_width if peak else extent.x * 2.0 * height / max(1.0, extent.z * 2.0)
            radius = half * ring_radius * (1.0 + rng.uniform(-0.18, 0.22))
            x, y = math.cos(angle) * radius, math.sin(angle) * radius
            _, sink = _surface_placement(x, y, terrain_features, 0.0, height * 0.3)
            yaw = rng.uniform(0.0, 360.0) if peak else math.degrees(angle) + 90.0 + rng.uniform(-6.0, 6.0)
            batches.add(mesh, _prop_transform(
                mesh, x, y, origin_x, terrain_features, height, yaw, sink=8.0 + sink,
                max_width=peak_width if peak else None,
            # The tall peaks behind the wall stay lit but cast no shadow: with
            # a low evening sun they would otherwise darken the whole forest.
            ), MOUNTAIN_TAG, "IWB_Cliff", _paint(palette, "mountain", stem="grass"), solid=True, shadow=not peak)
            angle += width * rng.uniform(1.6, 3.2) / radius
            total += 1
    batches.flush()
    return total


def _reserve_existing_cliffs(footprints, tag, origin_x):
    """Plants must not grow inside the rim cliffs, including on Refresh Plants."""
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if not (_has_tag(actor, tag) and _has_tag(actor, MOUNTAIN_TAG) and actor.get_actor_label().startswith("IWB_Cliff")):
            continue
        for component in actor.get_components_by_class(unreal.InstancedStaticMeshComponent):
            if not component.static_mesh:
                continue
            extent = component.static_mesh.get_bounds().box_extent
            for index in range(component.get_instance_count()):
                transform = component.get_instance_transform(index, True)
                radius = max(extent.x * transform.scale3d.x, extent.y * transform.scale3d.y) * 0.8
                footprints.reserve(transform.translation.x - origin_x, transform.translation.y, radius)


def _village_meshes():
    """Only complete, standalone pieces from the Town Kit (never wall or fountain fragments)."""
    wanted = ("lantern",)
    return [mesh for mesh in _fantasy_town_meshes() if mesh.get_name().lower() in wanted]


def _smoothstep(edge0, edge1, value):
    t = max(0.0, min(1.0, (value - edge0) / (edge1 - edge0)))
    return t * t * (3.0 - 2.0 * t)


def _ecology(seed, terrain, cover=50.0):
    """Broad, overlapping density fields instead of isolated groves.

    forest is high in woodland and low in glades, but never zero: glades keep
    scattered lone trees, and every layer of ground cover has a floor so no
    part of the world is ever bare.
    """
    # Forest cover shifts the woodland threshold: more cover, wider woods.
    shift = (cover / 100.0 - 0.5) * 0.5

    def forest(x, y):
        return _smoothstep(0.34 - shift, 0.66 - shift, _terrain_noise(x, y, seed + 11, terrain * 0.20))

    def noise(offset, scale):
        return lambda x, y: _terrain_noise(x, y, seed + offset, terrain * scale)

    grass_noise, pebble_noise, flower_noise, rock_noise = noise(23, 0.06), noise(31, 0.05), noise(47, 0.07), noise(41, 0.09)
    return {
        "tree": lambda x, y: 0.10 + 0.90 * forest(x, y),
        # Shrubs and young trees gather where wood meets glade.
        "edge": lambda x, y: 0.15 + 0.85 * max(0.0, 1.0 - abs(forest(x, y) - 0.5) / 0.32),
        "floor": lambda x, y: 0.12 + 0.88 * forest(x, y),
        "grass": lambda x, y: 0.45 + 0.55 * grass_noise(x, y),
        "pebble": lambda x, y: 0.30 + 0.70 * pebble_noise(x, y),
        "flower": lambda x, y: 0.18 + 0.82 * (1.0 - forest(x, y)) * _smoothstep(0.35, 0.7, flower_noise(x, y)),
        "rock": lambda x, y: 0.25 + 0.75 * rock_noise(x, y),
        "forest": forest,
    }


def _candidates(rng, radius, weight, limit):
    """Uniform points over the playable disc, kept with the probability weight(x, y)."""
    for _ in range(limit):
        distance = radius * math.sqrt(rng.random())
        angle = rng.uniform(0.0, math.tau)
        x, y = math.cos(angle) * distance, math.sin(angle) * distance
        if rng.random() < weight(x, y):
            yield x, y


def _spawn_instances(mesh, transforms, generated_tag, layer, label, tint=None, trunk_tint=None, paint=None, stem_tint=None, solid=False, visible=True, shadow=True):
    """One actor holding many copies of a mesh.

    The level stores one file per actor, so a thousand separate props made
    every save write a thousand files.  Instances keep the world to about a
    hundred actors; solid instances still block the player one by one.
    """
    if not transforms or not isinstance(mesh, unreal.StaticMesh):
        return None
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, 0), unreal.Rotator())
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    handles = subsystem.k2_gather_subobject_data_for_instance(actor)
    handle, _ = subsystem.add_new_subobject(unreal.AddNewSubobjectParams(
        parent_handle=handles[0], new_class=unreal.HierarchicalInstancedStaticMeshComponent, blueprint_context=None))
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    instances = library.get_object(library.get_data(handle))
    instances.set_static_mesh(mesh)
    if solid:
        instances.set_collision_profile_name("BlockAll")
        instances.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS)
    else:
        instances.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    if not visible:
        instances.set_visibility(False, True)
    # Ground cover is thousands of tiny meshes: with a shadow each and no
    # draw distance, they made Play mode crawl.  They fade out with distance
    # (start, end in cm) and cast no shadow; nobody sees a pebble's shadow.
    fade = next((distances for key, distances in GROUND_COVER_FADE if key in label), None)
    if fade:
        shadow = False
        if fade[1] > 0:
            instances.set_cull_distances(fade[0], fade[1])
    if not shadow:
        instances.set_cast_shadow(False)
    instances.add_instances(transforms, False, True)
    _tag(actor, generated_tag, layer, label, paint)
    _apply_mesh_tint(actor, tint, trunk_tint, stem_tint)
    return actor


class _Batches:
    """Collects props by mesh and paint, then spawns one instanced actor per group."""

    def __init__(self, generated_tag):
        self.tag, self.groups = generated_tag, {}

    def add(self, mesh, transform, layer, label, paint_args=None, solid=True, visible=True, shadow=True):
        if transform is None:
            return False
        paint_args = paint_args or {}
        key = (mesh.get_path_name(), layer, label, paint_args.get("paint"), solid, visible, shadow)
        group = self.groups.setdefault(key, {
            "mesh": mesh, "layer": layer, "label": label, "paint": paint_args,
            "solid": solid, "visible": visible, "shadow": shadow, "transforms": [],
        })
        group["transforms"].append(transform)
        return True

    def flush(self):
        actors = []
        for group in self.groups.values():
            actors.append(_spawn_instances(
                group["mesh"], group["transforms"], self.tag, group["layer"], group["label"],
                solid=group["solid"], visible=group["visible"], shadow=group["shadow"], **group["paint"],
            ))
        self.groups = {}
        return actors


def _prop_transform(mesh, x, y, origin_x, features, height, yaw, normal=None, sink=0.0, max_width=None):
    """Where an instanced prop goes: its base centred on (x, y), resting on the ground.

    Same rules as _spawn_grounded_mesh, computed from the mesh bounds instead
    of from a spawned actor: scale to a height, turn by yaw, lean onto the
    normal around the base, then sink slightly so no edge floats.
    """
    if not isinstance(mesh, unreal.StaticMesh):
        return None
    bounds = mesh.get_bounds()
    extent, centre = bounds.box_extent, bounds.origin
    if extent.z * 2.0 < 0.08 * max(extent.x, extent.y, 1.0):
        unreal.log_warning("World Builder skipped the flat mesh {}".format(mesh.get_name()))
        return None
    scale = height / (extent.z * 2.0)
    scale_xy = min(scale, max_width / (2.0 * max(extent.x, extent.y, 1.0))) if max_width else scale
    # Base centre in the mesh's own space, then turned by the yaw.
    bx, by, bz = centre.x * scale_xy, centre.y * scale_xy, (centre.z - extent.z) * scale
    c, s_ = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    base = (bx * c - by * s_, bx * s_ + by * c, bz)
    heading = (c, s_, 0.0)
    up = normal or (0.0, 0.0, 1.0)
    axis, angle = _tilt_axis_angle(up)
    if axis is not None:
        base = _rotate_about(base, axis, angle)
        heading = _rotate_about(heading, axis, angle)
    ground_z = _ground_height(x, y, features) - sink
    return unreal.Transform(
        location=unreal.Vector(origin_x + x - base[0], y - base[1], ground_z - base[2]),
        rotation=_rotator_from_axes(heading, up),
        scale=unreal.Vector(scale_xy, scale_xy, scale),
    )


def _instance_transform(mesh, x, y, origin_x, features, scale, yaw, follow, max_tilt, sink):
    """Ground one instance on the knolls, leaning with the slope like the props do."""
    bounds = mesh.get_bounds()
    base = (bounds.origin.z - bounds.box_extent.z) * scale
    normal, slope_sink = _surface_placement(x, y, features, follow, max(bounds.box_extent.x, bounds.box_extent.y) * scale, max_tilt)
    axis, angle = _tilt_axis_angle(normal)
    heading = (math.cos(math.radians(yaw)), math.sin(math.radians(yaw)), 0.0)
    if axis is not None:
        heading = _rotate_about(heading, axis, angle)
    ground_z = _ground_height(x, y, features) - sink - slope_sink
    return unreal.Transform(
        location=unreal.Vector(origin_x + x - normal[0] * base, y - normal[1] * base, ground_z - normal[2] * base),
        rotation=_rotator_from_axes(heading, normal),
        scale=unreal.Vector(scale, scale, scale),
    )


def _build_vegetation(data, tag, origin_x, terrain_features):
    rng, terrain = random.Random(data["seed"]), data["terrain"]
    palette = _palette(data)
    # Trails depend on the ground, not on the plant seed: Refresh Plants with
    # a new seed must still avoid the paths already laid on the ground.
    ground_seed = data.get("ground_seed", data["seed"])
    half = terrain * 0.5
    # Everything grows inside the mountain ring, never in the cliffs.
    radius = half * 0.80
    cube = unreal.load_asset("/Engine/BasicShapes/Cube")
    if not cube:
        raise RuntimeError("Unreal Engine basic-shape meshes are unavailable")
    season_trees = {
        "Spring": ("tree_default", "tree_oak", "tree_simple", "tree_blocks", "tree_cone", "tree_thin", "tree_plateau", "tree_pineRoundA"),
        "Summer": ("tree_detailed", "tree_oak", "tree_fat", "tree_default", "tree_plateau", "tree_tall", "tree_pineRoundA", "tree_pineTallB", "tree_pineDefaultA", "tree_cone"),
        "Autumn": ("tree_default_fall", "tree_detailed_fall", "tree_oak_fall", "tree_simple_fall", "tree_thin_fall", "tree_plateau_fall", "tree_fat_fall", "tree_cone_fall", "tree_blocks_fall", "tree_pineRoundA"),
        "Winter": ("tree_pineRoundA", "tree_pineRoundB", "tree_pineRoundC", "tree_pineRoundD", "tree_pineTallA", "tree_pineTallB", "tree_pineTallC", "tree_pineDefaultA", "tree_pineDefaultB"),
    }
    tree_meshes = _kenney_meshes(season_trees.get(data["theme"], season_trees["Summer"])) or _pcg_tree_meshes()
    young_meshes = _kenney_meshes(("tree_small", "tree_pineSmallA", "tree_pineSmallC", "tree_small_fall" if data["theme"] == "Autumn" else "tree_thin"))
    rock_meshes = _kenney_meshes(("rock_largeA", "rock_largeB", "rock_largeC", "rock_largeE", "rock_tallA", "rock_tallB", "rock_tallE", "rock_tallG"))
    small_rock_meshes = _kenney_meshes(("rock_smallA", "rock_smallB", "rock_smallD", "rock_smallE", "rock_smallG", "rock_smallH", "rock_smallI", "rock_smallFlatA", "rock_smallFlatB", "rock_smallFlatC"))
    pebble_meshes = _kenney_meshes(("stone_smallA", "stone_smallB", "stone_smallC", "stone_smallD", "stone_smallE", "stone_smallF", "stone_smallG", "stone_smallH", "stone_smallI", "stone_smallFlatA", "stone_smallFlatB", "stone_smallFlatC"))
    hilltop_meshes = _kenney_meshes(("stump_old", "stump_roundDetailed", "log_stack", "mushroom_redGroup", "mushroom_tanGroup"))
    floor_meshes = _kenney_meshes(("stump_round", "stump_oldTall", "stump_square", "log", "log_large"))
    mushroom_meshes = _kenney_meshes(("mushroom_red", "mushroom_tan", "mushroom_redTall", "mushroom_tanTall", "mushroom_redGroup", "mushroom_tanGroup"))
    village_meshes = _village_meshes()
    grass_meshes = _kenney_meshes(("grass_large", "grass_leafs", "grass_leafsLarge"))
    plant_meshes = _kenney_meshes(("plant_flatShort", "plant_flatTall"))
    flower_meshes = _kenney_meshes(("flower_purpleA", "flower_purpleB", "flower_purpleC", "flower_redA", "flower_redB", "flower_redC", "flower_yellowA", "flower_yellowB", "flower_yellowC"))
    bush_meshes = _kenney_meshes(("plant_bush", "plant_bushDetailed", "plant_bushLarge", "plant_bushSmall", "plant_bushTriangle", "plant_bushLargeTriangle"))
    # Trees, rocks, stumps, and lanterns are obstacles in Play mode.
    _ensure_collision(set(tree_meshes + young_meshes + rock_meshes + hilltop_meshes + floor_meshes + village_meshes))
    fields = _ecology(data["seed"], terrain, data.get("forest_cover", 50.0))
    counts = {key: data.get(key, 0) for key in ("trees", "shrubs", "rocks", "pebbles", "grass", "plants", "flowers", "details")}

    # Solid props claim a footprint; later layers sample around them instead
    # of growing through trunks, rocks, cliffs, trails, or each other.
    footprints = _Footprints()
    _reserve_existing_cliffs(footprints, tag, origin_x)
    trails = _trail_network(ground_seed, terrain, terrain_features, data.get("paths", "Network"))
    for trail in trails:
        for x, y in trail["points"]:
            footprints.reserve(x, y, trail["width"] * 0.62)
    start_x, start_y, _ = _trail_start(dict(data, seed=ground_seed), terrain_features)
    footprints.reserve(start_x, start_y, 260.0)
    accent_peaks = [index for index in range(len(terrain_features)) if terrain_features[index][3] >= 150.0] if hilltop_meshes else []
    for index in accent_peaks:
        footprints.reserve(terrain_features[index][0], terrain_features[index][1], 110.0)

    def ground_at(world_x, world_y):
        return _ground_height(world_x - origin_x, world_y, terrain_features)

    props = _Batches(tag)

    def place_prop(mesh, x, y, height, layer, label, paint_args, footprint, follow, max_tilt, sink, wobble=0.0, solid=True):
        if not footprints.is_free(x, y, footprint):
            return False
        normal, slope_sink = _surface_placement(x, y, terrain_features, follow, height * 0.4, max_tilt=max_tilt, rng=rng, wobble=wobble)
        transform = _prop_transform(mesh, x, y, origin_x, terrain_features, height, rng.uniform(0, 360), normal, sink + slope_sink)
        if not props.add(mesh, transform, layer, label, paint_args, solid=solid):
            return False
        footprints.reserve(x, y, footprint)
        return True

    # Trees: dense woods and open glades, with lone trees in the glades.
    trees = 0
    for x, y in _candidates(rng, radius, fields["tree"], counts["trees"] * 30):
        if trees >= counts["trees"]:
            break
        if _ground_slope(x, y, terrain_features) > 0.5:
            continue
        young = young_meshes and rng.random() < 0.18 + 0.4 * (1.0 - fields["forest"](x, y))
        meshes = young_meshes if young else tree_meshes
        height = rng.uniform(220.0, 380.0) if young else rng.uniform(400.0, 680.0)
        # White trunks dominate; a minority of black trunks breaks repetition.
        trunk = "trunk_dark" if rng.random() < 0.38 else "trunk_light"
        normal_follow = 0.15
        if place_prop(meshes[rng.randrange(len(meshes))], x, y, height, TREE_TAG, "IWB_StylizedTree",
                      _paint(palette, "leaf", trunk), height * 0.15, normal_follow, 6.0, 4.0):
            trees += 1

    # Shrubs gather along woodland edges.
    shrubs = 0
    for x, y in _candidates(rng, radius, fields["edge"], counts["shrubs"] * 30):
        if shrubs >= counts["shrubs"] or not bush_meshes:
            break
        height = rng.uniform(70.0, 160.0)
        if place_prop(bush_meshes[rng.randrange(len(bush_meshes))], x, y, height, GRASS_TAG, "IWB_SeasonalShrub",
                      _paint(palette, "accent_foliage"), height * 0.42, 0.7, 22.0, 3.0, solid=False):
            shrubs += 1

    # Rocks: a few small outcrops plus boulders scattered everywhere.
    rocks = 0
    outcrops = [(x, y) for x, y in _candidates(rng, radius, fields["rock"], 200)][:max(2, counts["rocks"] // 10)]
    for x, y in _candidates(rng, radius, fields["rock"], counts["rocks"] * 30):
        if rocks >= counts["rocks"] or not rock_meshes:
            break
        if outcrops and rng.random() < 0.45:
            center_x, center_y = outcrops[rng.randrange(len(outcrops))]
            x, y = center_x + rng.uniform(-300.0, 300.0), center_y + rng.uniform(-300.0, 300.0)
        height = rng.uniform(70.0, 240.0)
        if place_prop(rock_meshes[rng.randrange(len(rock_meshes))], x, y, height, ROCK_TAG, "IWB_Rock",
                      _paint(palette, "rock", stem="grass"), height * 0.35, 1.0, 30.0, height * 0.08, wobble=0.14):
            rocks += 1

    # Forest floor: old stumps and fallen logs under the trees.
    details = 0
    for x, y in _candidates(rng, radius, fields["floor"], counts["details"] * 30):
        if details >= counts["details"] or not floor_meshes:
            break
        height = rng.uniform(45.0, 95.0)
        if place_prop(floor_meshes[rng.randrange(len(floor_meshes))], x, y, height, LANDMARK_TAG, "IWB_ForestFloor",
                      _paint(palette, "trunk_dark" if rng.random() < 0.4 else "trunk_light"), height * 0.6, 0.9, 20.0, 3.0):
            details += 1

    props.flush()

    # Hilltops are deliberately spare, but never empty.  These landmarks make
    # the knolls feel authored and give the player visual destinations.
    hilltop_accents = 0
    for index in accent_peaks:
        x, y, _radius, peak = terrain_features[index]
        height = rng.uniform(105.0, 175.0)
        normal, sink = _surface_placement(x, y, terrain_features, 0.5, height * 0.4, max_tilt=12.0)
        _spawn_grounded_mesh(
            hilltop_meshes[index % len(hilltop_meshes)],
            unreal.Vector(origin_x + x, y, peak), height, _rot(rng.uniform(0, 360)),
            tag, LANDMARK_TAG, "IWB_HilltopLandmark", penetration=2.0 + sink, normal=normal, ground_at=ground_at,
            center_xy=True, **_paint(palette, "accent_foliage", "trunk_dark" if index % 3 == 0 else "trunk_light"),
        )
        hilltop_accents += 1

    # Lanterns line the main trail, alternating sides, like a path people use.
    lanterns = 0
    if village_meshes and trails:
        main = trails[0]
        spacing = max(1, int(1500.0 / (main["width"] * 0.5)))
        for index in range(spacing // 2, len(main["points"]) - 1, spacing):
            (x0, y0), (x1, y1) = main["points"][index], main["points"][index + 1]
            direction = math.atan2(y1 - y0, x1 - x0)
            side = 1.0 if lanterns % 2 else -1.0
            offset = main["width"] * 0.5 + 70.0
            x, y = x0 - math.sin(direction) * offset * side, y0 + math.cos(direction) * offset * side
            _spawn_grounded_mesh(
                village_meshes[0], unreal.Vector(origin_x + x, y, _ground_height(x, y, terrain_features)), 230.0,
                _rot(math.degrees(direction)), tag, LANDMARK_TAG, "IWB_Lantern",
                penetration=2.0, center_xy=True, ground_at=ground_at, **_paint(palette, "trunk_dark", stem="flower_b"),
            )
            footprints.reserve(x, y, 40.0)
            lanterns += 1

    # Ground cover: instanced, everywhere, so there is never a bare patch.
    # Each entry: meshes, count, density field, scale range, slope follow,
    # max tilt, sink, layer, label, paint, and an optional palette variant.
    layers = (
        # A fine carpet of small blades, with only a few big tufts for rhythm.
        (grass_meshes, round(counts["grass"] * 1.7), fields["grass"], (0.8, 1.5), 0.9, 25.0, 0.5, GRASS_TAG, "IWB_FineGrass", ("grass", None, None)),
        (grass_meshes, counts["grass"] // 6, fields["grass"], (2.2, 3.6), 0.9, 25.0, 1.0, GRASS_TAG, "IWB_Grass", ("grass", None, None)),
        (plant_meshes, counts["plants"], fields["floor"], (1.3, 2.4), 0.8, 20.0, 1.0, GRASS_TAG, "IWB_Plants", ("accent_foliage", None, None)),
        (small_rock_meshes, counts["pebbles"] // 3, fields["pebble"], (1.4, 2.6), 1.0, 30.0, 2.0, ROCK_TAG, "IWB_SmallRocks", ("rock", None, "grass")),
        (pebble_meshes, counts["pebbles"], fields["pebble"], (0.9, 1.9), 1.0, 30.0, 1.5, ROCK_TAG, "IWB_Pebbles", ("rock", None, None)),
        (mushroom_meshes, counts["details"] * 2, fields["floor"], (1.3, 2.3), 0.8, 15.0, 1.0, FLOWER_TAG, "IWB_Mushrooms", ("flower_b", None, None)),
        (flower_meshes, counts["flowers"], fields["flower"], (2.0, 3.4), 0.6, 15.0, 1.0, FLOWER_TAG, "IWB_Flower", ("flower_a", None, "grass")),
    )
    totals = {}
    for meshes, count, weight, (low, high), follow, max_tilt, sink, layer, label, paint_keys in layers:
        if not meshes or count <= 0:
            continue
        batches = {}
        placed = 0
        for x, y in _candidates(rng, radius, weight, count * 6):
            if placed >= count:
                break
            if not footprints.is_free(x, y, 18.0):
                continue
            mesh_index = rng.randrange(len(meshes))
            key = paint_keys[0]
            if label == "IWB_Flower" and rng.random() < 0.5:
                key = "flower_b"
            elif label in ("IWB_Grass", "IWB_FineGrass") and rng.random() < 0.4:
                key = "grass_dry"
            transform = _instance_transform(
                meshes[mesh_index], x, y, origin_x, terrain_features, rng.uniform(low, high), rng.uniform(0, 360), follow, max_tilt, sink,
            )
            batches.setdefault((mesh_index, key), []).append(transform)
            placed += 1
        for (mesh_index, key), transforms in batches.items():
            _spawn_instances(meshes[mesh_index], transforms, tag, layer, label, **_paint(palette, key, paint_keys[1], paint_keys[2]))
        totals[label] = placed
    try:
        animal_count, pickup_count = _build_gameplay(data, tag, origin_x, terrain_features, footprints, fields, rng)
        if animal_count:
            unreal.log("Interactive World Builder: {} animals and {} mushrooms to pick up.".format(animal_count, pickup_count))
    except Exception as error:
        # Gameplay is a layer on top of the world: never lose a build over it.
        unreal.log_warning("World Builder gameplay skipped: {}".format(error))
    grass = totals.get("IWB_Grass", 0) + totals.get("IWB_FineGrass", 0) + totals.get("IWB_Plants", 0)
    flowers = totals.get("IWB_Flower", 0) + totals.get("IWB_Mushrooms", 0)
    rocks += totals.get("IWB_SmallRocks", 0) + totals.get("IWB_Pebbles", 0)
    return trees, rocks, grass, flowers, shrubs + details, hilltop_accents, lanterns


WEATHER_LOOKS = {
    # drops, box size (cm), fall speed (cm/s), sway, colour, opacity, (width, height) of one drop
    "Rain": (9000, 3000.0, 950.0, 6.0, (0.62, 0.70, 0.82), 0.2, (1.0, 60.0)),
    "Snow": (7000, 3000.0, 110.0, 45.0, (1.0, 1.0, 1.0), 0.9, (5.0, 5.0)),
}
WEATHER_SURFACES = {"Clear": (0.0, 0.0), "Rain": (0.0, 1.0), "Snow": (1.0, 0.0)}


def _precipitation_material():
    """Unlit drops that always fall around the camera, wherever the player walks.

    Each drop stores its own centre in UV1/UV2.  The material moves that centre
    with time and wraps it inside a box around the camera, so a single small
    mesh is enough for endless rain or snow, in the editor and in Play.
    """
    path = "/Game/WorldBuilder/Materials/M_IWB_Precipitation"
    material = unreal.load_asset(path)
    if material and _material_is_current(material):
        return material
    lib = unreal.MaterialEditingLibrary
    if not material:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "M_IWB_Precipitation", "/Game/WorldBuilder/Materials", unreal.Material, unreal.MaterialFactoryNew())
    lib.delete_all_material_expressions(material)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)

    def node(cls, x, y, **props):
        item = lib.create_material_expression(material, cls, x, y)
        for key, value in props.items():
            item.set_editor_property(key, value)
        return item

    def param(name, x, y, default):
        return node(unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=default)

    colour = node(unreal.MaterialExpressionVectorParameter, -300, -200, parameter_name="Color",
                  default_value=unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    lib.connect_material_property(colour, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    lib.connect_material_property(param("Opacity", -300, -60, 0.5), "", unreal.MaterialProperty.MP_OPACITY)
    uv1 = node(unreal.MaterialExpressionTextureCoordinate, -1600, 100, coordinate_index=1)
    uv2 = node(unreal.MaterialExpressionTextureCoordinate, -1600, 260, coordinate_index=2)
    height = node(unreal.MaterialExpressionComponentMask, -1440, 220, r=True, g=False, b=False, a=False)
    lib.connect_material_expressions(uv2, "", height, "")
    phase = node(unreal.MaterialExpressionComponentMask, -1440, 330, r=False, g=True, b=False, a=False)
    lib.connect_material_expressions(uv2, "", phase, "")
    pivot = node(unreal.MaterialExpressionAppendVector, -1280, 140)
    lib.connect_material_expressions(uv1, "", pivot, "A")
    lib.connect_material_expressions(height, "", pivot, "B")
    time = node(unreal.MaterialExpressionTime, -1600, 460)
    fall = node(unreal.MaterialExpressionMultiply, -1440, 460)
    lib.connect_material_expressions(time, "", fall, "A")
    lib.connect_material_expressions(param("FallSpeed", -1600, 540, 900.0), "", fall, "B")
    wobble_time = node(unreal.MaterialExpressionMultiply, -1440, 620, const_b=0.35)
    lib.connect_material_expressions(time, "", wobble_time, "A")
    wobble_phase = node(unreal.MaterialExpressionAdd, -1300, 620)
    lib.connect_material_expressions(wobble_time, "", wobble_phase, "A")
    lib.connect_material_expressions(phase, "", wobble_phase, "B")
    wave = node(unreal.MaterialExpressionSine, -1170, 620)
    lib.connect_material_expressions(wobble_phase, "", wave, "")
    sway = node(unreal.MaterialExpressionMultiply, -1040, 620)
    lib.connect_material_expressions(wave, "", sway, "A")
    lib.connect_material_expressions(param("Sway", -1170, 720, 0.0), "", sway, "B")
    zero = node(unreal.MaterialExpressionConstant, -1040, 520, r=0.0)
    drift_xy = node(unreal.MaterialExpressionAppendVector, -900, 560)
    lib.connect_material_expressions(sway, "", drift_xy, "A")
    lib.connect_material_expressions(zero, "", drift_xy, "B")
    drift = node(unreal.MaterialExpressionAppendVector, -780, 520)
    lib.connect_material_expressions(drift_xy, "", drift, "A")
    lib.connect_material_expressions(fall, "", drift, "B")
    camera = node(unreal.MaterialExpressionCameraPositionWS, -1280, 0)
    relative = node(unreal.MaterialExpressionSubtract, -1100, 80)
    lib.connect_material_expressions(pivot, "", relative, "A")
    lib.connect_material_expressions(camera, "", relative, "B")
    moved = node(unreal.MaterialExpressionSubtract, -640, 200)
    lib.connect_material_expressions(relative, "", moved, "A")
    lib.connect_material_expressions(drift, "", moved, "B")
    box = param("Box", -640, 360, 3000.0)
    scaled = node(unreal.MaterialExpressionDivide, -500, 200)
    lib.connect_material_expressions(moved, "", scaled, "A")
    lib.connect_material_expressions(box, "", scaled, "B")
    centred = node(unreal.MaterialExpressionAdd, -380, 200, const_b=0.5)
    lib.connect_material_expressions(scaled, "", centred, "A")
    wrapped = node(unreal.MaterialExpressionFrac, -260, 200)
    lib.connect_material_expressions(centred, "", wrapped, "")
    back = node(unreal.MaterialExpressionSubtract, -150, 200, const_b=0.5)
    lib.connect_material_expressions(wrapped, "", back, "A")
    sized = node(unreal.MaterialExpressionMultiply, -40, 200)
    lib.connect_material_expressions(back, "", sized, "A")
    lib.connect_material_expressions(box, "", sized, "B")
    target = node(unreal.MaterialExpressionAdd, 80, 120)
    lib.connect_material_expressions(camera, "", target, "A")
    lib.connect_material_expressions(sized, "", target, "B")
    offset = node(unreal.MaterialExpressionSubtract, 200, 160)
    lib.connect_material_expressions(target, "", offset, "A")
    lib.connect_material_expressions(pivot, "", offset, "B")
    lib.connect_material_property(offset, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    _finish_material(material)
    return material


def _precipitation_mesh(weather):
    """Thousands of crossed quads (rain streaks or snowflakes) in one static mesh."""
    path = "/Game/WorldBuilder/Meshes/SM_IWB_" + weather
    count, box, _speed, _sway, _colour, _opacity, (width, height) = WEATHER_LOOKS[weather]
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        asset = unreal.load_asset(path)
        if asset and unreal.EditorAssetLibrary.get_metadata_tag(asset, "IWBVersion") == MATERIAL_VERSION:
            return asset
    else:
        unreal.EditorAssetLibrary.duplicate_asset("/Engine/BasicShapes/Plane", path)
    asset = unreal.load_asset(path)
    rng = random.Random(4242 if weather == "Rain" else 777)
    vertices, triangles, normals, uv0, uv1, uv2 = [], [], [], [], [], []
    half_w, half_h = width * 0.5, height * 0.5
    for _ in range(count):
        px, py, pz = (rng.uniform(-box * 0.5, box * 0.5) for _axis in range(3))
        phase = rng.uniform(0.0, 6.28)
        angle = rng.uniform(0.0, math.pi)
        for turn in (0.0, math.pi * 0.5):
            dx, dy = math.cos(angle + turn) * half_w, math.sin(angle + turn) * half_w
            base = len(vertices)
            for (sx, sz), (u, v) in (((-1, -1), (0, 1)), ((1, -1), (1, 1)), ((1, 1), (1, 0)), ((-1, 1), (0, 0))):
                vertices.append(unreal.Vector(px + dx * sx, py + dy * sx, pz + half_h * sz))
                normals.append(unreal.Vector(-dy, dx, 0.0))
                uv0.append(unreal.Vector2D(u, v))
                uv1.append(unreal.Vector2D(px, py))
                uv2.append(unreal.Vector2D(pz, phase))
            triangles.append(unreal.IntVector(base, base + 1, base + 2))
            triangles.append(unreal.IntVector(base, base + 2, base + 3))
    buffers = unreal.GeometryScriptSimpleMeshBuffers()
    for name, values in (("vertices", vertices), ("normals", normals), ("triangles", triangles), ("uv0", uv0), ("uv1", uv1), ("uv2", uv2)):
        buffers.set_editor_property(name, values)
    mesh = unreal.DynamicMesh()
    mesh.append_buffers_to_mesh(buffers)
    subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    try:
        settings = subsystem.get_lod_build_settings(asset, 0)
        for prop, value in (("generate_lightmap_u_vs", False), ("recompute_normals", False), ("use_full_precision_u_vs", True)):
            settings.set_editor_property(prop, value)
        subsystem.set_lod_build_settings(asset, 0, settings)
    except Exception as error:
        unreal.log_warning("World Builder weather mesh settings skipped: {}".format(error))
    options = unreal.GeometryScriptCopyMeshToAssetOptions()
    options.set_editor_property("enable_recompute_normals", False)
    options.set_editor_property("replace_materials", True)
    options.set_editor_property("new_materials", [_precipitation_material()])
    options.set_editor_property("new_material_slot_names", ["Weather"])
    unreal.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(mesh, asset, options, unreal.GeometryScriptMeshWriteLOD())
    body = asset.get_editor_property("body_setup")
    body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
    unreal.EditorAssetLibrary.set_metadata_tag(asset, "IWBVersion", MATERIAL_VERSION)
    unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
    return asset


def _build_weather(data, tag, origin_x):
    """Falling rain or snow around the camera, plus wet or snowy surfaces."""
    weather = data.get("weather", "Clear")
    if tag == GENERATED_TAG:
        _set_weather_values(*WEATHER_SURFACES.get(weather, (0.0, 0.0)))
    if weather not in WEATHER_LOOKS:
        return 0
    try:
        count, box, speed, sway, colour, opacity, _size = WEATHER_LOOKS[weather]
        actor = _spawn_mesh(_precipitation_mesh(weather), unreal.Vector(0, 0, 0), unreal.Vector(1, 1, 1),
                            unreal.Rotator(), tag, WEATHER_TAG, "IWB_" + weather)
        component = actor.get_component_by_class(unreal.StaticMeshComponent)
        component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        component.set_editor_property("cast_shadow", False)
        # The drops are moved to the camera by the material: never cull the mesh.
        component.set_editor_property("bounds_scale", 200.0)
        material = component.create_dynamic_material_instance(0)
        if material:
            material.set_vector_parameter_value("Color", unreal.LinearColor(colour[0], colour[1], colour[2], 1.0))
            for name, value in (("Opacity", opacity), ("FallSpeed", speed), ("Sway", sway), ("Box", box)):
                material.set_scalar_parameter_value(name, value)
        return count
    except Exception as error:
        # Weather is optional: do not lose a generated world if a material fails.
        unreal.log_warning("World Builder weather skipped: {}".format(error))
        return 0


def _template_state_path():
    return os.path.join(unreal.Paths.project_saved_dir(), TEMPLATE_STATE_FILENAME)


def _light_state(actor, component_class):
    component = actor.get_component_by_class(component_class)
    if not component:
        return {}
    color = component.get_editor_property("light_color")
    return {
        "intensity": component.get_editor_property("intensity"),
        "color": [color.r, color.g, color.b, color.a],
    }


def _transform_state(actor):
    location, rotation = actor.get_actor_location(), actor.get_actor_rotation()
    return {
        "location": [location.x, location.y, location.z],
        "rotation": [rotation.pitch, rotation.yaw, rotation.roll],
    }


def _save_template_state(actors):
    """Snapshot the template sun, skylight, and player starts the generator edits.

    Each actor is recorded once, the first time it is seen: later builds
    would otherwise record the already-tinted values as the "original" level.
    """
    state = {}
    if os.path.isfile(_template_state_path()):
        try:
            with open(_template_state_path(), "r", encoding="utf-8") as handle:
                state = json.load(handle)
        except (OSError, ValueError):
            state = {}
    for actor in actors:
        if actor.get_path_name() in state:
            continue
        if _has_tag(actor, GENERATED_TAG):
            continue
        try:
            if isinstance(actor, unreal.DirectionalLight):
                state[actor.get_path_name()] = dict(_transform_state(actor), **_light_state(actor, unreal.DirectionalLightComponent))
            elif isinstance(actor, unreal.SkyLight):
                state[actor.get_path_name()] = _light_state(actor, unreal.SkyLightComponent)
            elif isinstance(actor, unreal.PlayerStart):
                state[actor.get_path_name()] = _transform_state(actor)
            elif isinstance(actor, unreal.ExponentialHeightFog):
                fog = actor.get_component_by_class(unreal.ExponentialHeightFogComponent)
                colour = _get_first(fog, FOG_COLOR_PROPERTIES)
                sun = _get_first(fog, FOG_SUN_PROPERTIES)
                state[actor.get_path_name()] = {"fog": {
                    "density": fog.get_editor_property("fog_density"),
                    "color": [colour.r, colour.g, colour.b] if colour else None,
                    "sun": [sun.r, sun.g, sun.b] if sun else None,
                    "volumetric": bool(_get_first(fog, ("volumetric_fog",))),
                }}
        except Exception as error:
            unreal.log_warning("World Builder could not record {}: {}".format(actor.get_name(), error))
    try:
        with open(_template_state_path(), "w", encoding="utf-8") as handle:
            json.dump(state, handle, indent=2)
    except OSError as error:
        unreal.log_warning("World Builder could not save the template state: {}".format(error))


def _restore_template_state():
    """Put the template sun, skylight, and player starts back as they were."""
    if not os.path.isfile(_template_state_path()):
        return 0
    try:
        with open(_template_state_path(), "r", encoding="utf-8") as handle:
            state = json.load(handle)
    except (OSError, ValueError) as error:
        unreal.log_warning("World Builder could not read the template state: {}".format(error))
        return 0
    restored = 0
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        saved = state.get(actor.get_path_name())
        if not saved:
            continue
        try:
            if "fog" in saved:
                fog = actor.get_component_by_class(unreal.ExponentialHeightFogComponent)
                _set_first(fog, ("fog_density",), saved["fog"]["density"])
                if saved["fog"]["color"]:
                    _set_first(fog, FOG_COLOR_PROPERTIES, unreal.LinearColor(*saved["fog"]["color"], 1.0))
                if saved["fog"]["sun"]:
                    _set_first(fog, FOG_SUN_PROPERTIES, unreal.LinearColor(*saved["fog"]["sun"], 1.0))
                _set_first(fog, ("volumetric_fog",), saved["fog"]["volumetric"])
                restored += 1
                continue
            if "location" in saved:
                actor.set_actor_location(unreal.Vector(*saved["location"]), False, False)
                actor.set_actor_rotation(_rot(pitch=saved["rotation"][0], yaw=saved["rotation"][1], roll=saved["rotation"][2]), False)
            component_class = unreal.DirectionalLightComponent if isinstance(actor, unreal.DirectionalLight) else unreal.SkyLightComponent
            component = actor.get_component_by_class(component_class) if "intensity" in saved else None
            if component:
                r, g, b, a = saved["color"]
                component.set_editor_property("light_color", unreal.Color(r=r, g=g, b=b, a=a))
                # set_intensity also refreshes the render state for the color above.
                component.set_intensity(saved["intensity"])
                if isinstance(component, unreal.SkyLightComponent):
                    component.recapture_sky()
            restored += 1
        except Exception as error:
            unreal.log_warning("World Builder could not restore {}: {}".format(actor.get_name(), error))
    try:
        os.remove(_template_state_path())
    except OSError:
        pass
    return restored


def _fog_component(actors):
    fog = next((actor for actor in actors if isinstance(actor, unreal.ExponentialHeightFog)), None)
    return fog.get_component_by_class(unreal.ExponentialHeightFogComponent) if fog else None


def _set_first(component, names, value):
    """Set the first property name this engine version knows (some were renamed)."""
    for name in names:
        try:
            component.set_editor_property(name, value)
            return True
        except Exception:
            continue
    return False


def _get_first(component, names):
    for name in names:
        try:
            return component.get_editor_property(name)
        except Exception:
            continue
    return None


FOG_COLOR_PROPERTIES = ("fog_inscattering_luminance", "fog_inscattering_color")
FOG_SUN_PROPERTIES = ("directional_inscattering_luminance", "directional_inscattering_color")


def _apply_haze(actors, time, sun_color, haze_amount=50.0):
    """A warm, light haze: depth for the mountains, and volumetric fog for light shafts."""
    fog = _fog_component(actors)
    if not fog:
        return
    haze = time["haze"]
    _set_first(fog, ("fog_density",), time["fog"] * (0.1 + 1.8 * haze_amount / 100.0))
    _set_first(fog, FOG_COLOR_PROPERTIES, unreal.LinearColor(haze[0], haze[1], haze[2], 1.0))
    _set_first(fog, FOG_SUN_PROPERTIES, unreal.LinearColor(sun_color.r * 0.6, sun_color.g * 0.6, sun_color.b * 0.6, 1.0))
    _set_first(fog, ("volumetric_fog",), True)
    _set_first(fog, ("volumetric_fog_scattering_distribution",), 0.7)
    _set_first(fog, ("volumetric_fog_extinction_scale",), 0.6)
    try:
        fog.mark_render_state_dirty()
    except Exception:
        pass


def _apply_lighting(data, tag, origin_x):
    """Sun and sky follow the palette and Mood sliders (also used by palette recolor)."""
    palette = _palette(data)
    sky_base, sun_base = palette["sky"], palette["sun"]
    sky_color = unreal.LinearColor(
        min(1.0, sky_base.r + data["sky_tint"] * 0.14),
        min(1.0, sky_base.g + data["sky_tint"] * 0.12),
        min(1.0, sky_base.b + data["sky_tint"] * 0.08),
        1.0,
    )
    light_color = unreal.LinearColor(
        min(1.0, sun_base.r),
        min(1.0, sun_base.g + data["light_tint"] * 0.08),
        min(1.0, sun_base.b + data["light_tint"] * 0.12),
        1.0,
    )
    # Third Person already owns a sun and a skylight. Reuse those instead of
    # adding a second pair, which caused the "multiple directional lights" warning.
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    # Record the template's own light and start once, before the first edit,
    # so Delete World can put the original level back exactly.
    _save_template_state(actors)
    light = next((actor for actor in actors if isinstance(actor, unreal.DirectionalLight)), None)
    if not light:
        light = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(origin_x, 0, 1100), _rot(yaw=-28, pitch=-52))
        _tag(light, tag, LIGHT_TAG, "IWB_ForestLight")
    light_component = light.get_component_by_class(unreal.DirectionalLightComponent)
    if light_component:
        time = TIMES_OF_DAY.get(data.get("time_of_day", DEFAULT_TIME_OF_DAY), TIMES_OF_DAY[DEFAULT_TIME_OF_DAY])
        # The palette's sun, warmed or cooled by the hour.
        light_color = unreal.LinearColor(
            light_color.r * 0.5 + time["sun"][0] * 0.5, light_color.g * 0.5 + time["sun"][1] * 0.5,
            light_color.b * 0.5 + time["sun"][2] * 0.5, 1.0)
        # Rain greys and dims the light and thickens the air; snow makes it cold and pale.
        look = WEATHER_LIGHT.get(data.get("weather", "Clear"))
        if look:
            strength, sun_mix, sun, haze_mix, haze, fog = look
            light_color = unreal.LinearColor(*(getattr(light_color, c) * (1.0 - sun_mix) + sun[i] * sun_mix for i, c in enumerate("rgb")), 1.0)
            time = dict(time, strength=time["strength"] * strength, fog=time["fog"] * fog,
                        haze=tuple(time["haze"][i] * (1.0 - haze_mix) + haze[i] * haze_mix for i in range(3)))
        # UE directional lights use a small lux value here, not 10,000x the UI value.
        light_component.set_editor_property("intensity", (1.5 + data["lighting"] * 0.70) * time["strength"])
        light_component.set_light_color(light_color)
        light.set_actor_rotation(_rot(yaw=time["yaw"], pitch=time["pitch"]), False)
        # Soft god rays through the canopy when the sun is low.
        for prop, value in (("enable_light_shaft_bloom", True), ("bloom_scale", 0.12), ("bloom_threshold", 4.0)):
            try:
                light_component.set_editor_property(prop, value)
            except Exception:
                pass
        _apply_haze(actors, time, light_color, data.get("haze", 50.0))

    sky = next((actor for actor in actors if isinstance(actor, unreal.SkyLight)), None)
    if not sky:
        sky = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(origin_x, 0, 1050), unreal.Rotator())
        _tag(sky, tag, LIGHT_TAG, "IWB_ForestSky")
    sky_component = sky.get_component_by_class(unreal.SkyLightComponent)
    if sky_component:
        # Shade must keep its color: a stronger, paler sky light lifts the
        # pitch-black shadow sides that made the world hard to read.
        sky_component.set_editor_property("intensity", 1.2 + data["lighting"] * 0.12)
        sky_color = unreal.LinearColor(0.45 + sky_color.r * 0.55, 0.45 + sky_color.g * 0.55, 0.45 + sky_color.b * 0.55, 1.0)
        if data.get("weather") in WEATHER_LIGHT:
            grey = 0.62 if data["weather"] == "Rain" else 0.85
            sky_color = unreal.LinearColor((sky_color.r + grey) * 0.5, (sky_color.g + grey) * 0.5, (sky_color.b + grey * 1.08) * 0.5, 1.0)
        sky_component.set_light_color(sky_color)
        sky_component.recapture_sky()


def _build_lighting_and_player(data, tag, origin_x):
    _apply_lighting(data, tag, origin_x)
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    if data["playable_character"]:
        starts = [actor for actor in actors if isinstance(actor, unreal.PlayerStart)]
        if not starts:
            start = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(), unreal.Rotator())
            _tag(start, tag, PLAYER_TAG, "IWB_ThirdPersonStart")
            starts = [start]
        # The player begins at the head of the main trail, looking down it.
        x, y, yaw = _trail_start(data, _terrain_features(data))
        spawn_location = unreal.Vector(origin_x + x, y, _ground_height(x, y, _terrain_features(data)) + 112)
        for start in starts:
            start.set_actor_location(spawn_location, False, True)
            start.set_actor_rotation(_rot(yaw), False)


def _last_build_path():
    return os.path.join(unreal.Paths.project_saved_dir(), LAST_BUILD_FILENAME)


def _save_last_build(data):
    """Remember which seed and scale shaped the ground, for Refresh Plants."""
    try:
        with open(_last_build_path(), "w", encoding="utf-8") as handle:
            json.dump({"seed": data["seed"], "terrain": data["terrain"], "hills": data.get("hills", 50.0),
                       "paths": data.get("paths", "Network"), "mountains": bool(data.get("mountains", True))}, handle, indent=2)
    except OSError as error:
        unreal.log_warning("World Builder could not record the last build: {}".format(error))


def _load_last_build():
    try:
        with open(_last_build_path(), "r", encoding="utf-8") as handle:
            ground = json.load(handle)
        return {"seed": int(ground["seed"]), "terrain": float(ground["terrain"]),
                "hills": float(ground.get("hills", 50.0)), "paths": ground.get("paths", "Network"),
                "mountains": bool(ground.get("mountains", True))}
    except (OSError, ValueError, KeyError, TypeError):
        return None


def _build_world(widget, preview=False, vegetation_only=False, ground=None):
    data = _values(widget)
    if vegetation_only:
        # Plants must sit on the knolls that already exist, so the ground
        # shape comes from the last Build even if the scale slider has moved.
        # The current seed still drives the plant layout.
        data["terrain"] = ground["terrain"]
        data["ground_seed"] = ground["seed"]
        data["hills"], data["paths"] = ground["hills"], ground["paths"]
        data["mountains"] = ground.get("mountains", True)
        data.update(_vegetation_counts(data["vegetation"], ground["terrain"], data.get("forest_cover", 50.0)))
        data["seed"] = int(data["seed"]) + _plant_variation * 7919
        terrain_features = _terrain_features(ground)
    else:
        terrain_features = _terrain_features(data)
    # Keep Preview fully beside the active world, never overlapping its play space.
    # The ground mesh reaches two world sizes out on every side (the far ranges).
    tag, origin_x = (PREVIEW_TAG, data["terrain"] * 4.2) if preview else (GENERATED_TAG, 0.0)
    weather_count = 0
    if not vegetation_only:
        terrain_features = _build_base(data, tag, origin_x)
        _build_mountains(data, tag, origin_x, terrain_features)
        weather_count = _build_weather(data, tag, origin_x)
        try:
            _build_ambience(data, tag, origin_x)
        except Exception as error:
            unreal.log_warning("World Builder ambience skipped: {}".format(error))
    trees, rocks, grass, flowers, bushes, hilltop_accents, lanterns = _build_vegetation(data, tag, origin_x, terrain_features)
    if not preview and not vegetation_only:
        _build_lighting_and_player(data, tag, origin_x)
        _save_last_build(data)
    weather_label = "{} falling particles".format(weather_count) if weather_count else "clear sky"
    if vegetation_only:
        weather_label = "ground kept"
    if lanterns:
        weather_label += " | {} trail lanterns".format(lanterns)
    verb = "Preview" if preview else "Plants refreshed for" if vegetation_only else "Generated"
    _status(widget, "{} '{}' ({}, {}) | {} trees, {} shrubs, {} flowers, {} rocks, {} grass, {} hilltop landmarks.".format(verb, data["world_name"], data["theme"], weather_label, trees, bushes, flowers, rocks, grass, hilltop_accents))


def _animals_zip_path():
    return os.path.join(os.path.expanduser("~"), "Downloads", ANIMALS_ZIP_FILENAME)


def _animal_assets(name):
    """The imported skeletal mesh and its animations, keyed by animation name.

    Interchange names each glTF animation "<model><animation>", for example
    "animal-deerwalk" or "animal-deergesture-positive".
    """
    mesh = unreal.load_asset("{}/{}".format(ANIMALS_DESTINATION, name))
    animations = {}
    try:
        paths = unreal.EditorAssetLibrary.list_assets(ANIMALS_DESTINATION, recursive=False, include_folder=False)
    except Exception:
        paths = []
    for path in paths:
        asset = unreal.load_asset(path)
        asset_name = asset.get_name() if asset else ""
        if isinstance(asset, unreal.AnimSequence) and asset_name.lower().startswith(name.lower()):
            animations[asset_name[len(name):].lstrip("_").lower()] = asset
    return (mesh if isinstance(mesh, unreal.SkeletalMesh) else None), animations


def _import_animals(force=False):
    """Import the animated forest animals from the user's Kenney Cube Pets zip.

    The GLB files go through Interchange with an explicit pipeline: that keeps
    every animal as one skeletal mesh and brings all eight animations (the
    FBX importer only kept a 0.1 s pose, and the default glTF import split
    each animal into a body and four separate legs).
    """
    if not force and all(_animal_assets(name)[0] for name in ANIMAL_FILES):
        return True, "Animals are ready."
    archive_path = _animals_zip_path()
    if not os.path.isfile(archive_path):
        return False, "Kenney Cube Pets zip not found in Downloads."
    source_dir = os.path.join(unreal.Paths.project_intermediate_dir(), "WorldBuilderAnimalsGLB")
    os.makedirs(os.path.join(source_dir, "Textures"), exist_ok=True)
    try:
        with zipfile.ZipFile(archive_path, "r") as archive:
            # The GLB files point to a shared Textures/colormap.png next to them.
            members = [("Models/GLB format/{}.glb".format(name), name + ".glb") for name in ANIMAL_FILES]
            members.append(("Models/GLB format/Textures/colormap.png", os.path.join("Textures", "colormap.png")))
            for member, target in members:
                with archive.open(member) as source, open(os.path.join(source_dir, target), "wb") as handle:
                    handle.write(source.read())
    except (OSError, KeyError, zipfile.BadZipFile) as error:
        return False, "Could not unpack the animals: {}".format(error)
    manager = unreal.InterchangeManager.get_interchange_manager_scripted()
    for name in ANIMAL_FILES:
        pipeline = unreal.InterchangeGenericAssetsPipeline()
        pipeline.get_editor_property("mesh_pipeline").set_editor_property(
            "combine_skeletal_meshes_behavior", unreal.InterchangeCombineSkeletalMeshesBehavior.BY_SKELETON)
        parameters = unreal.ImportAssetParameters()
        parameters.set_editor_property("is_automated", True)
        parameters.set_editor_property("replace_existing", True)
        parameters.set_editor_property("override_pipelines", [unreal.SoftObjectPath(pipeline.get_path_name())])
        source = unreal.InterchangeManager.create_source_data(os.path.join(source_dir, name + ".glb"))
        manager.import_asset(ANIMALS_DESTINATION, source, parameters)
    _apply_animal_material(os.path.join(source_dir, "Textures", "colormap.png"))
    unreal.EditorAssetLibrary.save_directory(ANIMALS_DESTINATION, only_if_is_dirty=False, recursive=True)
    if all(_animal_assets(name)[0] for name in ANIMAL_FILES):
        return True, "Animated forest animals imported."
    return False, "Unreal did not import the animals. Check the Output Log."


def _apply_animal_material(colormap_path):
    """Give every animal Kenney's colour-palette texture.

    The glTF import leaves the animals plain white (its texture step fails),
    so the palette is imported on its own and wired into a small material.
    """
    texture_path = ANIMALS_DESTINATION + "/T_AnimalColormap"
    material_path = ANIMALS_DESTINATION + "/M_IWB_Animal"
    texture = unreal.load_asset(texture_path)
    if not texture and os.path.isfile(colormap_path):
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", colormap_path)
        task.set_editor_property("destination_path", ANIMALS_DESTINATION)
        task.set_editor_property("destination_name", "T_AnimalColormap")
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("save", True)
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        texture = unreal.load_asset(texture_path)
    if not isinstance(texture, unreal.Texture2D):
        unreal.log_warning("World Builder could not import the animal colour palette.")
        return
    # A palette texture: each face samples one flat colour, so keep it crisp.
    texture.set_editor_property("filter", unreal.TextureFilter.TF_NEAREST)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    unreal.EditorAssetLibrary.save_loaded_asset(texture, False)
    material = unreal.load_asset(material_path)
    if not material:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "M_IWB_Animal", ANIMALS_DESTINATION, unreal.Material, unreal.MaterialFactoryNew())
        sample = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -400, 0)
        sample.set_editor_property("parameter_name", "ColorMap")
        sample.set_editor_property("texture", texture)
        roughness = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionConstant, -400, 220)
        roughness.set_editor_property("r", 0.85)
        unreal.MaterialEditingLibrary.connect_material_property(sample, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
        unreal.MaterialEditingLibrary.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
        unreal.MaterialEditingLibrary.recompile_material(material)
        unreal.EditorAssetLibrary.save_loaded_asset(material, False)
    for name in ANIMAL_FILES:
        mesh = _animal_assets(name)[0]
        if not mesh:
            continue
        # Material slots come back as copies: build new ones, then assign them.
        mesh.set_editor_property("materials", [
            unreal.SkeletalMaterial(material_interface=material, material_slot_name=slot.get_editor_property("material_slot_name"))
            for slot in mesh.get_editor_property("materials")
        ])
        unreal.EditorAssetLibrary.save_loaded_asset(mesh, False)


def _gameplay_class(name):
    """C++ gameplay classes exist only once the ForestGame module is compiled."""
    return getattr(unreal, name, None)


def _ensure_blueprint(path, parent_class):
    asset = unreal.load_asset(path)
    if asset:
        return asset
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)
    folder, name = path.rsplit("/", 1)
    return unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.Blueprint, factory)


def _ensure_gameplay_blueprints():
    """One Blueprint per species (BP_Deer, BP_Bunny...) plus BP_MushroomPickup.

    They are Blueprint children of the C++ gameplay classes and hold the art
    and character of each species, so it can be tuned in the Blueprint editor
    without touching code.  Returns ([(blueprint, weight, height)], mushroom),
    or ([], None) when the C++ module is not compiled yet.
    """
    global _gameplay_blueprints
    animal_class, pickup_class = _gameplay_class("ForestAnimal"), _gameplay_class("ForestPickup")
    if not animal_class or not pickup_class:
        return [], None
    if _gameplay_blueprints and _gameplay_blueprints[0] and _gameplay_blueprints[1]:
        return _gameplay_blueprints
    animals = []
    for model, blueprint, name, height, radius, walk, run, shy, curious, follow, weight in ANIMAL_SPECIES:
        mesh, animations = _animal_assets(model)
        if not mesh:
            continue
        animal_bp = _ensure_blueprint(GAMEPLAY_FOLDER + "/" + blueprint, animal_class)
        if not animal_bp:
            continue
        defaults = unreal.get_default_object(animal_bp.generated_class())
        half_height = height * 0.5
        defaults.get_editor_property("capsule").set_capsule_size(radius, half_height)
        component = defaults.get_editor_property("mesh")
        component.set_skeletal_mesh_asset(mesh)
        scale = height / max(1.0, mesh.get_bounds().box_extent.z * 2.0)
        component.set_editor_property("relative_scale3d", unreal.Vector(scale, scale, scale))
        component.set_editor_property("relative_location", unreal.Vector(0.0, 0.0, -half_height))
        # Kenney's animals face +Y after import; actors walk along +X.
        component.set_editor_property("relative_rotation", _rot(-90.0))
        defaults.set_editor_property("display_name", unreal.Text(name))
        for prop, value in (("walk_speed", walk), ("run_speed", run), ("shy_distance", shy),
                            ("curious_distance", curious), ("follow_distance", follow)):
            defaults.set_editor_property(prop, value)
        for prop, key in (("idle_anim", "idle"), ("walk_anim", "walk"), ("run_anim", "run"), ("eat_anim", "eat"),
                          ("happy_anim", "gesture-positive"), ("refuse_anim", "gesture-negative"), ("dance_anim", "dance")):
            if key in animations:
                defaults.set_editor_property(prop, animations[key])
        unreal.BlueprintEditorLibrary.compile_blueprint(animal_bp)
        unreal.EditorAssetLibrary.save_loaded_asset(animal_bp, False)
        animals.append((animal_bp, weight, height))
    mushroom_bp = _ensure_blueprint(MUSHROOM_BLUEPRINT, pickup_class)
    mushroom = _kenney_mesh("mushroom_red")
    if mushroom_bp and isinstance(mushroom, unreal.StaticMesh):
        defaults = unreal.get_default_object(mushroom_bp.generated_class())
        component = defaults.get_editor_property("mesh")
        component.set_static_mesh(mushroom)
        component.set_editor_property("relative_scale3d", unreal.Vector(3.2, 3.2, 3.2))
        unreal.BlueprintEditorLibrary.compile_blueprint(mushroom_bp)
        unreal.EditorAssetLibrary.save_loaded_asset(mushroom_bp, False)
    _gameplay_blueprints = (animals, mushroom_bp)
    return _gameplay_blueprints


def _ambience_material(name, glow):
    """M_IWB_Glow (additive, unlit motes) or M_IWB_Wing (two-sided, softly lit wings)."""
    path = "/Game/WorldBuilder/Materials/" + name
    material = unreal.load_asset(path)
    if material:
        return material
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, "/Game/WorldBuilder/Materials", unreal.Material, unreal.MaterialFactoryNew())
    library = unreal.MaterialEditingLibrary
    material.set_editor_property("two_sided", True)
    tint = library.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -500, 0)
    tint.set_editor_property("parameter_name", "Tint")
    tint.set_editor_property("default_value", unreal.LinearColor(1.0, 0.8, 0.5, 1.0))
    strength = library.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -500, 200)
    strength.set_editor_property("parameter_name", "Glow")
    strength.set_editor_property("default_value", 6.0 if glow else 0.3)
    emissive = library.create_material_expression(material, unreal.MaterialExpressionMultiply, -250, 100)
    library.connect_material_expressions(tint, "", emissive, "A")
    library.connect_material_expressions(strength, "", emissive, "B")
    library.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    if glow:
        # Soft points of light that add to whatever is behind them.
        material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
        material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    else:
        library.connect_material_property(tint, "", unreal.MaterialProperty.MP_BASE_COLOR)
    library.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material, False)
    return material


def _ensure_ambience_blueprint():
    ambience_class = _gameplay_class("ForestAmbience")
    if not ambience_class:
        return None
    blueprint = _ensure_blueprint(GAMEPLAY_FOLDER + "/BP_ForestAmbience", ambience_class)
    defaults = unreal.get_default_object(blueprint.generated_class())
    sphere, plane, cube = (unreal.load_asset("/Engine/BasicShapes/" + name) for name in ("Sphere", "Plane", "Cube"))
    for component_name, mesh, material in (
        ("motes", sphere, _ambience_material("M_IWB_Glow", True)),
        ("butterfly_wings_a", plane, _ambience_material("M_IWB_Wing", False)),
        ("butterfly_wings_b", plane, _ambience_material("M_IWB_Wing", False)),
        ("dragonfly_wings", plane, _ambience_material("M_IWB_Wing", False)),
        ("bodies", cube, _get_tint_material()),
    ):
        component = defaults.get_editor_property(component_name)
        component.set_static_mesh(mesh)
        component.set_material(0, material)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    unreal.EditorAssetLibrary.save_loaded_asset(blueprint, False)
    return blueprint


def _paint_ambience(actor, palette):
    """Motes glow with the palette's sunlight; wings take its flower colours."""
    sun, cream = palette["sun"], palette["flower_a"]
    for component_name, colour, glow in (
        ("motes", unreal.LinearColor(sun.r * 0.6 + 0.4, sun.g * 0.6 + 0.35, sun.b * 0.6 + 0.2, 1.0), 6.0),
        ("butterfly_wings_a", palette["flower_b"], 0.35),
        ("butterfly_wings_b", unreal.LinearColor(cream.r, cream.g, cream.b, 1.0), 0.35),
        ("dragonfly_wings", unreal.LinearColor(0.55, 0.85, 0.90, 1.0), 0.5),
        ("bodies", palette["trunk_dark"], None),
    ):
        component = actor.get_editor_property(component_name)
        material = component.get_material(0)
        if not isinstance(material, unreal.MaterialInstanceDynamic):
            material = component.create_dynamic_material_instance(0)
        if material:
            material.set_vector_parameter_value("Tint", colour)
            if glow is not None:
                material.set_scalar_parameter_value("Glow", glow)


def _sound(name):
    """Import one CC0 recording on first use; None when it is not available."""
    path = "{}/{}".format(AUDIO_DESTINATION, name)
    asset = unreal.load_asset(path)
    if asset:
        return asset
    source = os.path.join(SOUNDS_FOLDER, SOUND_FILES[name])
    if not os.path.isfile(source):
        return None
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", source)
    task.set_editor_property("destination_path", AUDIO_DESTINATION)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    asset = unreal.load_asset(path)
    if not asset:
        unreal.log_warning("World Builder could not import the sound {}".format(SOUND_FILES[name]))
    return asset


def _bird_attenuation():
    """Birdsong fades over about 40 m, so each song comes from its bird."""
    path = AUDIO_DESTINATION + "/ATT_Birds"
    attenuation = unreal.load_asset(path)
    if attenuation:
        return attenuation
    attenuation = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "ATT_Birds", AUDIO_DESTINATION, unreal.SoundAttenuation, unreal.SoundAttenuationFactory())
    settings = attenuation.get_editor_property("attenuation")
    for prop, value in (("falloff_distance", 4000.0), ("spatialize", True), ("attenuate", True)):
        _set_first(settings, (prop, "b" + prop), value)
    radius = settings.get_editor_property("attenuation_shape_extents")
    settings.set_editor_property("attenuation_shape_extents", unreal.Vector(400.0, radius.y, radius.z))
    attenuation.set_editor_property("attenuation", settings)
    unreal.EditorAssetLibrary.save_loaded_asset(attenuation, False)
    return attenuation


def _ensure_birds_blueprint():
    birds_class = _gameplay_class("ForestBirds")
    if not birds_class:
        return None
    blueprint = _ensure_blueprint(GAMEPLAY_FOLDER + "/BP_ForestBirds", birds_class)
    defaults = unreal.get_default_object(blueprint.generated_class())
    sphere, plane = unreal.load_asset("/Engine/BasicShapes/Sphere"), unreal.load_asset("/Engine/BasicShapes/Plane")
    for component_name, mesh, material in (
        ("small_bodies", sphere, _get_tint_material()), ("small_heads", sphere, _get_tint_material()),
        ("small_wings", plane, _ambience_material("M_IWB_Wing", False)),
        ("large_bodies", sphere, _get_tint_material()), ("large_heads", sphere, _get_tint_material()),
        ("large_wings", plane, _ambience_material("M_IWB_Wing", False)),
    ):
        component = defaults.get_editor_property(component_name)
        component.set_static_mesh(mesh)
        component.set_material(0, material)
    chirps = [sound for sound in (_sound("S_Chirp_A"), _sound("S_Chirp_B"), _sound("S_BirdSong")) if sound]
    defaults.set_editor_property("chirp_sounds", chirps)
    crows = _sound("S_Crows")
    if crows:
        defaults.set_editor_property("large_call_sound", crows)
    defaults.set_editor_property("attenuation", _bird_attenuation())
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    unreal.EditorAssetLibrary.save_loaded_asset(blueprint, False)
    return blueprint


def _paint_birds(actor, palette):
    """Small birds in warm earth tones with pale heads; large birds in soft charcoal."""
    trunk_dark, cream = palette["trunk_dark"], palette["flower_a"]
    brown = unreal.LinearColor(0.42, 0.28, 0.18, 1.0)
    for component_name, colour, glow in (
        ("small_bodies", brown, None),
        ("small_heads", unreal.LinearColor(cream.r * 0.9, cream.g * 0.8, cream.b * 0.7, 1.0), None),
        ("small_wings", unreal.LinearColor(0.30, 0.20, 0.13, 1.0), 0.1),
        ("large_bodies", unreal.LinearColor(trunk_dark.r + 0.08, trunk_dark.g + 0.08, trunk_dark.b + 0.09, 1.0), None),
        ("large_heads", unreal.LinearColor(0.85, 0.82, 0.76, 1.0), None),
        ("large_wings", unreal.LinearColor(trunk_dark.r + 0.05, trunk_dark.g + 0.05, trunk_dark.b + 0.06, 1.0), 0.05),
    ):
        component = actor.get_editor_property(component_name)
        material = component.get_material(0)
        if not isinstance(material, unreal.MaterialInstanceDynamic):
            material = component.create_dynamic_material_instance(0)
        if material:
            material.set_vector_parameter_value("Tint", colour)
            if glow is not None:
                material.set_scalar_parameter_value("Glow", glow)


def _build_soundscape(tag, origin_x):
    """A soft forest bed and distant birdsong everywhere in the world."""
    placed = 0
    for name, volume in (("S_ForestAmbience", 0.7), ("S_BirdsAmbience", 0.3)):
        sound = _sound(name)
        if not sound:
            continue
        sound.set_editor_property("looping", True)
        unreal.EditorAssetLibrary.save_loaded_asset(sound, False)
        actor = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.AmbientSound, unreal.Vector(origin_x, 0.0, 200.0), unreal.Rotator())
        _tag(actor, tag, AMBIENCE_TAG, "IWB_" + name[2:])
        audio = actor.get_editor_property("audio_component")
        audio.set_sound(sound)
        audio.set_volume_multiplier(volume)
        # Not spatialised: the forest surrounds the player wherever they walk.
        audio.set_editor_property("allow_spatialization", False)
        audio.set_editor_property("auto_activate", True)
        placed += 1
    return placed


def _build_ambience(data, tag, origin_x):
    """Floating motes, butterflies, and dragonflies over the generated forest."""
    blueprint = _ensure_ambience_blueprint()
    if not blueprint or tag != GENERATED_TAG:
        return 0
    terrain = data["terrain"]
    area = (terrain / 12000.0) ** 2
    actor = unreal.EditorLevelLibrary.spawn_actor_from_object(blueprint, unreal.Vector(origin_x, 0.0, 0.0), unreal.Rotator())
    if not actor:
        return 0
    _tag(actor, tag, AMBIENCE_TAG, "IWB_Ambience")
    actor.set_editor_property("world_center", unreal.Vector(origin_x, 0.0, 0.0))
    actor.set_editor_property("world_radius", terrain * 0.5 * 0.75)
    wildlife = data.get("wildlife", 50.0) / 50.0
    actor.set_editor_property("butterfly_count", min(60, round(18 * area * wildlife)))
    actor.set_editor_property("dragonfly_count", min(24, round(7 * area * wildlife)))
    # Sparkles: the glowing motes in the air.
    actor.set_editor_property("mote_count", round(520 * data.get("sparkles", 50.0) / 100.0))
    try:
        # Re-create the instances with the new counts.
        actor.rerun_construction_scripts()
    except Exception:
        pass
    _paint_ambience(actor, _palette(data))
    birds_blueprint = _ensure_birds_blueprint()
    if birds_blueprint:
        birds = unreal.EditorLevelLibrary.spawn_actor_from_object(birds_blueprint, unreal.Vector(origin_x, 0.0, 0.0), unreal.Rotator())
        if birds:
            _tag(birds, tag, AMBIENCE_TAG, "IWB_Birds")
            birds.set_editor_property("world_center", unreal.Vector(origin_x, 0.0, 0.0))
            birds.set_editor_property("world_radius", terrain * 0.5 * 0.75)
            # Many small birds, only a few large ones.
            birds.set_editor_property("small_flock_count", min(24, round(7 * area * wildlife)))
            birds.set_editor_property("large_bird_count", min(10, round(3 * area * wildlife)))
            try:
                birds.rerun_construction_scripts()
            except Exception:
                pass
            _paint_birds(birds, _palette(data))
    _build_soundscape(tag, origin_x)
    return 1


def _build_gameplay(data, tag, origin_x, terrain_features, footprints, fields, rng):
    """Animals in the glades, mushrooms on the forest floor, and the game manager.

    Skipped quietly when the ForestGame C++ module is not compiled, so the
    World Builder keeps working on its own.
    """
    animals, mushroom_bp = _ensure_gameplay_blueprints()
    manager_class = _gameplay_class("ForestGameManager")
    if not animals or not mushroom_bp or not manager_class or tag != GENERATED_TAG:
        return 0, 0
    terrain = data["terrain"]
    radius = terrain * 0.5 * 0.78
    area = (terrain / 12000.0) ** 2
    wildlife = data.get("wildlife", 50.0) / 50.0
    # The game needs a few animals even in a quiet world.
    target = max(3, min(24, round(6 * area * wildlife)))
    weights = [weight for _bp, weight, _height in animals]
    placed = 0
    # Animals graze in the open: the inverse of the forest field.
    for x, y in _candidates(rng, radius * 0.9, lambda px, py: 1.0 - 0.85 * fields["forest"](px, py), 5000):
        if placed >= target:
            break
        if _ground_slope(x, y, terrain_features) > 0.25 or not footprints.is_free(x, y, 160.0):
            continue
        # The first animals cover every species; the rest follow how common each one is.
        blueprint, _weight, height = animals[placed] if placed < len(animals) else rng.choices(animals, weights=weights)[0]
        actor = unreal.EditorLevelLibrary.spawn_actor_from_object(
            blueprint, unreal.Vector(origin_x + x, y, _ground_height(x, y, terrain_features) + height * 0.5 + 2.0), _rot(rng.uniform(0, 360)))
        if not actor:
            continue
        _tag(actor, tag, GAMEPLAY_TAG, "IWB_" + blueprint.get_name()[3:])
        actor.set_editor_property("world_center", unreal.Vector(origin_x, 0.0, 0.0))
        actor.set_editor_property("world_radius", radius)
        footprints.reserve(x, y, 500.0)
        placed += 1
    # Two mushrooms per animal, plus a few spare, hidden under the trees.
    pickups = 0
    pickup_target = placed * 2 + 4
    for x, y in _candidates(rng, radius, fields["floor"], 6000):
        if pickups >= pickup_target:
            break
        if not footprints.is_free(x, y, 120.0):
            continue
        actor = unreal.EditorLevelLibrary.spawn_actor_from_object(
            mushroom_bp, unreal.Vector(origin_x + x, y, _ground_height(x, y, terrain_features) + 2.0), _rot(rng.uniform(0, 360)))
        if not actor:
            continue
        _tag(actor, tag, GAMEPLAY_TAG, "IWB_MushroomPickup")
        footprints.reserve(x, y, 300.0)
        pickups += 1
    manager = unreal.EditorLevelLibrary.spawn_actor_from_class(manager_class, unreal.Vector(origin_x, 0.0, 300.0), unreal.Rotator())
    _tag(manager, tag, GAMEPLAY_TAG, "IWB_ForestGame")
    return placed, pickups


def _save_level(widget, message=None):
    """Save the level (and its generated actors) so closing Unreal never loses a world."""
    try:
        saved = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    except Exception as error:
        saved = False
        unreal.log_warning("World Builder could not save the level: {}".format(error))
    if not saved:
        _status(widget, "The world is built but NOT saved: use File > Save All.")
    elif message:
        _status(widget, message)


def generate(widget):
    global _plant_variation
    _plant_variation = 0
    if not _fantasy_town_is_imported() and os.path.isfile(_fantasy_town_zip_path()):
        _status(widget, "First build: importing selected Fantasy Town landmarks...")
        imported, message = _import_fantasy_town_assets()
        if not imported:
            _status(widget, "Fantasy Town Kit was skipped: " + message)
    if not _kenney_is_imported() and os.path.isfile(_kenney_zip_path()):
        _status(widget, "First build: importing the selected Nature Kit models...")
        imported, message = _import_kenney_assets()
        if not imported:
            _status(widget, "Nature Kit was skipped: " + message)
    if _gameplay_class("ForestAnimal") and not all(_animal_assets(name)[0] for name in ANIMAL_FILES) and os.path.isfile(_animals_zip_path()):
        _status(widget, "First build: importing the forest animals...")
        imported, message = _import_animals()
        if not imported:
            _status(widget, "Animals were skipped: " + message)
    _clear_world()
    _clear_world(preview_only=True)
    _hide_template_environment()
    try:
        _build_world(widget)
    except Exception as error:
        _clear_world()
        _status(widget, "Generation stopped safely: {}".format(error))
        unreal.log_error("World Builder generation stopped: {}".format(error))
        return
    generated = [actor for actor in unreal.EditorLevelLibrary.get_all_level_actors() if _has_tag(actor, GENERATED_TAG)]
    unreal.get_editor_subsystem(unreal.EditorActorSubsystem).set_selected_level_actors(generated)
    _status(widget, "World generated. The terrain is selected - press F in the viewport to frame it.")
    # The river and its streams follow the new valleys; Houdini makes the water.
    try:
        import iwb_river
        river = iwb_river.build_river(save=False)
    except Exception as error:
        river = "No river this time ({}).".format(error)
        unreal.log_warning("World Builder river skipped: {}".format(error))
    _save_level(widget, "World built and saved. " + river)


def preview(widget):
    _clear_world(preview_only=True)
    try:
        _build_world(widget, preview=True)
    except Exception as error:
        _clear_world(preview_only=True)
        _status(widget, "Preview stopped safely: {}".format(error))
        unreal.log_error("World Builder preview stopped: {}".format(error))


def recolor_world(widget):
    """Repaint every generated actor with the selected palette, without rebuilding."""
    data = _values(widget)
    palette = _palette(data)
    name = data["palette"]
    _find(widget, "PaletteHint").set_text("{}: {}.".format(name, _palette_description(name)))
    painted, generated_world = 0, False
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if not (_has_tag(actor, GENERATED_TAG) or _has_tag(actor, PREVIEW_TAG)):
            continue
        generated_world = generated_world or _has_tag(actor, GENERATED_TAG)
        if _has_tag(actor, AMBIENCE_TAG):
            # Identify by class, not label: "IWB_BirdsAmbience" is a sound, not the birds.
            ambience_class, birds_class = _gameplay_class("ForestAmbience"), _gameplay_class("ForestBirds")
            if ambience_class and isinstance(actor, ambience_class):
                _paint_ambience(actor, palette)
            elif birds_class and isinstance(actor, birds_class):
                _paint_birds(actor, palette)
            continue
        if actor.get_actor_label().startswith("IWB_Ground"):
            _paint_ground(actor, palette)
            painted += 1
            continue
        key = _tag_value(actor, PAINT_PREFIX)
        if not key or key not in palette:
            continue
        trunk, stem = _tag_value(actor, TRUNK_PREFIX), _tag_value(actor, STEM_PREFIX)
        _apply_mesh_tint(
            actor, palette[key],
            palette.get(trunk) if trunk else None,
            palette.get(stem) if stem else None,
        )
        painted += 1
    if generated_world:
        _apply_lighting(data, GENERATED_TAG, 0.0)
    if painted:
        _status(widget, "{} palette applied to {} objects.".format(name, painted))
    elif generated_world:
        _status(widget, "This world was built by an older version. Build World once, then palettes repaint it instantly.")
    else:
        _status(widget, "{} palette selected. Build World to see it.".format(name))


def apply_time_of_day(widget):
    data = _values(widget)
    generated = any(_has_tag(actor, GENERATED_TAG) for actor in unreal.EditorLevelLibrary.get_all_level_actors())
    if generated:
        _apply_lighting(data, GENERATED_TAG, 0.0)
        _status(widget, "{}: the light and the haze have changed.".format(data["time_of_day"]))
    else:
        _status(widget, "{} selected. Build World to see it.".format(data["time_of_day"]))


def apply_weather(widget):
    """Rain or snow right away: new drops, wet or snowy surfaces, and the light."""
    data = _values(widget)
    if not any(_has_tag(actor, GENERATED_TAG) for actor in unreal.EditorLevelLibrary.get_all_level_actors()):
        _status(widget, "{} selected. Build World to see it.".format(data["weather"]))
        return
    _clear_world(layers=(WEATHER_TAG,))
    _build_weather(data, GENERATED_TAG, 0.0)
    _apply_lighting(data, GENERATED_TAG, 0.0)
    _status(widget, {"Clear": "Clear sky: the ground is dry again.", "Rain": "Rain: wet, shiny ground and a grey sky.",
                     "Snow": "Snow: white on every top, cold light."}.get(data["weather"], data["weather"]))


def _make_weather_callback(widget):
    def callback(_selected_item, selection_type):
        if selection_type == unreal.SelectInfo.DIRECT:
            return
        _queue(apply_weather, widget)
    return callback


def _make_time_callback(widget):
    def callback(_selected_item, selection_type):
        if selection_type == unreal.SelectInfo.DIRECT:
            return
        _queue(apply_time_of_day, widget)
    return callback


def apply_mood(widget):
    """Relight the current world and update its sparkles from the Mood tab."""
    data = _values(widget)
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    if not any(_has_tag(actor, GENERATED_TAG) for actor in actors):
        _status(widget, "Mood saved for the next Build World.")
        return
    _apply_lighting(data, GENERATED_TAG, 0.0)
    for actor in actors:
        ambience_class = _gameplay_class("ForestAmbience")
        if _has_tag(actor, AMBIENCE_TAG) and ambience_class and isinstance(actor, ambience_class):
            motes = round(520 * data["sparkles"] / 100.0)
            if actor.get_editor_property("mote_count") != motes:
                actor.set_editor_property("mote_count", motes)
                try:
                    actor.rerun_construction_scripts()
                except Exception:
                    pass
                _paint_ambience(actor, _palette(data))
    _status(widget, "Mood updated: {}, haze {}, sparkles {}.".format(data["time_of_day"], data["haze"], data["sparkles"]))


def _make_release_callback(widget):
    def callback():
        _queue(apply_mood, widget)
    return callback


def next_palette(widget):
    _find(widget, "PaletteBox").step(1)


_plant_variation = 0


def refresh_plants(widget):
    """Regrow only the plant, rock, and landmark layers on the existing ground.

    Every click grows a different layout: the seed field keeps the ground,
    and a running variation number changes the plants."""
    global _plant_variation
    _plant_variation += 1
    ground = _load_last_build()
    has_terrain = any(
        _has_tag(actor, GENERATED_TAG) and _has_tag(actor, TERRAIN_TAG)
        for actor in unreal.EditorLevelLibrary.get_all_level_actors()
    )
    if not ground or not has_terrain:
        _status(widget, "Build World first: New plants regrows the life on an existing world.")
        return
    _status(widget, "Growing new plants...")
    _clear_world(layers=PLANT_LAYERS)
    try:
        _build_world(widget, vegetation_only=True, ground=ground)
    except Exception as error:
        _clear_world(layers=PLANT_LAYERS)
        _status(widget, "Refresh stopped safely: {}".format(error))
        unreal.log_error("World Builder plant refresh stopped: {}".format(error))
        return
    _save_level(widget)


def update_vegetation(widget):
    """Apply every edited control reliably, including scale, terrain, season, and weather."""
    _status(widget, "Updating world...")
    _clear_world()
    _clear_world(preview_only=True)
    _hide_template_environment()
    try:
        _build_world(widget)
    except Exception as error:
        _clear_world()
        _status(widget, "Update stopped safely: {}".format(error))
        unreal.log_error("World Builder update stopped: {}".format(error))
        return
    generated = [actor for actor in unreal.EditorLevelLibrary.get_all_level_actors() if _has_tag(actor, GENERATED_TAG)]
    unreal.get_editor_subsystem(unreal.EditorActorSubsystem).set_selected_level_actors(generated)
    _status(widget, "World updated. Every edited control has been applied.")


def start_over(widget):
    _clear_world()
    _clear_world(preview_only=True)
    reset(widget)
    _status(widget, "Fresh start. The generated world is cleared and the controls are reset.")


def save_preset(widget):
    with open(_preset_path(), "w", encoding="utf-8") as handle:
        json.dump(_values(widget), handle, indent=2)
    _status(widget, "Preset saved to Saved/InteractiveWorldBuilderPreset.json.")


def load_preset(widget):
    if not os.path.exists(_preset_path()):
        _status(widget, "No saved preset yet.")
        return
    with open(_preset_path(), "r", encoding="utf-8") as handle:
        data = json.load(handle)
    _find(widget, "WorldNameBox").set_text(data.get("world_name", "Whispering Pines"))
    _find(widget, "SeedBox").set_text(str(data.get("seed", 20261018)))
    legacy_themes = {
        "Forest": "Summer", "Golden Meadow": "Autumn", "Misty Highlands": "Winter",
        "Mountain (next pass)": "Winter", "Meadow (next pass)": "Spring", "Beach (next pass)": "Summer",
    }
    # Emberwild used to be a season; it is now a palette that works with any season.
    theme = data.get("theme", "Summer")
    _find(widget, "ThemeBox").set_selected_option(theme if theme in SEASONS else legacy_themes.get(theme, "Summer"))
    palette = data.get("palette", DEFAULT_PALETTE)
    _find(widget, "PaletteBox").set_selected_option(palette if palette in PALETTE_NAMES else DEFAULT_PALETTE)
    time_of_day = data.get("time_of_day", DEFAULT_TIME_OF_DAY)
    _find(widget, "TimeBox").set_selected_option(time_of_day if time_of_day in TIMES_OF_DAY else DEFAULT_TIME_OF_DAY)
    _find(widget, "PathsBox").set_selected_option(data.get("paths", "Network"))
    _find(widget, "WeatherBox").set_selected_option(data.get("weather", "Clear"))
    if data.get("vegetation") is None and "trees" in data:
        # Older presets stored a tree count; convert it to the matching density.
        area = (data.get("terrain", 14000.0) / 12000.0) ** 2
        data["vegetation"] = max(0.0, min(100.0, data["trees"] / (60.0 * area) * 50.0))
    for key, value_key, _label, default, _minimum, _maximum, _decimals in SLIDERS:
        _find(widget, key + "Slider").set_value(data.get(value_key, default))
    _find(widget, "MountainsBox").set_is_checked(data.get("mountains", True))
    _find(widget, "PlayableCharacterBox").set_is_checked(data.get("playable_character", True))
    _refresh_readouts(widget)
    _status(widget, "Preset loaded. Preview checks it without replacing the generated world.")


def reset(widget):
    _find(widget, "WorldNameBox").set_text("Whispering Pines")
    _find(widget, "SeedBox").set_text("20261018")
    _find(widget, "ThemeBox").set_selected_option("Summer")
    _find(widget, "PaletteBox").set_selected_option(DEFAULT_PALETTE)
    _find(widget, "TimeBox").set_selected_option(DEFAULT_TIME_OF_DAY)
    _find(widget, "PathsBox").set_selected_option("Network")
    _find(widget, "WeatherBox").set_selected_option("Clear")
    for key, _value_key, _label, default, _minimum, _maximum, _decimals in SLIDERS:
        _find(widget, key + "Slider").set_value(default)
    _find(widget, "MountainsBox").set_is_checked(True)
    _find(widget, "PlayableCharacterBox").set_is_checked(True)
    _refresh_readouts(widget)
    _status(widget, "Controls reset. Existing generated actors were not changed.")


def delete_world(widget):
    _set_weather_values(0.0, 0.0)
    _clear_world()
    _clear_world(preview_only=True)
    _restore_template_environment()
    _restore_template_state()
    _status(widget, "Generated actors removed. The original Third Person level, light, and start are back.")


def _run_pending_actions(_delta_seconds):
    """Run the panel actions queued by button clicks, one per frame."""
    if not _pending_actions:
        return
    action, widget = _pending_actions.pop(0)
    try:
        action(widget)
    except Exception as error:
        unreal.log_error("World Builder action {} failed: {}".format(action.__name__, error))
        try:
            _status(widget, "Something went wrong: {}".format(error))
        except Exception:
            pass


def _ensure_action_runner():
    global _action_handle
    if _action_handle is None:
        _action_handle = _register_tick(_run_pending_actions)


def _queue(action, widget):
    """Run an action on the next frame, once, outside the widget event."""
    _ensure_action_runner()
    if not any(queued is action and target == widget for queued, target in _pending_actions):
        _pending_actions.append((action, widget))


def _make_click_callback(action, widget):
    # A click only queues the work.  Building a world compiles Blueprints,
    # imports assets, and spawns thousands of instances; doing that inside
    # the button's own click event let Unreal free objects the click was
    # still using, and the editor crashed right after a successful build.
    def callback():
        _queue(action, widget)
    return callback


def _make_slider_callback(widget):
    def callback(_value):
        _refresh_readouts(widget)
    return callback


def _make_tab_callback(widget, index):
    names = ("WorldTabButton", "NatureTabButton", "MoodTabButton", "BuildTabButton")

    def callback():
        _find(widget, "IWBTabSwitcher").set_active_widget_index(index)
        # The open tab is apricot; the others stay quiet.
        for position, name in enumerate(names):
            selected = position == index
            try:
                _style_button(_find(widget, name), "accent" if selected else "raised", "accent_hover" if selected else "hover", "pressed")
                ink = _color("on_accent" if selected else "text")
                _tint(_find(widget, name + "Label"), ink)
                _tint(_find(widget, name + "Icon"), ink)
            except Exception:
                pass
    return callback


def _make_theme_callback(widget):
    descriptions = {
        "Spring": "round broadleaf trees and a bright mid-morning sun",
        "Summer": "full broadleaf and pine trees under a high sun",
        "Autumn": "fall-leaf oaks and birches under a low golden sun",
        "Winter": "pine forest under a pale, low winter sun",
    }

    def callback(selected_item, _selection_type):
        theme = str(selected_item)
        description = descriptions.get(theme, "a custom season")
        _find(widget, "ThemeHint").set_text("{}: {}. Build World to apply it.".format(theme, description))
        _status(widget, "{} selected. Build World to grow its trees and sunlight.".format(theme))
    return callback


def _make_palette_callback(widget):
    def callback(selected_item, selection_type):
        # Setting the selector from code (Reset, Load Preset) reports a Direct
        # selection; only a click repaints the world.
        if selection_type == unreal.SelectInfo.DIRECT:
            return
        _queue(recolor_world, widget)
    return callback


def bind_widget(widget):
    state = _panel(widget)
    if state["bound"]:
        return
    keep = state["callbacks"].append
    actions = {
        "GenerateButton": generate,
        "RefreshPlantsButton": refresh_plants,
        "NewSeedButton": new_seed,
        "CompactProfileButton": compact_profile,
        "BalancedProfileButton": balanced_profile,
        "LushProfileButton": lush_profile,
        "SavePresetButton": save_preset,
        "LoadPresetButton": load_preset,
        "ResetButton": reset,
        "DeleteButton": delete_world,
    }
    for name, action in actions.items():
        callback = _make_click_callback(action, widget)
        _find(widget, name).on_clicked.add_callable(callback)
        keep(callback)
    for index, name in enumerate(("WorldTabButton", "NatureTabButton", "MoodTabButton", "BuildTabButton")):
        callback = _make_tab_callback(widget, index)
        _find(widget, name).on_clicked.add_callable(callback)
        keep(callback)
    for key, *_rest in SLIDERS:
        callback = _make_slider_callback(widget)
        _find(widget, key + "Slider").on_value_changed.add_callable(callback)
        keep(callback)
    for key in LIVE_MOOD_SLIDERS:
        callback = _make_release_callback(widget)
        _find(widget, key + "Slider").on_mouse_capture_end.add_callable(callback)
        keep(callback)
    for name, factory in (("ThemeBox", _make_theme_callback), ("TimeBox", _make_time_callback), ("PaletteBox", _make_palette_callback),
                          ("WeatherBox", _make_weather_callback)):
        callback = factory(widget)
        _find(widget, name).on_selection_changed.add_callable(callback)
        keep(callback)
    _refresh_readouts(widget)
    state["bound"] = True


def _panel_instances():
    """Every live World Builder panel (Unreal may hold more than one)."""
    found = []
    try:
        for item in unreal.ObjectIterator(unreal.EditorUtilityWidget):
            name = item.get_name()
            if name.startswith("EUW_WorldBuilderPanel") and not name.startswith("Default__") and item.find_child_widget_by_name(HOST_PANEL):
                found.append(item)
    except Exception:
        pass
    return found


def _prepare_panel(widget, force=False):
    _build_runtime_ui(widget, force)
    bind_widget(widget)


def open_tool():
    asset = build_widget_asset()
    subsystem = unreal.get_editor_subsystem(unreal.EditorUtilitySubsystem)
    widget = subsystem.find_utility_widget_from_blueprint(asset)
    if not widget:
        widget = subsystem.spawn_and_register_tab(asset)
    _prepare_panel(widget)
    _status(widget, "World Builder ready.")
    return widget


def reload_tool():
    """Refresh every open panel after a Python edit, without restarting Unreal."""
    global _watch_handle
    if _watch_handle is None:
        _watch_handle = _register_tick(_watch_for_manually_opened_panel)
    asset = build_widget_asset()
    subsystem = unreal.get_editor_subsystem(unreal.EditorUtilitySubsystem)
    widget = subsystem.find_utility_widget_from_blueprint(asset) or subsystem.spawn_and_register_tab(asset)
    for panel in set(_panel_instances() + [widget]):
        _prepare_panel(panel, force=True)
        _status(panel, "World Builder reloaded.")
    return widget


@unreal.uclass()
class InteractiveWorldBuilderMenuEntry(unreal.ToolMenuEntryScript):
    @unreal.ufunction(override=True)
    def get_label(self, context):
        return "Open Interactive World Builder"

    @unreal.ufunction(override=True)
    def get_tool_tip(self, context):
        return "Open the seeded forest world-building panel."

    @unreal.ufunction(override=True)
    def execute(self, context):
        open_tool()


def register_menu():
    menus = unreal.ToolMenus.get()
    menus.unregister_owner_by_name(MENU_OWNER)
    menu = menus.extend_menu("LevelEditor.MainMenu.Tools")
    entry = InteractiveWorldBuilderMenuEntry()
    entry.init_entry(MENU_OWNER, "LevelEditor.MainMenu.Tools", "WorldBuilder", "InteractiveWorldBuilder.Open", "Open Interactive World Builder")
    menu.add_menu_entry_object(entry)
    menus.refresh_all_widgets()


def _watch_for_manually_opened_panel(_delta_seconds):
    """Turn every newly opened host EUW into a working World Builder panel."""
    global _watch_clock
    _watch_clock += _delta_seconds
    if _watch_clock < 0.5:
        return
    _watch_clock = 0.0
    try:
        for widget in _panel_instances():
            if not _panel(widget)["built"]:
                _prepare_panel(widget)
                _status(widget, "World Builder ready.")
    except Exception as error:
        unreal.log_warning("World Builder panel watch skipped: {}".format(error))


def startup():
    try:
        global _watch_handle, _widget_asset
        register_menu()
        _widget_asset = build_widget_asset()
        if _watch_handle is None:
            _watch_handle = _register_tick(_watch_for_manually_opened_panel)
        _ensure_action_runner()
        return open_tool()
    except Exception as error:
        unreal.log_error("Interactive World Builder startup failed: {}".format(error))
        return None

