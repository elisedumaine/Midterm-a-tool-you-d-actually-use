"""Low-Poly Studio panel: design trees and rocks with arrows, see them live, grow them in the forest.

Opens from Tools > Open Low-Poly Studio.  It is built at runtime inside an
Editor Utility Widget, with the World Builder's own UI pieces (same soft dusk
style).  Houdini makes every preview and every baked asset (lowpoly_studio).
"""
import builtins
import json
import os
import random

import unreal

import interactive_world_builder as w
import lowpoly_studio as core
import lowpoly_character as character  # adds the Character tab to core.KINDS

ICON_FOLDER = "/Game/LowPolyStudio/UI"
STUDIO_ASSET = "/Game/LowPolyStudio/EUW_LowPolyStudio"
STUDIO_WIDTH = 470.0
MENU_OWNER = "LowPolyStudioMenu"
# Controls of the Landmass erosion utility this panel is duplicated from.
HIDDEN_HOST_CONTROLS = (
    "HorizontalBox_PresetButtons", "HorizontalBox_MoveDelete", "HorizontalBox_ErosionKeys",
    "Button_AddErosionKey", "Button_Simulate", "Button_Reset", "Text_Iterations", "TextBlock_13",
    "Button_LoadSettings", "Button_SaveToDataAsset", "Button_NewDataAsset",
    "Button_DataAssetCategory", "Button_Delete", "Button_SwapBack", "Button_SwapFwd",
    "HorizontalBox_34", "Slider_ErosionTimeline",
)


# ====================================================================== icons
def ensure_icons():
    """Studio icons, drawn with the World Builder's icon tool (Tools/make_icons.py)."""
    if unreal.EditorAssetLibrary.does_asset_exist(ICON_FOLDER + "/ICO_studio"):
        return
    import importlib.util
    script = os.path.join(unreal.Paths.project_dir(), "Tools", "make_icons.py")
    spec = importlib.util.spec_from_file_location("iwb_make_icons", script)
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    icons = {
        "arrow_left": ([m.polygon([(0.66, 0.18), (0.28, 0.5), (0.66, 0.82)])], []),
        "arrow_right": ([m.polygon([(0.34, 0.18), (0.72, 0.5), (0.34, 0.82)])], []),
        "dice": ([m.rect(0.14, 0.14, 0.86, 0.86, 0.16)],
                 [m.circle(0.32, 0.32, 0.075), m.circle(0.5, 0.5, 0.075), m.circle(0.68, 0.68, 0.075)]),
        "add": ([m.rect(0.42, 0.16, 0.58, 0.84, 0.05), m.rect(0.16, 0.42, 0.84, 0.58, 0.05)], []),
        "rock": ([m.polygon([(0.12, 0.82), (0.22, 0.48), (0.44, 0.3), (0.7, 0.36), (0.88, 0.6), (0.9, 0.82)])],
                 [m.stroke([(0.44, 0.3), (0.52, 0.56), (0.88, 0.6)], 0.04)]),
        "close": ([m.stroke([(0.26, 0.26), (0.74, 0.74)], 0.12), m.stroke([(0.74, 0.26), (0.26, 0.74)], 0.12)], []),
        "studio": ([m.circle(0.5, 0.36, 0.22), m.rect(0.46, 0.5, 0.54, 0.84, 0.02),
                    m.polygon([(0.18, 0.88), (0.5, 0.7), (0.82, 0.88)])], []),
    }
    folder = os.path.join(unreal.Paths.project_intermediate_dir(), "LowPolyStudioIcons")
    os.makedirs(folder, exist_ok=True)
    tasks = []
    for name, (adds, subtracts) in icons.items():
        path = os.path.join(folder, "ICO_%s.png" % name)
        m.write_png(path, m.render(adds, subtracts))
        task = unreal.AssetImportTask()
        for key, value in (("filename", path), ("destination_path", ICON_FOLDER), ("automated", True),
                           ("replace_existing", True), ("save", True)):
            task.set_editor_property(key, value)
        tasks.append(task)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    for name in icons:
        texture = unreal.load_asset("%s/ICO_%s" % (ICON_FOLDER, name))
        if texture:
            texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
            texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
            texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
            unreal.EditorAssetLibrary.save_loaded_asset(texture, False)


def _use_icon(image, icon):
    """Show one of the Studio's own icons (the World Builder ones are found by w._icon)."""
    texture = unreal.load_asset("%s/ICO_%s" % (ICON_FOLDER, icon))
    if texture and image:
        image.set_brush_from_texture(texture, False)


def _icon(owner, name, icon, size=16.0, color="text"):
    image = w._icon(owner, name, icon, size, color)
    _use_icon(image, icon)
    return image


def _button(owner, name, text, tone, size, icon):
    button = w._button(owner, name, text, tone, size, icon)
    _use_icon(w._panel(owner)["names"].get(name + "Icon"), icon)
    return button


def _icon_button(owner, name, icon, tip, size=14):
    button = w._new(unreal.Button, owner, name)
    w._style_button(button, "raised", "hover", "pressed", 6.0, padding=(5, 4))
    w._center(button.add_child(_icon(owner, name + "Icon", icon, size, "text")))
    button.set_tool_tip_text(tip)
    return button


def _short(error):
    lines = str(error).strip().splitlines()
    return lines[-1][:160] if lines else "unknown error"


# ====================================================================== panel
class Studio:
    """One open Studio panel: its widgets, the designs being edited, and what runs each frame."""

    def __init__(self, widget):
        self.widget = widget
        self.kind = "tree"
        self.designs = {kind: core.default_design(kind) for kind in core.KIND_ORDER}
        self.dirty, self.quiet = True, 0.0
        self.pending, self.pending_shown = None, False
        self.photo_job, self.photo_clock = None, 0.0
        self.retry = 0.0                 # seconds before trying Houdini again
        self.state = w._panel(widget)

    def find(self, name):
        return self.state["names"][name]

    def keep(self, callback):
        return w._keep(self.state, callback)

    def status(self, message):
        try:
            self.find("LPSStatusText").set_text(message)
        except Exception:
            pass
        unreal.log("Low-Poly Studio: " + message)

    @property
    def design(self):
        return self.designs[self.kind]

    # ------------------------------------------------------------------ build
    def build(self):
        widget = self.widget
        ensure_icons()
        w._ensure_icons()
        w._ui_generation += 1
        self.state.update({"names": {}, "callbacks": [], "icons": []})
        host = widget.find_child_widget_by_name(w.HOST_PANEL)
        host.clear_children()
        for name in HIDDEN_HOST_CONTROLS:
            old = widget.find_child_widget_by_name(name)
            if old:
                old.set_visibility(unreal.SlateVisibility.COLLAPSED)

        root = w._new(unreal.Border, widget, "LPSRoot")
        root.set_brush(w._rounded_brush(w._color("bg"), 0.0))
        root.set_padding(w._margin(0, 0, 0, 6))
        root.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_LEFT)
        # One column, the object on top and everything below it.  Arrows and
        # sliders sit two per line, so the whole Studio fits without scrolling.
        panel = w._new(unreal.VerticalBox, widget, "LPSPanel")
        root.add_child(w._size_box(widget, "LPSColumn", panel, width=STUDIO_WIDTH))
        w._add(host, root, w._margin(0, 0, 0, 0))

        # Title and category tabs share one line.
        header = w._new(unreal.HorizontalBox, widget, "LPSHeader")
        logo = header.add_child(_icon(widget, "LPSLogo", "studio", 16, "accent"))
        logo.set_padding(w._margin(0, 0, 5, 0))
        logo.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
        title = header.add_child(w._text(widget, "LPSTitle", "Low-Poly Studio", True, w._color("text"), 11))
        title.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
        title.set_padding(w._margin(0, 0, 10, 0))
        for index, kind in enumerate(core.KIND_ORDER):
            spec = core.KINDS[kind]
            button = _button(widget, "LPSTab_" + kind, spec["label"], "soft", 8, spec["icon"])
            button.on_clicked.add_callable(self.keep(self._tab_callback(kind)))
            w._add(header, button, w._margin(0 if index == 0 else 3, 0, 0, 0), fill=True)
        w._add(panel, w._size_box(widget, "LPSHeaderBox", header, height=26.0), w._margin(8, 6, 8, 4))

        # The preview: what the booth camera films, turning slowly.
        stage = w._new(unreal.Border, widget, "LPSStage")
        stage.set_brush(w._rounded_brush(w._color("card"), 8.0, w._color("line"), 1.0))
        stage.set_padding(w._margin(3, 3, 3, 2))
        stage.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)
        stage_box = w._new(unreal.VerticalBox, widget, "LPSStageBox")
        picture = w._new(unreal.Image, widget, "LPSPreviewImage")
        picture.set_color_and_opacity(unreal.LinearColor(1, 1, 1, 1))
        stage_box.add_child(w._size_box(widget, "LPSPreviewBox", picture, core.PREVIEW_SIZE[0], core.PREVIEW_SIZE[1]))
        caption = stage_box.add_child(w._text(widget, "LPSCaption", "", True, w._color("text"), 8))
        caption.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)
        caption.set_padding(w._margin(0, 2, 0, 0))
        stage.add_child(stage_box)
        w._add(panel, stage, w._margin(8, 0, 8, 3))

        switcher = w._new(unreal.WidgetSwitcher, widget, "LPSPages")
        for kind in core.KIND_ORDER:
            page = w._new(unreal.VerticalBox, widget, "LPSPage_" + kind)
            switcher.add_child(page)
            self._build_design_page(page, kind)
        w._add(panel, switcher)

        # Surprise, new variant and Add to forest on one line.
        actions = w._new(unreal.HorizontalBox, widget, "LPSActions")
        for name, label, icon, tip, method, tone in (
            ("LPSSurprise", "Surprise", "dice", "A random design", self.surprise, "soft"),
            ("LPSVariant", "Variant", "refresh", "Same design, another seed", self.new_variant, "soft"),
            ("LPSAdd", "Add to forest", "add", "Houdini bakes %d variants of this design; your forest grows them" % core.VARIANTS,
             self.add_to_forest, "primary"),
        ):
            button = _button(widget, name, label, tone, 9, icon)
            button.set_tool_tip_text(tip)
            button.on_clicked.add_callable(self.keep(self._call(method)))
            slot = w._add(actions, button, w._margin(0 if name == "LPSSurprise" else 3, 0, 0, 0), fill=True)
            if tone == "primary":
                size = unreal.SlateChildSize()
                size.set_editor_property("size_rule", unreal.SlateSizeRule.FILL)
                size.set_editor_property("value", 1.6)
                slot.set_size(size)
        w._add(panel, w._size_box(widget, "LPSActionsBox", actions, height=26.0), w._margin(8, 3, 8, 3))

        # The population, with "Grow forest now" on its title line.
        forest = w._new(unreal.Border, widget, "LPSForestCard")
        forest.set_brush(w._rounded_brush(w._color("card"), 8.0, w._color("line"), 1.0))
        forest.set_padding(w._margin(8, 4, 8, 5))
        forest.set_tool_tip_text("What the World Builder grows. Click a name to edit it; x1/x2/x3 is how often it grows.")
        forest_box = w._new(unreal.VerticalBox, widget, "LPSForestBox")
        heading = w._new(unreal.HorizontalBox, widget, "LPSForestHeading")
        icon = heading.add_child(_icon(widget, "LPSForestIcon", "forest", 12, "accent"))
        icon.set_padding(w._margin(0, 0, 5, 0))
        icon.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
        label = w._text(widget, "LPSForestTitle", "Your forest", True, w._color("text"), 9)
        w._add(heading, label, w._margin(0, 0, 0, 0), fill=True).set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
        grow = _button(widget, "LPSGrow", "Grow forest now", "soft", 8, "vegetation")
        grow.set_tool_tip_text("Same ground, new plants with your designs (World Builder: New plants)")
        grow.on_clicked.add_callable(self.keep(self._call(self.grow_now)))
        heading.add_child(w._size_box(widget, "LPSGrowBox", grow, height=20.0))
        forest_box.add_child(heading)
        w._add(forest_box, w._new(unreal.VerticalBox, widget, "LPSPopulation"), w._margin(0, 3, 0, 0))
        forest.add_child(forest_box)
        w._add(panel, forest, w._margin(8, 0, 8, 3))

        status = w._new(unreal.Border, widget, "LPSStatus")
        status.set_brush(w._rounded_brush(w._color("card"), 6.0, w._color("line"), 1.0))
        status.set_padding(w._margin(8, 3, 8, 3))
        status.add_child(w._text(widget, "LPSStatusText", "Ready.", False, w._color("text"), 8, wrap=True))
        w._add(panel, status, w._margin(8, 0, 8, 0))

        self.show_kind(self.kind)
        self.refresh_population()
        w._resize_icons(widget)
        widget.invalidate_layout_and_volatility()
        widget.force_layout_prepass()

    def _grid(self, card, name):
        """Two columns inside a card; returns (left, right)."""
        widget = self.widget
        row = w._new(unreal.HorizontalBox, widget, name)
        left = w._new(unreal.VerticalBox, widget, name + "Left")
        right = w._new(unreal.VerticalBox, widget, name + "Right")
        w._add(row, left, w._margin(0, 0, 5, 0), fill=True)
        w._add(row, right, w._margin(5, 0, 0, 0), fill=True)
        w._add(card, row)
        return left, right

    def _card(self, page, name, title, tip, icon):
        """A compact card: a small title line, then its content."""
        widget = self.widget
        card = w._new(unreal.Border, widget, name + "Card")
        card.set_brush(w._rounded_brush(w._color("card"), 8.0, w._color("line"), 1.0))
        card.set_padding(w._margin(8, 3, 8, 4))
        card.set_tool_tip_text(tip)
        box = w._new(unreal.VerticalBox, widget, name + "Box")
        heading = w._new(unreal.HorizontalBox, widget, name + "Heading")
        mark = heading.add_child(_icon(widget, name + "Icon", icon, 11, "accent"))
        mark.set_padding(w._margin(0, 0, 5, 0))
        mark.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
        heading.add_child(w._text(widget, name + "Title", title, True, w._color("text"), 9))
        w._add(box, heading, w._margin(0, 0, 0, 2))
        card.add_child(box)
        w._add(page, card, w._margin(8, 0, 8, 3))
        return box

    def _build_design_page(self, page, kind):
        widget, spec = self.widget, core.KINDS[kind]
        if kind == "import":
            self._build_source(page)
        look = self._card(page, "LPSLook_" + kind, "Look", "The arrows change the look; the preview follows.", "palette")
        columns = self._grid(look, "LPSLookGrid_" + kind)
        for index, (key, label, options) in enumerate(spec["arrows"]):
            self._arrow_row(columns[index % 2], kind, key, label, options)
        if not spec["sliders"]:
            return
        tune = self._card(page, "LPSTune_" + kind, "Fine tuning", "Let go of a slider to see the change.", "scale")
        columns = self._grid(tune, "LPSTuneGrid_" + kind)
        for index, (key, label, low, high, default, decimals) in enumerate(spec["sliders"]):
            self._slider(columns[index % 2], kind, key, label, low, high, default, decimals)

    def _slider(self, panel, kind, key, label, low, high, default, decimals):
        """A slider half a line wide: short label, rail, value."""
        widget, name = self.widget, "LPS_%s_%s" % (kind, key)
        row = w._new(unreal.HorizontalBox, widget, name + "Row")
        caption = w._text(widget, name + "Label", label, False, w._color("muted"), 8)
        caption.set_min_desired_width(54.0)
        row.add_child(caption).set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
        slider = w._new(unreal.Slider, widget, name + "Slider")
        slider.set_min_value(low)
        slider.set_max_value(high)
        slider.set_value(default)
        if not decimals:
            slider.set_step_size(1.0)
        try:
            slider.set_slider_bar_color(w._color("faint"))
            slider.set_slider_handle_color(w._color("accent"))
        except Exception:
            pass
        w._add(row, slider, w._margin(0, 0, 4, 0), fill=True)
        readout = w._text(widget, name + "Readout", "%.2f" % default if decimals else str(default), True, w._color("text"), 8)
        readout.set_min_desired_width(26.0)
        row.add_child(readout).set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
        w._add(panel, w._size_box(widget, name + "Box", row, height=19.0))
        slider.on_value_changed.add_callable(self.keep(self._slider_callback(kind, key, decimals)))
        slider.on_mouse_capture_end.add_callable(self.keep(self._call(self.changed)))

    def _build_source(self, page):
        """Where the imported object comes from: a 3D file, or a photo the AI turns into 3D."""
        widget = self.widget
        source = w._section(widget, page, "LPSSource", "Source",
                            "Any 3D model (OBJ, FBX, GLB...), or a photo: the AI guesses its 3D shape.", "load")
        row = w._new(unreal.HorizontalBox, widget, "LPSSourceRow")
        chip = w._new(unreal.Border, widget, "LPSSourceChip")
        chip.set_brush(w._rounded_brush(w._color("raised"), 6.0))
        chip.set_padding(w._margin(6, 3, 6, 3))
        chip.add_child(w._text(widget, "LPSSourceName", "No model yet", True, w._color("text"), 9))
        w._add(row, chip, w._margin(0, 0, 3, 0), fill=True)
        for index, (name, label, icon, tip, method) in enumerate((
            ("LPSChooseModel", "Model", "load", "Choose a 3D model file", self.choose_model),
            ("LPSFromPhoto", "Photo", "preview", "Choose a photo: the AI makes a 3D model of it (about a minute)", self.from_photo),
        )):
            button = _button(widget, name, label, "soft", 9, icon)
            button.set_tool_tip_text(tip)
            button.on_clicked.add_callable(self.keep(self._call(method)))
            w._add(row, button, w._margin(0 if index == 0 else 3, 0, 0, 0))
        w._add(source, row, w._margin(0, 1, 0, 1))

    def _arrow_row(self, panel, kind, key, label, options):
        """Label, a back arrow, the current choice in a chip, a next arrow (and swatches for the palette)."""
        widget, tag = self.widget, "%s_%s" % (kind, key)
        row = w._new(unreal.HorizontalBox, widget, "LPSArrowRow_" + tag)
        caption = w._text(widget, "LPSArrowLabel_" + tag, label, False, w._color("muted"), 8)
        caption.set_min_desired_width(50.0)
        row.add_child(caption).set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
        for name, icon, step in (("LPSBack_", "arrow_left", -1), ("LPSNext_", "arrow_right", 1)):
            arrow = _icon_button(widget, name + tag, icon, ("Previous " if step < 0 else "Next ") + label.lower(), 9)
            arrow.on_clicked.add_callable(self.keep(self._arrow_callback(kind, key, step)))
            if step > 0:
                chip = w._new(unreal.Border, widget, "LPSValueChip_" + tag)
                chip.set_brush(w._rounded_brush(w._color("raised"), 6.0))
                chip.set_padding(w._margin(4, 1, 4, 1))
                chip.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)
                chip.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
                chip.add_child(w._text(widget, "LPSValue_" + tag, options[0][0], True, w._color("text"), 9))
                w._add(row, chip, w._margin(3, 0, 3, 0), fill=True)
            w._add(row, w._size_box(widget, name + "Box_" + tag, arrow, 24.0, 20.0))
        if key == "palette":
            swatches = w._new(unreal.HorizontalBox, widget, "LPSSwatches_" + kind)
            for index in range(4):
                chip = w._new(unreal.Border, widget, "LPSSwatch_%s_%d" % (kind, index))
                chip.set_brush(w._rounded_brush(w._color("raised"), 2.0))
                w._add(swatches, w._size_box(widget, "LPSSwatchBox_%s_%d" % (kind, index), chip, 5.0, 14.0), w._margin(1, 0, 0, 0))
            slot = row.add_child(swatches)
            slot.set_padding(w._margin(4, 0, 0, 0))
            slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
        w._add(panel, w._size_box(widget, "LPSArrowBox_" + tag, row, height=22.0), w._margin(0, 1, 0, 1))

    # ------------------------------------------------- callbacks (one closure each)
    def _call(self, method):
        def callback():
            method()
        return callback

    def _tab_callback(self, kind):
        def callback():
            self.show_kind(kind)
        return callback

    def _arrow_callback(self, kind, key, step):
        def callback():
            options = {k: o for k, _l, o in core.KINDS[kind]["arrows"]}[key]
            arrows = self.designs[kind]["arrows"]
            arrows[key] = (arrows.get(key, 0) + step) % len(options)
            sliders = self.designs[kind]["sliders"]
            for name, value in options[arrows[key]][1].items():
                if name in sliders:
                    sliders[name] = value
            self.refresh_controls()
            self.changed()
        return callback

    def _slider_callback(self, kind, key, decimals):
        def callback(value):
            self.designs[kind]["sliders"][key] = value if decimals else round(value)
            self.find("LPS_%s_%sReadout" % (kind, key)).set_text("%.2f" % value if decimals else str(round(value)))
        return callback

    def _entry_callback(self, method, entry_id):
        def callback():
            method(entry_id)
        return callback

    # ---------------------------------------------------------------- actions
    def show_kind(self, kind):
        self.kind = kind
        self.find("LPSPages").set_active_widget_index(core.KIND_ORDER.index(kind))
        for other in core.KIND_ORDER:
            selected = other == kind
            w._style_button(self.find("LPSTab_" + other), "accent" if selected else "raised",
                            "accent_hover" if selected else "hover", "pressed")
            for part in ("Label", "Icon"):
                item = self.state["names"].get("LPSTab_%s%s" % (other, part))
                if item:
                    w._tint(item, w._color("on_accent" if selected else "text"))
        # The character is not grown in the forest: it is played.
        self.find("LPSAddLabel").set_text("Play as this character" if kind == "character" else "Add to forest")
        self.find("LPSForestCard").set_visibility(
            unreal.SlateVisibility.COLLAPSED if kind == "character" else unreal.SlateVisibility.VISIBLE)
        self.refresh_controls()
        self.changed()

    def refresh_controls(self):
        kind, design = self.kind, self.design
        if kind == "import":
            name = os.path.basename(design.get("file", "")) or "No model yet"
            self.find("LPSSourceName").set_text(name if len(name) <= 30 else name[:27] + "...")
        for key, _label, options in core.KINDS[kind]["arrows"]:
            self.find("LPSValue_%s_%s" % (kind, key)).set_text(options[design["arrows"].get(key, 0) % len(options)][0])
        if "palette" in design["arrows"]:
            palette = core.KIT_PALETTES[design["arrows"]["palette"]]
            for index, color in enumerate(w._palette_preview(w.NATURAL_PALETTE if palette == "Natural" else palette)):
                self.find("LPSSwatch_%s_%d" % (kind, index)).set_brush(w._rounded_brush(color, 2.0))
        for key, _label, _low, _high, default, decimals in core.KINDS[kind]["sliders"]:
            value = design["sliders"].setdefault(key, default)
            self.find("LPS_%s_%sSlider" % (kind, key)).set_value(value)
            self.find("LPS_%s_%sReadout" % (kind, key)).set_text("%.2f" % value if decimals else str(round(value)))

    def changed(self):
        self.dirty, self.quiet = True, 0.0

    def surprise(self):
        rng, design = random.Random(), self.design
        for key, _label, options in core.KINDS[self.kind]["arrows"]:
            design["arrows"][key] = rng.randrange(len(options))
        for key, _label, low, high, _default, decimals in core.KINDS[self.kind]["sliders"]:
            design["sliders"][key] = round(rng.uniform(low, low + (high - low) * 0.8), 2) if decimals else rng.randint(int(low), int(high))
        design["sliders"]["detail"] = rng.randint(1, 3)
        design["seed"] = rng.randrange(1000)
        self.refresh_controls()
        self.changed()

    def new_variant(self):
        if self.kind == "character":
            self.status("A character has no variants: change it with the arrows, or try Surprise.")
            return
        self.design["seed"] = (self.design["seed"] + 37) % 1000
        self.changed()

    def choose_model(self):
        path = core.pick_file("Choose a 3D model", core.MODEL_FILTER)
        if path:
            self.designs["import"]["file"] = path
            self.show_kind("import")
            self.status("Low-Polify is reshaping %s..." % os.path.basename(path))

    def from_photo(self):
        if not core.photo_ready():
            self.status("The photo AI is not installed on this computer (see the README: Photo to 3D).")
            return
        if self.photo_job:
            self.status("The AI is still working on the last photo.")
            return
        image = core.pick_file("Choose a photo of one object", core.PHOTO_FILTER)
        if not image:
            return
        self.photo_job = core.start_photo_job(image) + (os.path.basename(image),)
        self.photo_clock = 0.0
        self.show_kind("import")
        self.status("The AI is imagining %s in 3D (about a minute; the first time it also downloads ~2 GB)..."
                    % os.path.basename(image))

    def _poll_photo(self, delta):
        process, model, log, name = self.photo_job
        self.photo_clock += delta
        if process.poll() is None:
            if int(self.photo_clock) % 10 == 0 and int(self.photo_clock - delta) % 10 != 0:
                self.status("The AI is imagining %s in 3D... %d s" % (name, self.photo_clock))
            return
        self.photo_job = None
        if process.returncode == 0 and os.path.isfile(model):
            self.designs["import"]["file"] = model
            self.show_kind("import")
            self.status("%s is now 3D. Low-Polify is reshaping it..." % name)
        else:
            try:
                last = open(log, encoding="utf-8", errors="replace").read().strip().splitlines()[-1]
            except (OSError, IndexError):
                last = "no message"
            self.status("The AI could not make %s: %s" % (name, last[:160]))

    def add_to_forest(self):
        kind, design = self.kind, json.loads(json.dumps(self.design))
        if kind == "character":
            def play():
                mesh = character.play_as(design)
                self.status("You are now %s (%s). Press Play to walk around."
                            % (core.design_title(kind, design), mesh.get_name()))
            self.pending = ("Houdini is making your character and fitting it on the player's skeleton...", play)
            return

        def work():
            entry = core.add_to_forest(kind, design)
            self.refresh_population()
            self.status("%s joined your forest (%d variants). Click Grow forest now, or Build World, to see it grow."
                        % (entry["title"], core.VARIANTS))
        self.pending = ("Houdini is making %d %s variants..." % (core.VARIANTS, core.KINDS[kind]["noun"]), work)

    def grow_now(self):
        panels = w._panel_instances()
        if not panels:
            self.status("Open the World Builder (Tools menu) first, then click Grow forest now again.")
            return

        def work():
            w.refresh_plants(panels[0])
            self.status("Your forest grew again, with your designs.")
        self.pending = ("Growing the forest with your designs...", work)

    def remove_entry(self, entry_id):
        population = core.load_population()
        population["entries"] = [entry for entry in population["entries"] if entry["id"] != entry_id]
        core.save_population(population)
        self.refresh_population()
        self.status("Removed. Where none of your designs are left, the World Builder grows its own.")

    def cycle_share(self, entry_id):
        population = core.load_population()
        for entry in population["entries"]:
            if entry["id"] == entry_id:
                entry["share"] = entry.get("share", 1) % 3 + 1
        core.save_population(population)
        self.refresh_population()

    def edit_entry(self, entry_id):
        for entry in core.load_population()["entries"]:
            if entry["id"] == entry_id:
                self.designs[entry["kind"]] = json.loads(json.dumps(entry["design"]))
                self.show_kind(entry["kind"])
                self.status("Editing %s. Add to forest keeps it as a new design." % entry["title"])

    def refresh_population(self):
        widget = self.widget
        listing = self.find("LPSPopulation")
        listing.clear_children()
        entries = core.load_population()["entries"]
        if not entries:
            w._add(listing, w._text(widget, "LPSEmpty", "Nothing yet: the World Builder grows its own trees and rocks. "
                                    "Design one above, then click Add to forest.", False, w._color("muted"), 8, wrap=True))
            return
        for entry in entries:
            tag = entry["id"]
            row = w._new(unreal.HorizontalBox, widget, "LPSEntry_" + tag)
            icon = row.add_child(_icon(widget, "LPSEntryIcon_" + tag, core.KINDS[entry["kind"]]["icon"], 12, "accent"))
            icon.set_padding(w._margin(0, 0, 6, 0))
            icon.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
            name = w._new(unreal.Button, widget, "LPSEntryName_" + tag)
            w._style_button(name, "card", "hover", "pressed", 4.0, padding=(3, 2))
            name.add_child(w._text(widget, "LPSEntryText_" + tag, entry["title"], False, w._color("text"), 8))
            name.set_tool_tip_text("Edit this design")
            name.on_clicked.add_callable(self.keep(self._entry_callback(self.edit_entry, tag)))
            w._add(row, name, w._margin(0, 0, 4, 0), fill=True)
            share = w._button(widget, "LPSEntryShare_" + tag, "x%d" % entry.get("share", 1), "soft", 8)
            share.set_tool_tip_text("How often it grows: click for x1, x2, x3")
            share.on_clicked.add_callable(self.keep(self._entry_callback(self.cycle_share, tag)))
            w._add(row, w._size_box(widget, "LPSEntryShareBox_" + tag, share, 34.0, 22.0), w._margin(0, 0, 3, 0))
            remove = _icon_button(widget, "LPSEntryRemove_" + tag, "close", "Remove from the forest", 9)
            remove.on_clicked.add_callable(self.keep(self._entry_callback(self.remove_entry, tag)))
            w._add(row, w._size_box(widget, "LPSEntryRemoveBox_" + tag, remove, 24.0, 22.0))
            w._add(listing, row, w._margin(0, 1, 0, 1))
        w._resize_icons(widget)

    # ------------------------------------------------------------ every frame
    def tick(self, delta):
        if self.pending:
            # Show the message first (one frame), then do the slow work.
            message, action = self.pending
            if not self.pending_shown:
                self.status(message)
                self.pending_shown = True
                return
            self.pending, self.pending_shown = None, False
            try:
                action()
            except Exception as error:
                self.status("Could not finish: " + _short(error))
            return
        if self.photo_job:
            self._poll_photo(delta)
        if self.retry > 0.0:
            self.retry -= delta
            if self.retry <= 0.0:
                self.changed()
        self.quiet += delta
        if self.dirty and self.quiet > 0.12:
            self.dirty = False
            self.update_preview()

    def update_preview(self):
        kind, design = self.kind, self.design
        if kind == "import" and not design.get("file"):
            self.find("LPSCaption").set_text("No model yet")
            self.status("Choose a 3D model, or a photo of one object.")
            return
        try:
            if kind == "character":
                faces = character.preview_faces(character.cook(character.design_parms(design)))
            else:
                faces = core.cook(kind, core.design_parms(kind, design))
            count = core.show_preview(faces)
        except Exception as error:
            # Most often Houdini is simply closed: say so where the picture should be, and retry.
            closed = "not answering" in str(error) or "Open Houdini" in str(error)
            self.find("LPSCaption").set_text("Open Houdini to see the preview" if closed else "Houdini could not make this design")
            self.status(("Houdini is closed: open it and the preview appears by itself." if closed
                         else "Houdini could not make this design: %s" % _short(error)))
            if closed:
                self.retry = 3.0
            return
        picture = self.find("LPSPreviewImage")
        picture.set_brush_resource_object(core.ensure_booth()["target"])
        picture.set_desired_size_override(unreal.Vector2D(*core.PREVIEW_SIZE))
        variant = "" if kind == "character" else "  ·  variant %d" % design["seed"]
        self.find("LPSCaption").set_text("%s%s  ·  %d faces" % (core.design_title(kind, design), variant, count))
        self.status("Like it? Play as this character, then press Play." if kind == "character"
                    else "Like it? Add to forest. Or keep exploring with the arrows.")


# ============================================================ open the panel
_studios = {}
_tick_handle = None
_watch_clock = 0.0


def _open_panels():
    """Every Studio tab Unreal has open (it reopens them by itself at startup)."""
    found = []
    for item in unreal.ObjectIterator(unreal.EditorUtilityWidget):
        name = item.get_name()
        if name.startswith("EUW_LowPolyStudio") and not name.startswith("Default__") and item.find_child_widget_by_name(w.HOST_PANEL):
            found.append(item)
    return found


def _tick(delta):
    global _watch_clock
    core.spin_preview(delta)            # one turntable for every open Studio
    _watch_clock += delta
    if _watch_clock > 0.5:
        # A Studio tab opened without our menu (restored by Unreal) is still empty: build it.
        _watch_clock = 0.0
        try:
            for widget in _open_panels():
                if widget.get_path_name() not in _studios:
                    studio = Studio(widget)
                    _studios[widget.get_path_name()] = studio
                    studio.build()
                    studio.status("Ready. Use the arrows to design; Houdini builds the preview.")
        except Exception as error:
            unreal.log_warning("Low-Poly Studio panel watch: %s" % error)
    for path, studio in list(_studios.items()):
        try:
            studio.tick(delta)
        except Exception as error:
            if not unreal.SystemLibrary.is_valid(studio.widget):
                _studios.pop(path, None)      # its tab was closed
            else:
                unreal.log_warning("Low-Poly Studio: %s" % error)


def _ensure_tick():
    global _tick_handle
    if _tick_handle is None:
        _tick_handle = unreal.register_slate_post_tick_callback(_tick)
        builtins._lps_tick_handle = _tick_handle


# A reloaded module stops the previous one's frame callback (else both would run).
if getattr(builtins, "_lps_tick_handle", None) is not None:
    try:
        unreal.unregister_slate_post_tick_callback(builtins._lps_tick_handle)
    except Exception:
        pass
    builtins._lps_tick_handle = None


def studio_widget_asset():
    if not unreal.EditorAssetLibrary.does_asset_exist(STUDIO_ASSET):
        if not unreal.EditorAssetLibrary.duplicate_asset(w.SOURCE_EUW_PATH, STUDIO_ASSET):
            raise RuntimeError("Could not create the Low-Poly Studio panel")
    asset = unreal.load_asset(STUDIO_ASSET)
    w._clean_host_graph(asset)
    return asset


def open_studio():
    asset = studio_widget_asset()
    subsystem = unreal.get_editor_subsystem(unreal.EditorUtilitySubsystem)
    # spawn_and_register_tab also brings back a tab that is closed or hidden behind others.
    widget = subsystem.spawn_and_register_tab(asset)
    studio = Studio(widget)
    # Only one Studio at a time: two of them ticking doubled every preview and capture.
    _studios.clear()
    _studios[widget.get_path_name()] = studio
    studio.build()
    _ensure_tick()
    studio.status("Ready. Use the arrows to design; Houdini builds the preview.")
    return studio


@unreal.uclass()
class LowPolyStudioMenuEntry(unreal.ToolMenuEntryScript):
    @unreal.ufunction(override=True)
    def get_label(self, context):
        return "Open Low-Poly Studio"

    @unreal.ufunction(override=True)
    def get_tool_tip(self, context):
        return "Design low-poly trees and rocks with Houdini, then grow them in your world."

    @unreal.ufunction(override=True)
    def execute(self, context):
        open_studio()


def register_menu():
    menus = unreal.ToolMenus.get()
    menus.unregister_owner_by_name(MENU_OWNER)
    menu = menus.extend_menu("LevelEditor.MainMenu.Tools")
    entry = LowPolyStudioMenuEntry()
    entry.init_entry(MENU_OWNER, "LevelEditor.MainMenu.Tools", "WorldBuilder", "LowPolyStudio.Open", "Open Low-Poly Studio")
    menu.add_menu_entry_object(entry)
    menus.refresh_all_widgets()
    _ensure_tick()
