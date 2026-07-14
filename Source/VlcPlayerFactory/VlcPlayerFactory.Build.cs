using UnrealBuildTool;

public class VlcPlayerFactory : ModuleRules
{
    public VlcPlayerFactory(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core", 
                "Media"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore", 
                "VlcPlayer"
            }
        );
        
        PrivateIncludePaths.Add("VlcPlayer/Private");
    }
}