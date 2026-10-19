using UnrealBuildTool;

public class InteractiveWorldBuilderEditor : ModuleRules
{
    public InteractiveWorldBuilderEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "UMG",
            "Slate",
            "SlateCore"
        });

        PrivateDependencyModuleNames.AddRange(new[]
        {
            "UnrealEd",
            "Blutility",
            "LevelEditor",
            "ToolMenus",
            "Projects",
            "AssetTools",
            "PropertyEditor",
            "ApplicationCore",
            "RenderCore",
            "PCG"
        });
    }
}
