// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class VlcPlayer : ModuleRules
{
	public VlcPlayer(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"RHI",
				"Media", 
				"MediaUtils",
				"RenderCore",
				"ImageCore"
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"Settings",
				"LibVlc"
			}
			);

		if (Target.bBuildEditor)
		{
			PublicDependencyModuleNames.Add("UnrealEd");
		}
	}
}
