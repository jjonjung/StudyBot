using UnrealBuildTool;
using System.Collections.Generic;

public class UnrealStudyBotTarget : TargetRules
{
	public UnrealStudyBotTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("UnrealStudyBot");
	}
}
