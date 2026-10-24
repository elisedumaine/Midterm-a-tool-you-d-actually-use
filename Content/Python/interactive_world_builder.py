"""Interactive World Builder Editor Utility Widget runtime."""

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
TEMPLATE_HIDDEN_TAG = "IWB_TemplateHidden"
PRESET_FILENAME = "InteractiveWorldBuilderPreset.json"
KENNEY_ZIP_FILENAME = "kenney_nature-kit.zip"
KENNEY_DESTINATION = "/Game/WorldBuilder/Imported/Kenney"
FANTASY_TOWN_ZIP_FILENAME = "kenney_fantasy-town-kit_2.0.zip"
FANTASY_TOWN_DESTINATION = "/Game/WorldBuilder/Imported/FantasyTown"
FANTASY_TOWN_IMPORT_VERSION = "Fantasy Town landmarks v1"
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
PATH_THICKNESS = 6.0
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
)
MENU_OWNER = "InteractiveWorldBuilderMenu"
_callbacks = []
_bound_widget = None
_watch_handle = None
_widget_asset = None
_tint_material = None
_weather_material = None
_roboto_font = None


def _margin(left=8.0, top=3.0, right=8.0, bottom=3.0):
    return unreal.Margin(left, top, right, bottom)


def _new(cls, owner, name):
    return cls(outer=owner, name=name)


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


def _text(owner, name, text, strong=False, color=None, size=13):
    item = _new(unreal.TextBlock, owner, name)
    item.set_text(text)
    item.set_auto_wrap_text(True)
    item.set_font(_font(size, strong))
    if color:
        item.set_color_and_opacity(unreal.SlateColor(color))
    return item


def _button(owner, name, text, tone="soft"):
    item = _new(unreal.Button, owner, name)
    colors = {
        "primary": unreal.LinearColor(0.10, 0.40, 0.23, 1.0),
        "forest": unreal.LinearColor(0.08, 0.32, 0.20, 1.0),
        "clay": unreal.LinearColor(0.47, 0.25, 0.14, 1.0),
        "danger": unreal.LinearColor(0.40, 0.12, 0.11, 1.0),
        "soft": unreal.LinearColor(0.18, 0.23, 0.19, 1.0),
    }
    try:
        item.set_background_color(colors.get(tone, colors["soft"]))
    except Exception:
        pass
    item.add_child(_text(owner, name + "Label", text, True, unreal.LinearColor(0.94, 0.94, 0.88, 1.0), 12))
    return item


def _section(owner, parent, name, title, caption=None):
    """A compact card keeps the long tool readable without making it feel like a form."""
    card = _new(unreal.Border, owner, name + "Card")
    try:
        card.set_brush_color(unreal.LinearColor(0.045, 0.075, 0.058, 1.0))
        card.set_padding(_margin(8, 7, 8, 7))
    except Exception:
        pass
    content = _new(unreal.VerticalBox, owner, name + "Content")
    card.add_child(content)
    _add(content, _text(owner, name + "Title", title, True, unreal.LinearColor(0.68, 0.81, 0.67, 1.0), 12), _margin(3, 1, 3, 1))
    if caption:
        _add(content, _text(owner, name + "Caption", caption, False, unreal.LinearColor(0.47, 0.60, 0.49, 1.0), 10), _margin(3, 0, 3, 5))
    _add(parent, card, _margin(8, 4, 8, 4))
    return content


def _field_row(owner, panel, key, label, default):
    row = _new(unreal.HorizontalBox, owner, key + "Row")
    caption = _text(owner, key + "Label", label)
    caption.set_min_desired_width(112.0)
    _add(row, caption, _margin(8, 3, 4, 3))
    field = _new(unreal.EditableTextBox, owner, key + "Box")
    field.set_text(default)
    _add(row, field, _margin(0, 3, 8, 3), fill=True)
    _add(panel, row)


def _slider_row(owner, panel, key, label, value, minimum, maximum, decimals=False):
    row = _new(unreal.HorizontalBox, owner, key + "Row")
    caption = _text(owner, key + "Label", label)
    caption.set_min_desired_width(112.0)
    _add(row, caption, _margin(8, 3, 4, 3))
    slider = _new(unreal.Slider, owner, key + "Slider")
    slider.set_min_value(minimum)
    slider.set_max_value(maximum)
    slider.set_value(value)
    # A Fill slot gives the Slider a visible rail instead of a tiny isolated handle.
    _add(row, slider, _margin(0, 5, 8, 5), fill=True)
    readout = _text(owner, key + "Readout", f"{value:.2f}" if decimals else str(round(value)))
    readout.set_min_desired_width(48.0)
    _add(row, readout, _margin(0, 3, 8, 3))
    _add(panel, row)


def _notice(owner, parent, name, text, tone="muted"):
    """Small, tinted callout used for context without competing with controls."""
    colors = {
        "muted": unreal.LinearColor(0.055, 0.105, 0.074, 1.0),
        "success": unreal.LinearColor(0.045, 0.145, 0.083, 1.0),
        "warning": unreal.LinearColor(0.16, 0.105, 0.045, 1.0),
    }
    border = _new(unreal.Border, owner, name)
    try:
        border.set_brush_color(colors.get(tone, colors["muted"]))
        border.set_padding(_margin(8, 6, 8, 6))
    except Exception:
        pass
    border.add_child(_text(owner, name + "Text", text, False, unreal.LinearColor(0.72, 0.82, 0.72, 1.0), 10))
    _add(parent, border, _margin(3, 4, 3, 3))
    return border


def _summary(widget):
    """Turn the raw density controls into a quick, useful build estimate."""
    try:
        trees = round(_find(widget, "TreesSlider").get_value())
        flowers = round(trees * _find(widget, "FlowerRatioSlider").get_value() / 10.0)
        rocks = round(_find(widget, "RocksSlider").get_value())
        grass = round(trees * _find(widget, "GrassRatioSlider").get_value() / 10.0)
        terrain = round(_find(widget, "TerrainSlider").get_value())
        total = trees + flowers + rocks + grass
        terrain_label = "compact" if terrain <= 4500 else "wide" if terrain >= 10500 else "balanced"
        density_label = "light" if total < 170 else "balanced" if total < 370 else "lush"
        message = "{} scene · {} placements · {} scale".format(density_label.title(), total, terrain_label)
        message = "{} scene | {}T / {}F / {}G | {} scale".format(density_label.title(), trees, flowers, grass, terrain_label)
        summary = widget.find_child_widget_by_name("BuildSummaryText")
        if summary:
            summary.set_text(message)
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
    item = widget.find_child_widget_by_name(name)
    if not item:
        raise RuntimeError("World Builder UI element is missing: " + name)
    return item


def _build_runtime_ui(widget, force=False):
    if widget.find_child_widget_by_name("IWBTitle") and not force:
        return
    global _callbacks, _bound_widget
    if force:
        # Callbacks held by the former dynamic controls are no longer useful.
        _callbacks = []
        _bound_widget = None
    panel = _find(widget, HOST_PANEL)
    panel.clear_children()
    # This asset started as the Landmass erosion utility. Hide every remaining
    # Erosion control so it reads as one deliberate World Builder tool.
    for name in (
        "HorizontalBox_PresetButtons", "HorizontalBox_MoveDelete", "HorizontalBox_ErosionKeys",
        "Button_AddErosionKey", "Button_Simulate", "Button_Reset", "Text_Iterations", "TextBlock_13",
        "Button_LoadSettings", "Button_SaveToDataAsset", "Button_NewDataAsset",
        "Button_DataAssetCategory", "Button_Delete", "Button_SwapBack", "Button_SwapFwd",
    ):
        old = widget.find_child_widget_by_name(name)
        if old:
            old.set_visibility(unreal.SlateVisibility.COLLAPSED)

    cream = unreal.LinearColor(0.93, 0.89, 0.78, 1.0)
    leaf = unreal.LinearColor(0.53, 0.78, 0.48, 1.0)
    muted = unreal.LinearColor(0.47, 0.61, 0.48, 1.0)
    header = _new(unreal.Border, widget, "IWBHeader")
    try:
        header.set_brush_color(unreal.LinearColor(0.12, 0.27, 0.18, 1.0))
        header.set_padding(_margin(12, 10, 12, 10))
    except Exception:
        pass
    header_content = _new(unreal.VerticalBox, widget, "IWBHeaderContent")
    header.add_child(header_content)
    _add(header_content, _text(widget, "IWBKicker", "SEED-DRIVEN ENVIRONMENT TOOL", True, leaf, 9), _margin(0, 0, 0, 1))
    _add(header_content, _text(widget, "IWBTitle", "WORLD BUILDER", True, cream, 23), _margin(0, 0, 0, 0))
    _add(header_content, _text(widget, "IWBSubtitle", "Shape a playable low-poly scene, then keep the seed to return to it.", False, leaf, 11), _margin(0, 0, 0, 0))
    _add(panel, header, _margin(8, 10, 8, 5))
    _notice(widget, panel, "QuickStartNotice", "QUICK START  ·  Set a seed  →  choose a mood  →  build a world", "success")

    _find(widget, "QuickStartNoticeText").set_text("QUICK START  |  Set a seed -> choose a mood -> build a world")
    # Four tabs keep the workflow legible: shape the place, compose its ecology,
    # set its atmosphere, then commit the build.
    tabs = _new(unreal.HorizontalBox, widget, "IWBTabBar")
    for name, label in (
        ("WorldTabButton", "WORLD"), ("NatureTabButton", "NATURE"),
        ("MoodTabButton", "MOOD"), ("BuildTabButton", "BUILD"),
    ):
        _add(tabs, _button(widget, name, label, "forest" if name == "WorldTabButton" else "soft"), _margin(2, 4, 2, 4), fill=True)
    _add(panel, tabs, _margin(8, 1, 8, 2))
    switcher = _new(unreal.WidgetSwitcher, widget, "IWBTabSwitcher")
    world_tab = _new(unreal.VerticalBox, widget, "IWBWorldTab")
    nature_tab = _new(unreal.VerticalBox, widget, "IWBNatureTab")
    mood_tab = _new(unreal.VerticalBox, widget, "IWBMoodTab")
    build_tab = _new(unreal.VerticalBox, widget, "IWBBuildTab")
    for tab in (world_tab, nature_tab, mood_tab, build_tab):
        switcher.add_child(tab)
    switcher.set_active_widget_index(0)
    _add(panel, switcher, _margin(0, 0, 0, 0))

    world = _section(widget, world_tab, "World", "WORLD SETTINGS", "Name it, choose a season, then keep the seed for repeatable worlds.")
    _field_row(widget, world, "WorldName", "World name", "Whispering Pines")
    _field_row(widget, world, "Seed", "Seed", "20261018")
    seed_tools = _new(unreal.HorizontalBox, widget, "SeedTools")
    _add(seed_tools, _text(widget, "SeedHint", "A seed lets you recreate this exact layout.", False, muted, 10), _margin(8, 4, 5, 4), fill=True)
    _add(seed_tools, _button(widget, "NewSeedButton", "New seed", "soft"), _margin(3, 2, 5, 2))
    _add(world, seed_tools)
    theme_row = _new(unreal.HorizontalBox, widget, "ThemeRow")
    theme_label = _text(widget, "ThemeLabel", "Season & mood", size=12)
    theme_label.set_min_desired_width(112.0)
    _add(theme_row, theme_label, _margin(8, 3, 4, 3))
    theme = _new(unreal.ComboBoxString, widget, "ThemeBox")
    for option in ("Emberwild", "Spring", "Summer", "Autumn", "Winter"):
        theme.add_option(option)
    theme.set_selected_option("Emberwild")
    _add(theme_row, theme, _margin(0, 3, 8, 3), fill=True)
    _add(world, theme_row)
    _notice(widget, world, "ThemeHint", "Emberwild is the signature look: crimson grass, orange foliage, ivory bark, and blue-violet stone. Build or Preview applies every palette.")

    land = _section(widget, world_tab, "Land", "LAND & PATH", "The terrain is built as a clearing, rolling knolls, and a protected walkable path.")
    _slider_row(widget, land, "Terrain", "World scale", 12000.0, 3000.0, 30000.0)
    mountain_row = _new(unreal.HorizontalBox, widget, "MountainRow")
    mountains = _new(unreal.CheckBox, widget, "MountainsBox")
    mountains.set_is_checked(True)
    _add(mountain_row, mountains, _margin(8, 5, 5, 5))
    _add(mountain_row, _text(widget, "MountainsLabel", "Add low-poly mountains at the edges", size=12), _margin(0, 5, 8, 5), fill=True)
    _add(land, mountain_row)

    life = _section(widget, panel, "Life", "LIFE & DENSITY", "Control each layer separately — the terrain is not rebuilt when you refresh vegetation.")
    life_card = widget.find_child_widget_by_name("LifeCard")
    panel.remove_child(life_card)
    _add(nature_tab, life_card, _margin(8, 4, 8, 4))
    _find(widget, "LifeTitle").set_text("ECOLOGY PLAN")
    _find(widget, "LifeCaption").set_text("Anchor the canopy; meadow layers follow in a deliberate proportion.")
    _slider_row(widget, life, "Trees", "Canopy trees", 60.0, 8.0, 160.0)
    _slider_row(widget, life, "FlowerRatio", "Flowers / 10 trees", 25.0, 0.0, 50.0)
    _slider_row(widget, life, "Rocks", "Rock accents", 28.0, 0.0, 100.0)
    _slider_row(widget, life, "GrassRatio", "Grass / 10 trees", 10.0, 0.0, 40.0)
    _add(life, _text(widget, "RuleNote", "Example: 20 trees + 25 flowers/10 trees + 10 grass/10 trees builds 20 trees, 50 flowers, and 20 grass. Groves, meadow clearings, and rock outcrops are placed from the same terrain map.", False, muted, 10), _margin(8, 2, 8, 3))
    density_tools = _new(unreal.HorizontalBox, widget, "DensityTools")
    _add(density_tools, _button(widget, "CompactProfileButton", "Compact", "soft"), _margin(3, 2, 3, 2), fill=True)
    _add(density_tools, _button(widget, "BalancedProfileButton", "Balanced", "soft"), _margin(3, 2, 3, 2), fill=True)
    _add(density_tools, _button(widget, "LushProfileButton", "Lush", "forest"), _margin(3, 2, 3, 2), fill=True)
    _add(life, density_tools)
    estimate = _new(unreal.Border, widget, "BuildEstimate")
    try:
        estimate.set_brush_color(unreal.LinearColor(0.035, 0.090, 0.060, 1.0))
        estimate.set_padding(_margin(8, 6, 8, 6))
    except Exception:
        pass
    estimate_content = _new(unreal.VerticalBox, widget, "BuildEstimateContent")
    estimate.add_child(estimate_content)
    _add(estimate_content, _text(widget, "BuildEstimateLabel", "BUILD ESTIMATE", True, leaf, 9), _margin(0, 0, 0, 1))
    _add(estimate_content, _text(widget, "BuildSummaryText", "Lush scene · 463 placements · wide scale", False, cream, 11))
    _add(life, estimate, _margin(3, 5, 3, 2))
    _find(widget, "BuildSummaryText").set_text("Balanced scene | linked ecology | wide scale")

    atmosphere = _section(widget, mood_tab, "Atmosphere", "MOOD & WEATHER", "Tune the light and color after you are happy with the composition.")
    _slider_row(widget, atmosphere, "Lighting", "Sun strength", 8.0, 1.0, 15.0, True)
    _slider_row(widget, atmosphere, "SkyTint", "Sky warmth", 0.55, 0.0, 1.0, True)
    _slider_row(widget, atmosphere, "GroundTint", "Ground richness", 0.32, 0.0, 1.0, True)
    _slider_row(widget, atmosphere, "LightTint", "Sun warmth", 0.12, 0.0, 1.0, True)
    weather_row = _new(unreal.HorizontalBox, widget, "WeatherRow")
    weather_label = _text(widget, "WeatherLabel", "Weather", size=12)
    weather_label.set_min_desired_width(112.0)
    _add(weather_row, weather_label, _margin(8, 3, 4, 3))
    weather = _new(unreal.ComboBoxString, widget, "WeatherBox")
    for option in ("Clear", "Rain", "Snow"):
        weather.add_option(option)
    weather.set_selected_option("Clear")
    _add(weather_row, weather, _margin(0, 3, 8, 3), fill=True)
    _add(atmosphere, weather_row)
    _notice(widget, atmosphere, "WeatherHint", "Rain and snow use lightweight animated weather strips so they stay visible while playing.")
    character_row = _new(unreal.HorizontalBox, widget, "CharacterRow")
    character = _new(unreal.CheckBox, widget, "PlayableCharacterBox")
    character.set_is_checked(True)
    _add(character_row, character, _margin(8, 5, 5, 5))
    _add(character_row, _text(widget, "PlayableCharacterLabel", "Keep the Third Person character ready to explore", size=12), _margin(0, 5, 8, 5), fill=True)
    _add(atmosphere, character_row)

    actions = _section(widget, build_tab, "Actions", "BUILD WORLD", "Build replaces the current World Builder result. Update World safely rebuilds every layer after you change a tab.")
    primary = _new(unreal.HorizontalBox, widget, "PrimaryButtons")
    _add(primary, _button(widget, "GenerateButton", "Build World", "primary"), _margin(3, 3, 3, 3), fill=True)
    _add(primary, _button(widget, "PreviewButton", "Preview", "soft"), _margin(3, 3, 3, 3), fill=True)
    _add(actions, primary)
    _notice(widget, actions, "BuildHint", "Build creates a new scene. Update World safely rebuilds every layer after you edit terrain, season, weather, or density.")
    utility = _new(unreal.HorizontalBox, widget, "UtilityButtons")
    _add(utility, _button(widget, "UpdateVegetationButton", "Update World", "soft"), _margin(3, 2, 3, 2), fill=True)
    _add(utility, _button(widget, "ImportNatureButton", "Repair Nature Kit", "soft"), _margin(3, 2, 3, 2), fill=True)
    _add(utility, _button(widget, "ImportTownButton", "Import Town Kit", "soft"), _margin(3, 2, 3, 2), fill=True)
    _add(utility, _button(widget, "StartOverButton", "Start Over", "clay"), _margin(3, 2, 3, 2), fill=True)
    _add(actions, utility)
    presets = _new(unreal.HorizontalBox, widget, "PresetButtons")
    for name, label, tone in (("SavePresetButton", "Save Preset", "soft"), ("LoadPresetButton", "Load Preset", "soft"), ("ResetButton", "Reset", "soft"), ("DeleteButton", "Delete World", "danger")):
        _add(presets, _button(widget, name, label, tone), _margin(3, 2, 3, 2), fill=True)
    _add(actions, presets)
    status = _new(unreal.Border, widget, "StatusCard")
    try:
        status.set_brush_color(unreal.LinearColor(0.055, 0.105, 0.074, 1.0))
        status.set_padding(_margin(10, 7, 10, 7))
    except Exception:
        pass
    status_content = _new(unreal.VerticalBox, widget, "StatusContent")
    status.add_child(status_content)
    _add(status_content, _text(widget, "StatusLabel", "WORLD BUILDER STATUS", True, leaf, 9), _margin(0, 0, 0, 1))
    _add(status_content, _text(widget, "StatusText", "Ready to grow a forest.", False, cream, 11))
    _add(build_tab, status, _margin(11, 7, 11, 12))
    _summary(widget)
    widget.invalidate_layout_and_volatility()
    widget.force_layout_prepass()


def _status(widget, message):
    _find(widget, "StatusText").set_text(message)
    unreal.log("Interactive World Builder: " + message)


def _refresh_readouts(widget):
    fractional = {"Lighting", "SkyTint", "GroundTint", "LightTint"}
    for key in ("Terrain", "Trees", "FlowerRatio", "Rocks", "GrassRatio", "Lighting", "SkyTint", "GroundTint", "LightTint"):
        value = _find(widget, key + "Slider").get_value()
        _find(widget, key + "Readout").set_text(f"{value:.2f}" if key in fractional else str(round(value)))
    _summary(widget)


def _values(widget):
    _refresh_readouts(widget)
    try:
        seed = int(str(_find(widget, "SeedBox").get_text()).strip())
    except ValueError:
        seed = 20261018
    return {
        "world_name": str(_find(widget, "WorldNameBox").get_text()).strip() or "Untitled Forest",
        "seed": seed, "theme": str(_find(widget, "ThemeBox").get_selected_option()),
        "terrain": _find(widget, "TerrainSlider").get_value(),
        "trees": round(_find(widget, "TreesSlider").get_value()),
        "flower_ratio": round(_find(widget, "FlowerRatioSlider").get_value()),
        "rocks": round(_find(widget, "RocksSlider").get_value()),
        "grass_ratio": round(_find(widget, "GrassRatioSlider").get_value()),
        "lighting": _find(widget, "LightingSlider").get_value(),
        "sky_tint": _find(widget, "SkyTintSlider").get_value(),
        "ground_tint": _find(widget, "GroundTintSlider").get_value(),
        "light_tint": _find(widget, "LightTintSlider").get_value(),
        "mountains": _find(widget, "MountainsBox").is_checked(),
        "playable_character": _find(widget, "PlayableCharacterBox").is_checked(),
        "weather": str(_find(widget, "WeatherBox").get_selected_option()),
    } | {
        "flowers": round(_find(widget, "TreesSlider").get_value() * _find(widget, "FlowerRatioSlider").get_value() / 10.0),
        "grass": round(_find(widget, "TreesSlider").get_value() * _find(widget, "GrassRatioSlider").get_value() / 10.0),
    }


def new_seed(widget):
    """Choose a legible random seed without changing the current art direction."""
    seed = random.SystemRandom().randint(10000000, 99999999)
    _find(widget, "SeedBox").set_text(str(seed))
    _status(widget, "New seed {} is ready. Build World to grow a different layout.".format(seed))


def apply_density_profile(widget, profile):
    """Give new users useful starting points, while preserving their name, seed, and mood."""
    profiles = {
        "compact": {
            "Terrain": 6000.0, "Trees": 32.0, "FlowerRatio": 18.0,
            "Rocks": 15.0, "GrassRatio": 9.0,
        },
        "balanced": {
            "Terrain": 14000.0, "Trees": 60.0, "FlowerRatio": 25.0,
            "Rocks": 28.0, "GrassRatio": 10.0,
        },
        "lush": {
            "Terrain": 24000.0, "Trees": 120.0, "FlowerRatio": 34.0,
            "Rocks": 65.0, "GrassRatio": 18.0,
        },
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


def _hide_template_environment():
    """Hide the Third Person template geometry while keeping its light and player start alive."""
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if any(_has_tag(actor, tag) for tag in (GENERATED_TAG, PREVIEW_TAG, TEMPLATE_HIDDEN_TAG)):
            continue
        if isinstance(actor, unreal.StaticMeshActor):
            tags = list(actor.get_editor_property("tags"))
            tags.append(unreal.Name(TEMPLATE_HIDDEN_TAG))
            actor.set_editor_property("tags", tags)
            actor.set_is_temporarily_hidden_in_editor(True)
            actor.set_actor_hidden_in_game(True)


def _restore_template_environment():
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if _has_tag(actor, TEMPLATE_HIDDEN_TAG):
            actor.set_editor_property("tags", [tag for tag in actor.get_editor_property("tags") if str(tag) != TEMPLATE_HIDDEN_TAG])
            actor.set_is_temporarily_hidden_in_editor(False)
            actor.set_actor_hidden_in_game(False)


def _tag(actor, generated_tag, layer, label):
    actor.set_editor_property("tags", [unreal.Name(generated_tag), unreal.Name(layer)])
    actor.set_actor_label(label)
    return actor


def _get_tint_material():
    """Create one real parameterized material instead of hoping BasicShapeMaterial exposes a color parameter."""
    global _tint_material
    if _tint_material:
        return _tint_material
    material_path = "/Game/WorldBuilder/Materials/M_IWB_Tint"
    material = unreal.load_asset(material_path)
    if not material:
        tools = unreal.AssetToolsHelpers.get_asset_tools()
        material = tools.create_asset(
            "M_IWB_Tint",
            "/Game/WorldBuilder/Materials",
            unreal.Material,
            unreal.MaterialFactoryNew(),
        )
        if not material:
            raise RuntimeError("Could not create the World Builder tint material")
        tint = unreal.MaterialEditingLibrary.create_material_expression(
            material, unreal.MaterialExpressionVectorParameter, -380, 0
        )
        tint.set_editor_property("parameter_name", "Tint")
        tint.set_editor_property("default_value", unreal.LinearColor(0.12, 0.42, 0.12, 1.0))
        roughness = unreal.MaterialEditingLibrary.create_material_expression(
            material, unreal.MaterialExpressionConstant, -380, 150
        )
        roughness.set_editor_property("r", 0.82)
        unreal.MaterialEditingLibrary.connect_material_property(tint, "", unreal.MaterialProperty.MP_BASE_COLOR)
        unreal.MaterialEditingLibrary.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
        unreal.MaterialEditingLibrary.recompile_material(material)
        unreal.EditorAssetLibrary.save_loaded_asset(material)
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


def _apply_mesh_tint(actor, tint, trunk_tint=None):
    """Tint every slot, while preserving a separate bark/trunk color on trees.

    Kenney trees use leaf and wood material slots.  Replacing only slot zero
    made the imported foliage inconsistent and could never produce the ivory
    trunk / orange canopy contrast of the artistic palette.
    """
    if not tint:
        return
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    if not component:
        return
    try:
        slot_count = max(1, component.get_num_materials())
    except Exception:
        slot_count = 1
    for index in range(slot_count):
        color = tint
        try:
            source_material = component.get_material(index)
            source_name = source_material.get_name().lower() if source_material else ""
            if trunk_tint and any(token in source_name for token in ("bark", "wood", "trunk", "stump", "log")):
                color = trunk_tint
        except Exception:
            pass
        try:
            component.set_material(index, _get_tint_material())
            material = component.create_dynamic_material_instance(index)
            if material:
                material.set_vector_parameter_value("Tint", color)
        except Exception as error:
            unreal.log_warning("World Builder tint skipped on {} slot {}: {}".format(actor.get_name(), index, error))


def _spawn_mesh(mesh, location, scale, rotation, generated_tag, layer, label, tint=None, trunk_tint=None):
    actor = unreal.EditorLevelLibrary.spawn_actor_from_object(mesh, location, rotation)
    actor.set_actor_scale3d(scale)
    _tag(actor, generated_tag, layer, label)
    _apply_mesh_tint(actor, tint, trunk_tint)
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
    """Extract and import only the models the generator uses, from the user's zip."""
    if _kenney_is_imported() and not force:
        return True, "Nature Kit is already ready."
    archive_path = _kenney_zip_path()
    if not os.path.isfile(archive_path):
        return False, "Nature Kit zip not found in Downloads."
    source_dir = os.path.join(unreal.Paths.project_intermediate_dir(), "WorldBuilderNatureSource")
    os.makedirs(source_dir, exist_ok=True)
    try:
        with zipfile.ZipFile(archive_path, "r") as archive:
            for name in KENNEY_FILES:
                member = "Models/FBX format/{}.fbx".format(name)
                destination = os.path.join(source_dir, name + ".fbx")
                with archive.open(member) as source, open(destination, "wb") as target:
                    target.write(source.read())
    except (OSError, KeyError, zipfile.BadZipFile) as error:
        return False, "Could not unpack Nature Kit: {}".format(error)

    tasks = []
    for name in KENNEY_FILES:
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
    _status(widget, "Importing selected Nature Kit models…")
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
    _status(widget, "Importing selected Fantasy Town landmarks…")
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


def _spawn_grounded_mesh(mesh, location_xy, height, rotation, generated_tag, layer, label, tint=None, penetration=0.0, trunk_tint=None):
    """Scale and ground a Z-up imported mesh using its actual transformed bounds."""
    # A malformed FBX import can expose a material under a mesh name. Skipping
    # it is always better than leaving the user with a half-generated world.
    if not isinstance(mesh, unreal.StaticMesh):
        unreal.log_warning("World Builder skipped a non-mesh asset in {}".format(label))
        return None
    bounds = mesh.get_bounds()
    mesh_height = max(1.0, bounds.box_extent.z * 2.0)
    scale = height / mesh_height
    actor = _spawn_mesh(
        mesh,
        unreal.Vector(location_xy.x, location_xy.y, location_xy.z),
        unreal.Vector(scale, scale, scale),
        rotation,
        generated_tag,
        layer,
        label,
        tint,
        trunk_tint,
    )
    _snap_actor_to_ground(actor, location_xy.z, penetration)
    return actor


def _spawn_stylized_tree(mesh, location_xy, height, rotation, generated_tag, foliage_tint=None, trunk_tint=None):
    # A tiny overlap hides numerical seams without burying the visible trunk.
    return _spawn_grounded_mesh(
        mesh, location_xy, height, rotation, generated_tag, TREE_TAG,
        "IWB_StylizedTree", foliage_tint, 4.0, trunk_tint,
    )


def _elevation(x, y, terrain):
    frequency = 6.25 / terrain
    return 0.5 + 0.5 * math.sin(x * frequency) * math.cos(y * frequency * 1.18)


def _slope(x, y, terrain):
    frequency = 6.25 / terrain
    return abs(math.cos(x * frequency) * math.sin(y * frequency * 1.18))


def _terrain_features(data):
    """Deterministic, broad low-poly knolls that keep the middle path walkable."""
    rng = random.Random(data["seed"] + 2718)
    terrain = data["terrain"]
    half = terrain * 0.5
    path_half = max(180.0, min(500.0, terrain * 0.045))
    features = []
    # Larger worlds get a few more shapes instead of one giant, artificial hill.
    count = 6 + int(max(0.0, terrain - 5000.0) / 3000.0)
    for _ in range(count):
        y_sign = -1.0 if rng.random() < 0.5 else 1.0
        radius = rng.uniform(terrain * 0.065, terrain * 0.125)
        x = rng.uniform(-half * 0.72, half * 0.72)
        # Keep the entire hill clear of the central route, not just its centre.
        y = y_sign * rng.uniform(path_half * 1.8 + radius, half * 0.76)
        height = rng.uniform(180.0, 380.0) * min(2.4, max(0.8, terrain / 8000.0))
        features.append((x, y, radius, height))
    return features


def _ground_height(x, y, features):
    """Height of the visible upper hemisphere for the generated knolls."""
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


def _group_centres(rng, count, terrain, path_half, label):
    """Lay out readable composition zones: groves, meadows, then outcrops."""
    half = terrain * 0.5
    centres = []
    for index in range(max(1, count)):
        side = -1.0 if (index + (0 if label == "grove" else 1)) % 2 else 1.0
        x = rng.uniform(-half * 0.70, half * 0.70)
        y = side * rng.uniform(path_half * 1.55, half * 0.74)
        centres.append((x, y))
    return centres


def _sample_group_point(rng, centres, spread, half):
    """Sample an organic disk around a logical zone instead of the whole map."""
    center_x, center_y = centres[rng.randrange(len(centres))]
    angle = rng.uniform(0.0, math.tau)
    distance = math.sqrt(rng.random()) * spread * rng.uniform(0.58, 1.08)
    return (
        max(-half * 0.93, min(half * 0.93, center_x + math.cos(angle) * distance)),
        max(-half * 0.93, min(half * 0.93, center_y + math.sin(angle) * distance)),
    )


def _finish_palette(colors):
    """Supply shared paint treatments without flattening each world direction."""
    colors.setdefault("trunk", unreal.LinearColor(0.34, 0.18, 0.08, 1.0))
    # Bark is intentionally a two-tone design choice, rather than the usual
    # natural brown.  The seeded selection in _build_vegetation gives every
    # world a stable mix of chalk-white and ink-black trunks.
    colors.setdefault("trunk_light", unreal.LinearColor(0.94, 0.89, 0.76, 1.0))
    colors.setdefault("trunk_dark", unreal.LinearColor(0.018, 0.012, 0.035, 1.0))
    colors.setdefault("path", unreal.LinearColor(0.18, 0.10, 0.055, 1.0))
    return colors


def _palette(data):
    """Art directions applied across terrain, plants, stone, sky, and sunlight."""
    ground_richness = data["ground_tint"]
    theme = data["theme"]
    if theme == "Emberwild":
        # The primary portfolio look: intentionally impossible, readable color
        # families rather than another natural-green forest.
        return _finish_palette({
            "ground": unreal.LinearColor(0.095 + ground_richness * 0.10, 0.028 + ground_richness * 0.035, 0.17 + ground_richness * 0.13, 1.0),
            "ground_mound": unreal.LinearColor(0.26, 0.075, 0.34, 1.0),
            "leaf": unreal.LinearColor(1.0, 0.18, 0.025, 1.0),
            "grass": unreal.LinearColor(0.78, 0.025, 0.10, 1.0),
            "mountain": unreal.LinearColor(0.095, 0.075, 0.27, 1.0),
            "rock": unreal.LinearColor(0.075, 0.27, 0.43, 1.0),
            "accent_foliage": unreal.LinearColor(0.96, 0.055, 0.22, 1.0),
            "flower_a": unreal.LinearColor(0.05, 0.84, 0.74, 1.0),
            "flower_b": unreal.LinearColor(1.0, 0.73, 0.17, 1.0),
            "sky": unreal.LinearColor(0.23, 0.10, 0.48, 1.0),
            "sun": unreal.LinearColor(1.0, 0.45, 0.22, 1.0),
            "trunk": unreal.LinearColor(0.96, 0.86, 0.66, 1.0),
            "trunk_light": unreal.LinearColor(0.98, 0.92, 0.78, 1.0),
            "trunk_dark": unreal.LinearColor(0.012, 0.006, 0.028, 1.0),
            "path": unreal.LinearColor(0.31, 0.035, 0.09, 1.0),
        })
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


def _build_base(data, tag, origin_x):
    cube = unreal.load_asset("/Engine/BasicShapes/Cube")
    sphere = unreal.load_asset("/Engine/BasicShapes/Sphere")
    terrain = data["terrain"]
    palette = _palette(data)
    # A UE BasicShape cube is 100 units high.  Centre it half its thickness
    # below the surface so the top is exactly GROUND_SURFACE_Z, which is the
    # height used by every vegetation and rock placement.
    base_scale_z = TERRAIN_THICKNESS / 100.0
    base = _spawn_mesh(
        cube,
        unreal.Vector(origin_x, 0, GROUND_SURFACE_Z - TERRAIN_THICKNESS * 0.5),
        unreal.Vector(terrain / 100.0, terrain / 100.0, base_scale_z),
        unreal.Rotator(), tag, TERRAIN_TAG, "IWB_Terrain", palette["ground"],
    )
    _enable_world_collision(base)
    features = _terrain_features(data)
    if sphere:
        for index, (x, y, radius, height) in enumerate(features):
            knoll = _spawn_mesh(
                sphere,
                unreal.Vector(origin_x + x, y, 0),
                unreal.Vector(radius / 50.0, radius / 50.0, height / 50.0),
                unreal.Rotator(0, (index * 41) % 360, 0),
                tag,
                TERRAIN_TAG,
                "IWB_TerrainKnoll",
                palette["ground_mound"],
            )
            _enable_world_collision(knoll)
    return features


def _spawn_mountain_blocker(sphere, location, radius, height, generated_tag):
    """Give decorative imported cliff meshes a guaranteed solid body in Play."""
    if not sphere:
        return
    blocker = _spawn_mesh(
        sphere, location,
        unreal.Vector(radius / 50.0, radius / 50.0, height / 50.0),
        unreal.Rotator(), generated_tag, MOUNTAIN_TAG, "IWB_MountainCollision",
    )
    _enable_world_collision(blocker)
    try:
        # It participates in physics but never renders; the visible rock remains
        # the art direction while this simple convex body handles the player.
        blocker.set_actor_hidden_in_game(True)
        blocker.set_is_temporarily_hidden_in_editor(True)
    except Exception:
        pass


def _build_mountains(data, tag, origin_x, terrain_features):
    if not data["mountains"]:
        return 0
    sphere = unreal.load_asset("/Engine/BasicShapes/Sphere")
    # The actual Nature Kit cliffs replace the old primitive cones.
    cliff_meshes = _kenney_meshes(("cliff_blockSlope_rock", "cliff_large_rock", "cliff_top_rock"))
    fallback_rocks = _kenney_meshes(("rock_largeB", "rock_largeE", "rock_smallA"))
    rng, terrain, palette = random.Random(data["seed"] + 415), data["terrain"], _palette(data)
    half, total = terrain * 0.5, 0
    # A distant irregular rim frames the clearing while preserving a safe route.
    for _ in range(7 + int(terrain / 7000.0)):
        angle = rng.uniform(0.0, math.tau)
        distance = rng.uniform(half * 0.80, half * 0.94)
        x, y = math.cos(angle) * distance, math.sin(angle) * distance
        height = rng.uniform(380.0, 760.0)
        rotation = unreal.Rotator(0.0, rng.uniform(0, 360), 0.0)
        ground_z = _ground_height(x, y, terrain_features)
        rocks = cliff_meshes or fallback_rocks
        if rocks:
            _spawn_grounded_mesh(
                rocks[rng.randrange(len(rocks))],
                unreal.Vector(origin_x + x, y, ground_z), height, rotation, tag, MOUNTAIN_TAG, "IWB_Cliff", palette["mountain"],
            )
            _spawn_mountain_blocker(
                sphere, unreal.Vector(origin_x + x, y, ground_z),
                max(380.0, min(terrain * 0.10, height * 0.95)), height, tag,
            )
        elif sphere:
            radius = rng.uniform(terrain * 0.055, terrain * 0.095)
            _spawn_mesh(
                sphere,
                unreal.Vector(origin_x + x, y, ground_z),
                unreal.Vector(radius / 50.0, radius / 50.0, height / 50.0),
                rotation,
                tag,
                MOUNTAIN_TAG,
                "IWB_MountainFallback",
                palette["mountain"],
            )
        total += 1
    return total


def _build_vegetation(data, tag, origin_x, terrain_features):
    rng, terrain = random.Random(data["seed"]), data["terrain"]
    palette = _palette(data)
    half, path_half = terrain * 0.5, max(180.0, min(500.0, terrain * 0.045))
    cube = unreal.load_asset("/Engine/BasicShapes/Cube")
    cone = unreal.load_asset("/Engine/BasicShapes/Cone")
    cylinder = unreal.load_asset("/Engine/BasicShapes/Cylinder")
    sphere = unreal.load_asset("/Engine/BasicShapes/Sphere")
    tree_names = ("tree_default", "tree_detailed", "tree_oak", "tree_pineRoundA", "tree_pineTallB")
    if data["theme"] == "Autumn":
        tree_names = ("tree_default_fall", "tree_detailed_fall", "tree_oak_fall", "tree_oak")
    elif data["theme"] == "Winter":
        tree_names = ("tree_pineRoundA", "tree_pineRoundC", "tree_pineTallA", "tree_pineTallB")
    tree_meshes = _kenney_meshes(tree_names) or _pcg_tree_meshes()
    rock_meshes = _kenney_meshes(("rock_largeB", "rock_largeE", "rock_smallA", "rock_smallH"))
    hilltop_meshes = _kenney_meshes(("stump_old", "stump_roundDetailed", "log_stack", "mushroom_redGroup", "mushroom_tanGroup"))
    town_meshes = _fantasy_town_meshes()
    boulder = unreal.load_asset("/PCG/SampleContent/SimpleForest/Meshes/PCG_Boulder_02")
    grass_meshes = _kenney_meshes(("grass", "grass_large", "grass_leafs", "grass_leafsLarge"))
    flower_meshes = _kenney_meshes(("flower_purpleA", "flower_purpleC", "flower_redB", "flower_yellowC"))
    bush_meshes = _kenney_meshes(("plant_bush", "plant_bushDetailed", "plant_bushLarge", "plant_bushSmall", "plant_bushTriangle"))
    if not all((cube, cone, cylinder, sphere)):
        raise RuntimeError("Unreal Engine basic-shape meshes are unavailable")

    # Composition is authored as zones.  A repeatable noise field softens their
    # boundaries, but no layer is sprayed evenly across the entire square world.
    groves = _group_centres(rng, max(3, round(data["trees"] / 14.0)), terrain, path_half, "grove")
    meadows = _group_centres(rng, max(2, round(data["flowers"] / 45.0)), terrain, path_half, "meadow")
    outcrops = _group_centres(rng, max(2, round(data["rocks"] / 8.0)), terrain, path_half, "outcrop")

    trees = 0
    for _ in range(data["trees"] * 6):
        if trees >= data["trees"]:
            break
        x, y = _sample_group_point(rng, groves, terrain * 0.105, half)
        ground_z = _ground_height(x, y, terrain_features)
        if ground_z > 190.0 or _ground_slope(x, y, terrain_features) > 0.55 or abs(y) < path_half or _terrain_noise(x, y, data["seed"], terrain * 0.10) < 0.27:
            continue
        height = rng.uniform(380.0, 660.0)
        location = unreal.Vector(origin_x + x, y, ground_z)
        rotation = unreal.Rotator(0, rng.uniform(0, 360), 0)
        # White trunks are the dominant silhouette; a substantial minority of
        # black trunks prevents the canopy from reading as a uniform asset set.
        trunk_tint = palette["trunk_dark"] if rng.random() < 0.38 else palette["trunk_light"]
        if tree_meshes:
            _spawn_stylized_tree(
                tree_meshes[rng.randrange(len(tree_meshes))], location, height,
                rotation, tag, palette["leaf"], trunk_tint,
            )
        else:
            _spawn_mesh(cylinder, unreal.Vector(origin_x + x, y, ground_z + height * 0.5), unreal.Vector(rng.uniform(0.12, 0.18), rng.uniform(0.12, 0.18), height / 100.0), rotation, tag, TREE_TAG, "IWB_TreeTrunk", trunk_tint)
            _spawn_mesh(cone, unreal.Vector(origin_x + x, y, ground_z + height + 110.0), unreal.Vector(rng.uniform(0.62, 1.0), rng.uniform(0.62, 1.0), rng.uniform(1.2, 1.8)), rotation, tag, TREE_TAG, "IWB_TreeCrown", palette["leaf"])
        trees += 1

    # Low shrubs break up the tree/grass scale and carry the visible season accent.
    bushes = 0
    bush_target = max(10, round(data["grass"] * 0.08))
    for _ in range(bush_target * 5):
        if bushes >= bush_target:
            break
        x, y = _sample_group_point(rng, groves, terrain * 0.13, half)
        ground_z = _ground_height(x, y, terrain_features)
        if abs(y) < path_half * 1.15 or ground_z > 175.0 or _terrain_noise(x, y, data["seed"] + 91, terrain * 0.10) < 0.20:
            continue
        if bush_meshes:
            _spawn_grounded_mesh(
                bush_meshes[rng.randrange(len(bush_meshes))], unreal.Vector(origin_x + x, y, ground_z),
                rng.uniform(75.0, 155.0), unreal.Rotator(0, rng.uniform(0, 360), 0),
                tag, GRASS_TAG, "IWB_SeasonalShrub", palette["accent_foliage"], 3.0,
            )
            bushes += 1

    rocks = 0
    attempts = 0
    while rocks < data["rocks"] and attempts < data["rocks"] * 12:
        attempts += 1
        center_x, center_y = _sample_group_point(rng, outcrops, terrain * 0.075, half)
        if abs(center_y) < path_half * 1.4:
            continue
        for _ in range(min(rng.randint(2, 5), data["rocks"] - rocks)):
            x, y = center_x + rng.uniform(-260, 260), center_y + rng.uniform(-260, 260)
            ground_z = _ground_height(x, y, terrain_features)
            rock_mesh = rock_meshes[rng.randrange(len(rock_meshes))] if rock_meshes else (boulder or sphere)
            _spawn_grounded_mesh(
                rock_mesh, unreal.Vector(origin_x + x, y, ground_z), rng.uniform(80.0, 240.0),
                unreal.Rotator(0, rng.uniform(0, 360), 0), tag, ROCK_TAG, "IWB_Rock",
                palette["rock"],
            )
            rocks += 1

    # Hilltops are deliberately spare, but never empty.  These landmarks make
    # the knolls feel authored and give the player visual destinations.
    hilltop_accents = 0
    for index, (x, y, _radius, peak) in enumerate(terrain_features):
        if not hilltop_meshes or peak < 150.0:
            continue
        mesh = hilltop_meshes[index % len(hilltop_meshes)]
        _spawn_grounded_mesh(
            mesh,
            unreal.Vector(origin_x + x, y, peak),
            rng.uniform(105.0, 175.0),
            unreal.Rotator(0, rng.uniform(0, 360), 0),
            tag, MOUNTAIN_TAG, "IWB_HilltopLandmark",
            palette["accent_foliage"], 2.0,
            palette["trunk_dark"] if index % 3 == 0 else palette["trunk_light"],
        )
        hilltop_accents += 1

    # Imported buildings are rare landmarks, not a procedurally scattered town.
    # Placing them on the highest knolls gives the player destinations while
    # preserving broad, explorable red-grass clearings between them.
    town_landmarks = 0
    ranked_hilltops = sorted(enumerate(terrain_features), key=lambda feature: feature[1][3], reverse=True)
    for landmark_index, (feature_index, (x, y, _radius, peak)) in enumerate(ranked_hilltops):
        if not town_meshes or town_landmarks >= 4:
            break
        if peak < 125.0:
            continue
        mesh = town_meshes[(data["seed"] + feature_index) % len(town_meshes)]
        _spawn_grounded_mesh(
            mesh,
            unreal.Vector(origin_x + x, y, peak),
            rng.uniform(260.0, 460.0),
            unreal.Rotator(0, rng.uniform(0, 360), 0),
            tag, MOUNTAIN_TAG, "IWB_TownLandmark",
            palette["mountain"], 2.0,
            palette["trunk_dark"] if landmark_index % 2 else palette["trunk_light"],
        )
        town_landmarks += 1

    grass = 0
    for _ in range(data["grass"] * 5):
        if grass >= data["grass"]:
            break
        x, y = _sample_group_point(rng, groves + meadows, terrain * 0.14, half)
        ground_z = _ground_height(x, y, terrain_features)
        if abs(y) < path_half or ground_z > 210.0 or _terrain_noise(x, y, data["seed"] + 207, terrain * 0.09) < 0.23:
            continue
        location = unreal.Vector(origin_x + x, y, ground_z)
        rotation = unreal.Rotator(0, rng.uniform(0, 360), 0)
        if grass_meshes:
            _spawn_grounded_mesh(grass_meshes[rng.randrange(len(grass_meshes))], location, rng.uniform(65.0, 140.0), rotation, tag, GRASS_TAG, "IWB_Grass", palette["grass"], 1.0)
        else:
            _spawn_mesh(cone, unreal.Vector(origin_x + x, y, ground_z + 32), unreal.Vector(rng.uniform(0.035, 0.065), rng.uniform(0.035, 0.065), rng.uniform(0.32, 0.70)), rotation, tag, GRASS_TAG, "IWB_Grass", palette["grass"])
        grass += 1

    flowers = 0
    for _ in range(data["flowers"] * 5):
        if flowers >= data["flowers"]:
            break
        x, y = _sample_group_point(rng, meadows, terrain * 0.095, half)
        ground_z = _ground_height(x, y, terrain_features)
        if abs(y) < path_half * 1.2 or ground_z > 165.0 or _terrain_noise(x, y, data["seed"] + 481, terrain * 0.08) < 0.28:
            continue
        location = unreal.Vector(origin_x + x, y, ground_z)
        rotation = unreal.Rotator(0, rng.uniform(0, 360), 0)
        if flower_meshes:
            flower_tint = palette["flower_a"] if flowers % 2 else palette["flower_b"]
            _spawn_grounded_mesh(flower_meshes[rng.randrange(len(flower_meshes))], location, rng.uniform(45.0, 85.0), rotation, tag, FLOWER_TAG, "IWB_Flower", flower_tint, 1.0)
        else:
            stem_height = rng.uniform(20.0, 45.0)
            _spawn_mesh(cylinder, unreal.Vector(origin_x + x, y, ground_z + stem_height * 0.5), unreal.Vector(0.012, 0.012, stem_height / 100.0), rotation, tag, FLOWER_TAG, "IWB_FlowerStem", palette["grass"])
            petal_color = palette["flower_a"] if flowers % 2 else palette["flower_b"]
            petal_size = rng.uniform(0.065, 0.10)
            for dx, dy in ((9, 0), (-9, 0), (0, 9), (0, -9)):
                _spawn_mesh(sphere, unreal.Vector(origin_x + x + dx, y + dy, ground_z + stem_height + 10.0), unreal.Vector(petal_size, petal_size, petal_size * 0.42), unreal.Rotator(), tag, FLOWER_TAG, "IWB_FlowerPetal", petal_color)
            _spawn_mesh(sphere, unreal.Vector(origin_x + x, y, ground_z + stem_height + 12.0), unreal.Vector(0.045, 0.045, 0.025), unreal.Rotator(), tag, FLOWER_TAG, "IWB_FlowerCentre", unreal.LinearColor(0.98, 0.72, 0.16, 1.0))
        flowers += 1
    _spawn_mesh(
        cube,
        unreal.Vector(origin_x, 0, GROUND_SURFACE_Z + PATH_THICKNESS * 0.5),
        unreal.Vector(terrain / 100.0, (path_half * 1.4) / 100.0, PATH_THICKNESS / 100.0),
        unreal.Rotator(), tag, PATH_TAG, "IWB_Path", palette["path"],
    )
    return trees, rocks, grass, flowers, bushes, hilltop_accents, town_landmarks


def _build_weather(data, tag, origin_x):
    """Spawn lightweight mesh weather; its material loops downward during Play."""
    weather = data.get("weather", "Clear")
    if weather == "Clear":
        return 0
    try:
        cube = unreal.load_asset("/Engine/BasicShapes/Cube")
        material = _get_weather_material()
        if not cube or not material:
            return 0
        rng = random.Random(data["seed"] + (911 if weather == "Rain" else 1777))
        terrain = data["terrain"]
        spread = min(terrain * 0.48, 8500.0)
        count = 180 if weather == "Rain" else 130
        tint = unreal.LinearColor(0.42, 0.66, 0.95, 1.0) if weather == "Rain" else unreal.LinearColor(0.92, 0.97, 1.0, 1.0)
        speed = 0.52 if weather == "Rain" else 0.075
        distance = -4200.0 if weather == "Rain" else -1500.0
        for _ in range(count):
            location = unreal.Vector(
                origin_x + rng.uniform(-spread, spread),
                rng.uniform(-spread, spread),
                rng.uniform(700.0, 4400.0),
            )
            scale = unreal.Vector(0.012, 0.012, rng.uniform(0.40, 1.15)) if weather == "Rain" else unreal.Vector(0.040, 0.040, 0.040)
            actor = _spawn_mesh(cube, location, scale, unreal.Rotator(0, rng.uniform(0, 360), 0), tag, WEATHER_TAG, "IWB_" + weather)
            actor.set_actor_enable_collision(False)
            component = actor.get_component_by_class(unreal.StaticMeshComponent)
            if component:
                component.set_material(0, material)
                dynamic_material = component.create_dynamic_material_instance(0)
                if dynamic_material:
                    dynamic_material.set_vector_parameter_value("Tint", tint)
                    dynamic_material.set_scalar_parameter_value("FallSpeed", speed)
                    dynamic_material.set_scalar_parameter_value("FallDistance", distance)
        return count
    except Exception as error:
        # Weather is optional: do not lose a generated world if a material fails.
        unreal.log_warning("World Builder weather skipped: {}".format(error))
        return 0


def _build_lighting_and_player(data, tag, origin_x):
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
    light = next((actor for actor in actors if isinstance(actor, unreal.DirectionalLight)), None)
    if not light:
        light = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(origin_x, 0, 1100), unreal.Rotator(-52, -28, 0))
        _tag(light, tag, LIGHT_TAG, "IWB_ForestLight")
    light_component = light.get_component_by_class(unreal.DirectionalLightComponent)
    if light_component:
        # UE directional lights use a small lux value here, not 10,000× the UI value.
        light_component.set_editor_property("intensity", 1.5 + data["lighting"] * 0.70)
        light_component.set_light_color(light_color)
        sun_angles = {
            "Emberwild": (-34, -42),
            "Spring": (-42, -30), "Summer": (-58, -22),
            "Autumn": (-26, -36), "Winter": (-16, -52),
        }
        pitch, yaw = sun_angles.get(data["theme"], sun_angles["Summer"])
        light.set_actor_rotation(unreal.Rotator(pitch, yaw, 0), False)

    sky = next((actor for actor in actors if isinstance(actor, unreal.SkyLight)), None)
    if not sky:
        sky = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(origin_x, 0, 1050), unreal.Rotator())
        _tag(sky, tag, LIGHT_TAG, "IWB_ForestSky")
    sky_component = sky.get_component_by_class(unreal.SkyLightComponent)
    if sky_component:
        sky_component.set_editor_property("intensity", 0.45 + data["lighting"] * 0.07)
        sky_component.set_light_color(sky_color)
        sky_component.recapture_sky()
    if data["playable_character"]:
        starts = [actor for actor in actors if isinstance(actor, unreal.PlayerStart)]
        if not starts:
            start = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(), unreal.Rotator())
            _tag(start, tag, PLAYER_TAG, "IWB_ThirdPersonStart")
            starts = [start]
        # The template start can be above geometry hidden by this tool. Put all
        # candidate starts on the solid central path to prevent a flying spawn.
        spawn_location = unreal.Vector(origin_x - data["terrain"] * 0.31, 0, 112)
        for start in starts:
            start.set_actor_location(spawn_location, False, True)
            start.set_actor_rotation(unreal.Rotator(0, 0, 0), False)


def _build_world(widget, preview=False, vegetation_only=False):
    data = _values(widget)
    # Keep Preview fully beside the active world, never overlapping its play space.
    tag, origin_x = (PREVIEW_TAG, data["terrain"] * 1.25) if preview else (GENERATED_TAG, 0.0)
    terrain_features = _terrain_features(data)
    weather_count = 0
    if not vegetation_only:
        terrain_features = _build_base(data, tag, origin_x)
        _build_mountains(data, tag, origin_x, terrain_features)
        weather_count = _build_weather(data, tag, origin_x)
    trees, rocks, grass, flowers, bushes, hilltop_accents, town_landmarks = _build_vegetation(data, tag, origin_x, terrain_features)
    if not preview and not vegetation_only:
        _build_lighting_and_player(data, tag, origin_x)
    weather_label = "{} falling particles".format(weather_count) if weather_count else "clear sky"
    if town_landmarks:
        weather_label += " | {} town landmarks".format(town_landmarks)
    _status(widget, "{} '{}' ({}, {}) · {} trees, {} shrubs, {} flowers, {} rocks, {} grass, {} hilltop landmarks.".format("Preview" if preview else "Generated", data["world_name"], data["theme"], weather_label, trees, bushes, flowers, rocks, grass, hilltop_accents))


def generate(widget):
    if not _fantasy_town_is_imported() and os.path.isfile(_fantasy_town_zip_path()):
        _status(widget, "First build: importing selected Fantasy Town landmarks…")
        imported, message = _import_fantasy_town_assets()
        if not imported:
            _status(widget, "Fantasy Town Kit was skipped: " + message)
    if not _kenney_is_imported() and os.path.isfile(_kenney_zip_path()):
        _status(widget, "First build: importing the selected Nature Kit models…")
        imported, message = _import_kenney_assets()
        if not imported:
            _status(widget, "Nature Kit was skipped: " + message)
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
    _status(widget, "World generated. The terrain is selected — press F in the viewport to frame it.")


def preview(widget):
    _clear_world(preview_only=True)
    _build_world(widget, preview=True)


def update_vegetation(widget):
    """Apply every edited control reliably, including scale, terrain, season, and weather."""
    _status(widget, "Updating world…")
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
    _find(widget, "ThemeBox").set_selected_option(legacy_themes.get(data.get("theme", "Emberwild"), data.get("theme", "Emberwild")))
    saved_trees = max(1, round(data.get("trees", 60)))
    flower_ratio = data.get("flower_ratio", round(data.get("flowers", 150) * 10.0 / saved_trees))
    grass_ratio = data.get("grass_ratio", round(data.get("grass", 60) * 10.0 / saved_trees))
    for key, source in (("Terrain", "terrain"), ("Trees", "trees"), ("Rocks", "rocks"), ("Lighting", "lighting"), ("SkyTint", "sky_tint"), ("GroundTint", "ground_tint"), ("LightTint", "light_tint")):
        _find(widget, key + "Slider").set_value(data.get(source, _find(widget, key + "Slider").get_value()))
    _find(widget, "FlowerRatioSlider").set_value(flower_ratio)
    _find(widget, "GrassRatioSlider").set_value(grass_ratio)
    _find(widget, "MountainsBox").set_is_checked(data.get("mountains", True))
    _find(widget, "PlayableCharacterBox").set_is_checked(data.get("playable_character", True))
    _find(widget, "WeatherBox").set_selected_option(data.get("weather", "Clear"))
    _refresh_readouts(widget)
    _status(widget, "Preset loaded. Preview checks it without replacing the generated world.")


def reset(widget):
    _find(widget, "WorldNameBox").set_text("Whispering Pines")
    _find(widget, "SeedBox").set_text("20261018")
    _find(widget, "ThemeBox").set_selected_option("Emberwild")
    defaults = {"Terrain": 14000.0, "Trees": 60.0, "FlowerRatio": 25.0, "Rocks": 28.0, "GrassRatio": 10.0, "Lighting": 8.0, "SkyTint": 0.55, "GroundTint": 0.32, "LightTint": 0.12}
    for key, value in defaults.items():
        _find(widget, key + "Slider").set_value(value)
    _find(widget, "MountainsBox").set_is_checked(True)
    _find(widget, "PlayableCharacterBox").set_is_checked(True)
    _find(widget, "WeatherBox").set_selected_option("Clear")
    _refresh_readouts(widget)
    _status(widget, "Controls reset. Existing generated actors were not changed.")


def delete_world(widget):
    _clear_world()
    _clear_world(preview_only=True)
    _restore_template_environment()
    _status(widget, "Generated actors removed. The original Third Person level is visible again.")


def _make_click_callback(action, widget):
    def callback():
        action(widget)
    return callback


def _make_slider_callback(widget):
    def callback(_value):
        _refresh_readouts(widget)
    return callback


def _make_tab_callback(widget, index):
    def callback():
        _find(widget, "IWBTabSwitcher").set_active_widget_index(index)
    return callback


def _make_theme_callback(widget):
    descriptions = {
        "Emberwild": "crimson grass, orange-coral canopies, ivory bark, and blue-violet stone",
        "Spring": "fresh green ground, bright blue sky, and lively yellow-green shrubs",
        "Summer": "deep forest ground, a high warm sun, and rich green accents",
        "Autumn": "ember ground, low orange sunlight, and warm foliage accents",
        "Winter": "cool blue ground, a pale low sun, and frosted cyan accents",
    }

    def callback(selected_item, _selection_type):
        theme = str(selected_item)
        description = descriptions.get(theme, "a custom seasonal palette")
        _find(widget, "ThemeHintText").set_text("{} selected: {}. Build or Preview to apply it.".format(theme, description))
        _status(widget, "{} mood selected. Build World or Preview to see the new atmosphere.".format(theme))
    return callback


def bind_widget(widget):
    global _callbacks, _bound_widget
    if _bound_widget and _bound_widget == widget:
        return
    _callbacks = []
    actions = {
        "GenerateButton": generate,
        "PreviewButton": preview,
        "NewSeedButton": new_seed,
        "CompactProfileButton": compact_profile,
        "BalancedProfileButton": balanced_profile,
        "LushProfileButton": lush_profile,
        "ImportNatureButton": import_nature_kit,
        "ImportTownButton": import_town_kit,
        "UpdateVegetationButton": update_vegetation,
        "StartOverButton": start_over,
        "SavePresetButton": save_preset,
        "LoadPresetButton": load_preset,
        "ResetButton": reset,
        "DeleteButton": delete_world,
    }
    for name, action in actions.items():
        callback = _make_click_callback(action, widget)
        _find(widget, name).on_clicked.add_callable(callback)
        _callbacks.append(callback)
    for index, name in enumerate(("WorldTabButton", "NatureTabButton", "MoodTabButton", "BuildTabButton")):
        callback = _make_tab_callback(widget, index)
        _find(widget, name).on_clicked.add_callable(callback)
        _callbacks.append(callback)
    for key in ("Terrain", "Trees", "FlowerRatio", "Rocks", "GrassRatio", "Lighting", "SkyTint", "GroundTint", "LightTint"):
        callback = _make_slider_callback(widget)
        _find(widget, key + "Slider").on_value_changed.add_callable(callback)
        _callbacks.append(callback)
    theme_callback = _make_theme_callback(widget)
    _find(widget, "ThemeBox").on_selection_changed.add_callable(theme_callback)
    _callbacks.append(theme_callback)
    _refresh_readouts(widget)
    _bound_widget = widget


def open_tool():
    asset = build_widget_asset()
    subsystem = unreal.get_editor_subsystem(unreal.EditorUtilitySubsystem)
    widget = subsystem.find_utility_widget_from_blueprint(asset)
    if not widget:
        widget = subsystem.spawn_and_register_tab(asset)
    _build_runtime_ui(widget)
    bind_widget(widget)
    _status(widget, "World Builder v3.1 ready — clean ground placement enabled.")
    return widget


def reload_tool():
    """Refresh the live panel after a Python edit, without restarting Unreal."""
    asset = build_widget_asset()
    subsystem = unreal.get_editor_subsystem(unreal.EditorUtilitySubsystem)
    widget = subsystem.find_utility_widget_from_blueprint(asset)
    if not widget:
        widget = subsystem.spawn_and_register_tab(asset)
    _build_runtime_ui(widget, force=True)
    bind_widget(widget)
    _status(widget, "World Builder v3 loaded — safe generation is ready.")
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
    """Convert a manually-run host EUW into the World Builder panel."""
    try:
        if not _widget_asset:
            return
        widget = unreal.get_editor_subsystem(unreal.EditorUtilitySubsystem).find_utility_widget_from_blueprint(_widget_asset)
        if widget and not widget.find_child_widget_by_name("IWBTitle"):
            _build_runtime_ui(widget)
            bind_widget(widget)
            _status(widget, "World Builder v3.1 ready — clean ground placement enabled.")
    except Exception as error:
        unreal.log_warning("World Builder panel watch skipped: {}".format(error))


def startup():
    try:
        global _watch_handle, _widget_asset
        register_menu()
        _widget_asset = build_widget_asset()
        if _watch_handle is None:
            _watch_handle = unreal.register_slate_post_tick_callback(_watch_for_manually_opened_panel)
        return open_tool()
    except Exception as error:
        unreal.log_error("Interactive World Builder startup failed: {}".format(error))
        return None
