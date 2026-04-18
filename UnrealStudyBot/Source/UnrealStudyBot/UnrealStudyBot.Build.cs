using UnrealBuildTool;

public class UnrealStudyBot : ModuleRules
{
    public UnrealStudyBot(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicIncludePaths.AddRange(new string[]
        {
            System.IO.Path.Combine(ModuleDirectory),
        });

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "EnhancedInput",
            "UMG",               // 위젯
            "HTTP",              // REST API 통신
            "Json",              // JSON 파싱
            "JsonUtilities",     // JsonObjectConverter
            "Slate",
            "SlateCore",
            "WebSockets",        // 로비 WebSocket 연결
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "RenderCore",
        });
    }
}
