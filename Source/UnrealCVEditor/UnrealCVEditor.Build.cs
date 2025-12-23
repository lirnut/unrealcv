using UnrealBuildTool;
using System.Collections.Generic;

public class UnrealCVEditor : ModuleRules
{
	public UnrealCVEditor(ReadOnlyTargetRules Target) : base(Target)
	{
			PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

			PublicIncludePaths.AddRange(new string[] {
			});

			PrivateIncludePaths.AddRange(new string[] {
				"UnrealCVEditor/Private",
			});

			PublicDependencyModuleNames.AddRange(new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"UnrealCV",
				"AssetRegistry"
			});

			PrivateDependencyModuleNames.AddRange(new string[] {
				"UnrealEd",
				"ToolMenus",
				"Blutility"
			});
		}
	}
