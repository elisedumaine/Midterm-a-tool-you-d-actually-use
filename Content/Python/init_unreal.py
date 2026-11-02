import unreal

# Unreal automatically runs this project Python startup script.  It creates and
# opens the dockable Editor Utility Widget each time the project opens.
# The game itself (Standalone, or a packaged build) has no editor: opening the
# panel there crashed the launch, so the tool only starts inside the editor.
def _in_editor():
    try:
        return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem) is not None
    except Exception:
        return False


if _in_editor():
    try:
        import interactive_world_builder
        interactive_world_builder.startup()
    except Exception as error:
        unreal.log_error("World Builder could not open its panel: {}".format(error))
