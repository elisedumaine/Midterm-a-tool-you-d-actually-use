using UnrealBuildTool;

// Gameplay for the generated forest: mushrooms to pick up, deer to feed.
public class ForestGame : ModuleRules
{
	public ForestGame(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore" });
	}
}
