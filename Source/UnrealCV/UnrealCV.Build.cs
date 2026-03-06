// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
using System.IO;
using System.Collections.Generic;

// An engine version independent configuration class
public class UnrealcvBuildConfig
{
	public List<string> PrivateIncludePaths = new List<string>();
	public List<string> PublicIncludePaths = new List<string>();
	public List<string> PublicDependencyModuleNames = new List<string>();
	public List<string> EditorPrivateDependencyModuleNames = new List<string>();
	public List<string> DynamicallyLoadedModuleNames = new List<string>();

	public UnrealcvBuildConfig(string EnginePath)
	{
		PublicIncludePaths.AddRange(
			new string[]
			{
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
				"UnrealCV/Private",
				"UnrealCV/Private/Actor",
				"UnrealCV/Public/Actor",
				"UnrealCV/Public/BPFunctionLib",
				"UnrealCV/Public/Component",
				"UnrealCV/Public/Controller",
				"UnrealCV/Public/Sensor",
				"UnrealCV/Public/Sensor/CameraSensor",
				"UnrealCV/Public/Server",
				"UnrealCV/Public/Utils",
				"UnrealCV/Public/Encoder"
			}
		);

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"RenderCore",
			"Networking",
			"Sockets",
			"Slate",
			"SlateCore",
			"UMG", // For in-game UI widgets
			"ImageWrapper",
			"ImageCore", // For ERawImageFormat
			"ImageWriteQueue", // For async image writing (MRQ-style)
			"CinematicCamera",
			"Projects", // Support IPluginManager
			"RHI", // Support low-level RHI operation
			"Json",
			"JsonUtilities",
			"AVEncoder",
			"AudioCapture",
            "AudioCaptureCore",
            "AudioMixer",
			"NavigationSystem", // For NavAgentController
			"AIModule", // For AI navigation
			"AssetRegistry", // For MetaHuman asset discovery
			"HairStrandsCore", // For GroomComponent annotation support
			"Renderer", // For HairStrands interface functions
			"Niagara", // For GroomComponent physics control
			"NiagaraCore",
			"MovieRenderPipelineCore",
			"PakFile", // For runtime pak mounting
			"Landscape", // For manual Landscape LOD computation
			"Foliage" // For InstancedFoliageActor annotation support
		});

		EditorPrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"UnrealEd", // To support GetGameWorld
				"Landscape", // For Landscape annotation support
				// This is only available for Editor build
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
			}
		);
	}
}

namespace UnrealBuildTool.Rules
{
	public class UnrealCV: ModuleRules
	{
		// ReadOnlyTargetRules for version > 4.15
		public UnrealCV(ReadOnlyTargetRules Target) : base(Target)
		// 4.16 or better
		{
			//bEnforceIWYU = true;
	  		//bFasterWithoutUnity = true;
			PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

			// This trick is from https://answers.unrealengine.com/questions/258689/how-to-include-private-header-files-of-other-modul.html
			// string EnginePath = Path.GetFullPath(BuildConfigurationTarget.RelativeEnginePath);
			string EnginePath = Path.GetFullPath(Target.RelativeEnginePath);
			UnrealcvBuildConfig BuildConfig = new UnrealcvBuildConfig(EnginePath);

			PublicIncludePaths = BuildConfig.PublicIncludePaths;
			PrivateIncludePaths = BuildConfig.PrivateIncludePaths;
			PublicDependencyModuleNames = BuildConfig.PublicDependencyModuleNames;
			DynamicallyLoadedModuleNames = BuildConfig.DynamicallyLoadedModuleNames;

			// PrivateDependency only available in Private folder
			// Reference: https://answers.unrealengine.com/questions/23384/what-is-the-difference-between-publicdependencymod.html
			// if (UEBuildConfiguration.bBuildEditor == true)
			PrivateDependencyModuleNames.Add("Landscape"); // For Landscape annotation support

			if (Target.bBuildEditor == true)
			{
				PrivateDependencyModuleNames.AddRange(BuildConfig.EditorPrivateDependencyModuleNames);
			}

			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				PublicSystemLibraries.AddRange(new string[] {
					"mfuuid.lib",
					"mfplat.lib",
					"mf.lib",
					"mfreadwrite.lib",
					"wmcodecdspuuid.lib"
				});
			}
		}
	}
}
