#include "PythonExecutor.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"
#include "UnrealcvLog.h"

bool FPythonExecutor::ExecutePythonScript(const FExecutionParams& Params, int32* OutProcessID)
{
	if (!ValidateScriptPath(Params.ScriptPath))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("PythonExecutor: Script not found: %s"), *Params.ScriptPath);
		return false;
	}

	FString ArgsString;
	for (const FString& Arg : Params.Args)
	{
		ArgsString += TEXT(" ") + Arg;
	}

	FString PythonExecutable = TEXT("G:\\.conda\\envs\\uezoo\\python.exe");
	FString PythonCommand = FString::Printf(TEXT("%s \"%s\" %s"), *PythonExecutable, *Params.ScriptPath, *ArgsString);
	// FString FullCommand = FString::Printf(TEXT("conda activate %s && %s"), *Params.CondaEnvName, *PythonCommand);
	FString FullCommand = PythonCommand;

	UE_LOG(LogUnrealCV, Display, TEXT("PythonExecutor: Executing command: %s"), *FullCommand);

	FString WorkDir = Params.WorkingDirectory.IsEmpty()
		? FPaths::GetPath(Params.ScriptPath)
		: Params.WorkingDirectory;

#if PLATFORM_WINDOWS
	// FString ShellExecutable = TEXT("powershell.exe");
	FString ShellExecutable = TEXT("cmd.exe");
	FString ShellArgs = FString::Printf(TEXT("/c \"%s\""), *FullCommand);
#else
	FString ShellExecutable = TEXT("/bin/bash");
	FString ShellArgs = FString::Printf(TEXT("-c \"%s\""), *FullCommand);
#endif

	uint32 ProcessID = 0;
	FProcHandle ProcHandle = FPlatformProcess::CreateProc(
		*ShellExecutable,
		*ShellArgs,
		Params.bLaunchDetached,
		Params.bLaunchHidden,
		Params.bLaunchHidden,
		&ProcessID,
		0,
		*WorkDir,
		nullptr,
		nullptr
	);

	if (!ProcHandle.IsValid())
	{
		UE_LOG(LogUnrealCV, Error, TEXT("PythonExecutor: Failed to create process"));
		return false;
	}

	if (OutProcessID)
	{
		*OutProcessID = ProcessID;
	}

	UE_LOG(LogUnrealCV, Display, TEXT("PythonExecutor: Process started with PID: %d"), ProcessID);

	if (!Params.bLaunchDetached)
	{
		FPlatformProcess::WaitForProc(ProcHandle);
	}

	FPlatformProcess::CloseProc(ProcHandle);
	return true;
}

bool FPythonExecutor::ExecuteGenvidScript(
	const FString& ScriptPath,
	const FString& InputDir,
	int32 FPS,
	const FString& CondaEnvName,
	int32* OutProcessID)
{
	FExecutionParams Params;
	Params.ScriptPath = ScriptPath;
	Params.Args.Add(TEXT("--input-dir"));
	Params.Args.Add(FString::Printf(TEXT("\"%s\""), *InputDir));
	Params.Args.Add(TEXT("--fps"));
	Params.Args.Add(FString::FromInt(FPS));
	Params.Args.Add(TEXT("--time_delay"));
	Params.Args.Add(TEXT("120.0"));
	Params.CondaEnvName = CondaEnvName;
	Params.bLaunchDetached = true;
	Params.bLaunchHidden = true;

	return ExecutePythonScript(Params, OutProcessID);
}

bool FPythonExecutor::ValidateScriptPath(const FString& ScriptPath)
{
	return FPaths::FileExists(ScriptPath);
}
