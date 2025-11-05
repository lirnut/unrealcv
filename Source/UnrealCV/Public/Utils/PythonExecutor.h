#pragma once

#include "CoreMinimal.h"

class UNREALCV_API FPythonExecutor
{
public:
	struct FExecutionParams
	{
		FString ScriptPath;
		TArray<FString> Args;
		FString CondaEnvName;
		FString WorkingDirectory;
		bool bLaunchDetached;
		bool bLaunchHidden;

		FExecutionParams()
			: CondaEnvName(TEXT("uezoo"))
			, bLaunchDetached(true)
			, bLaunchHidden(true)
		{}
	};

	static bool ExecutePythonScript(const FExecutionParams& Params, int32* OutProcessID = nullptr);

	static bool ExecuteGenvidScript(
		const FString& ScriptPath,
		const FString& InputDir,
		int32 FPS,
		const FString& CondaEnvName = TEXT("uezoo"),
		int32* OutProcessID = nullptr
	);

private:
	static FString BuildCondaActivationCommand(const FString& EnvName, const FString& PythonCommand);
	static bool ValidateScriptPath(const FString& ScriptPath);
};
