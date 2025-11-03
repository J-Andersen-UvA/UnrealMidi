using UnrealBuildTool;

public class MidiMapperEditor : ModuleRules
{
    public MidiMapperEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Core", "CoreUObject", "Engine", "Slate", "SlateCore",
            "InputCore", "ToolMenus", "Projects",
        });

        // Includes by Toucan
        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "MidiMapper", "UnrealMidi"
        });

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "UnrealEd", "EditorSubsystem",
        });


        if (Target.bBuildEditor == true)
        {
            PrivateDependencyModuleNames.Add("EditorStyle");
        }
    }
}
