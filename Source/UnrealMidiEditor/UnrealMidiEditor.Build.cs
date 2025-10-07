using UnrealBuildTool;

public class UnrealMidiEditor : ModuleRules
{
    public UnrealMidiEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PrivateDependencyModuleNames.AddRange(new[] {
            "Core", "CoreUObject", "Engine",
            "Slate", "SlateCore", "EditorSubsystem", "UnrealEd",
            "ToolMenus", "Projects",
            "UnrealMidi", // depend on runtime module
            "InputCore",
        });
    }
}
