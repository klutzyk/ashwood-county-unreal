// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AshwoodCounty : ModuleRules
{
	public AshwoodCounty(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"AshwoodCounty",
			"AshwoodCounty/Variant_Platforming",
			"AshwoodCounty/Variant_Platforming/Animation",
			"AshwoodCounty/Variant_Combat",
			"AshwoodCounty/Variant_Combat/AI",
			"AshwoodCounty/Variant_Combat/Animation",
			"AshwoodCounty/Variant_Combat/Gameplay",
			"AshwoodCounty/Variant_Combat/Interfaces",
			"AshwoodCounty/Variant_Combat/UI",
			"AshwoodCounty/Variant_SideScrolling",
			"AshwoodCounty/Variant_SideScrolling/AI",
			"AshwoodCounty/Variant_SideScrolling/Gameplay",
			"AshwoodCounty/Variant_SideScrolling/Interfaces",
			"AshwoodCounty/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
