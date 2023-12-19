// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class WidgetGenCodeTool : ModuleRules
{
	public WidgetGenCodeTool(ReadOnlyTargetRules Target) : base(Target)
	{
		OptimizeCode = CodeOptimization.Never;
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);


		PrivateIncludePaths.Add(System.IO.Path.Combine(GetModuleDirectory("WidgetGenCodeTool"), "Private"));


		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				// ... add other public dependencies that you statically link with here ...
			}
			);


		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Projects",
				"InputCore",
				"EditorFramework",
				"UnrealEd",
				"ToolMenus",
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"UMG",
				"UMGEditor",
				"Kismet",
				"MainFrame",
				"EngineSettings",
				"GameProjectGeneration",
				"Documentation",
				"AppFramework",
				"ToolWidgets",
				"ContentBrowser",
				"DesktopPlatform",
				"SourceControl",
				// ... add private dependencies that you statically link with here ...	
			}
			);

		if (Target.bWithLiveCoding)
		{
			PrivateIncludePathModuleNames.Add("LiveCoding");
		}
	}
}
