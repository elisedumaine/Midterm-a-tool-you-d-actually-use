using UnrealBuildTool;

public class InteractiveWorldBuilderTarget : TargetRules
{
	public InteractiveWorldBuilderTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("ForestGame");
	}
}
