using UnrealBuildTool;
using System.Collections.Generic;

public class UnrealStudyBotEditorTarget : TargetRules
{
	public UnrealStudyBotEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("UnrealStudyBot");
	}
}
