#include "Runtime/Core/Public/Misc/CommandLine.h"
#include "Runtime/Core/Public/Misc/Parse.h"
#include "Runtime/Engine/Classes/Engine/Engine.h"
#include "Runtime/Core/Public/Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

#include "UnrealcvServer.h"
#include "UnrealcvLog.h"
#include "Utils/MetaHumanCacheManager.h"

DEFINE_LOG_CATEGORY(LogUnrealCV);

class FUnrealCVPlugin : public IModuleInterface
{
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

IMPLEMENT_MODULE(FUnrealCVPlugin, UnrealCV)

bool StartServerWithRetry(FUnrealcvServer &Server)
{
	const int32 MaxRetries = 20;
	// const int32 BaseDelayMs = 100;
	int32 CurrentPort = Server.Config.Port;
	bool StartSuccess = false;

	for (int32 Attempt = 0; Attempt < MaxRetries; ++Attempt)
	{
		StartSuccess = Server.TcpServer->Start(CurrentPort);
		if (StartSuccess)
		{
			if (Attempt > 0)
			{
				UE_LOG(LogUnrealCV, Warning, TEXT("Network server started successfully on port %d after %d attempts"), CurrentPort, Attempt + 1);
			}
			break;
		}

		if (Attempt < MaxRetries - 1)
		{
			// int32 DelayMs = BaseDelayMs * (1 << Attempt);
			UE_LOG(LogUnrealCV, Warning, TEXT("Failed to start network server on port %d (attempt %d/%d), retrying with port %d"),
				CurrentPort, Attempt + 1, MaxRetries, CurrentPort + 1 + Attempt);

			// FPlatformProcess::Sleep(DelayMs / 1000.0f);
			FPlatformProcess::Sleep(0.0f);
			CurrentPort++;
			Server.Config.Port = CurrentPort;
		}
	}

	if (!StartSuccess)
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("Failed to start network server after %d attempts"), MaxRetries);
	}

	return StartSuccess;
}

void FUnrealCVPlugin::StartupModule()
{
	FCommandLine::Append(TEXT(" -UNATTENDED"));

	FString Commandline = FCommandLine::Get();

	FMetaHumanCacheManager::Get().RegisterWithAssetManager();

	if (IsRunningDedicatedServer() ||
		Commandline.Contains(TEXT("cookcommandlet")) ||
		Commandline.Contains(TEXT("run=cook")))
	{
		return;
	}

	FUnrealcvServer &Server = FUnrealcvServer::Get();
	Server.RegisterCommandHandlers();

	int OverridePort = Server.Config.Port;
	if (FParse::Value(FCommandLine::Get(), TEXT("UnrealCVPort"), OverridePort)) {
	UE_LOG(LogUnrealCV, Warning, TEXT("Overriding listening port to %d"), OverridePort);
		Server.Config.Port = OverridePort;
	}

	int OverrideWidth = Server.Config.Width;
	if (FParse::Value(FCommandLine::Get(), TEXT("UnrealCVWidth"), OverrideWidth)) {
	UE_LOG(LogUnrealCV, Warning, TEXT("Overriding width to %d"), OverrideWidth);
		Server.Config.Width = OverrideWidth;
	}

	int OverrideHeight = Server.Config.Height;
	if (FParse::Value(FCommandLine::Get(), TEXT("UnrealCVHeight"), OverrideHeight)) {
	UE_LOG(LogUnrealCV, Warning, TEXT("Overriding height to %d"), OverrideHeight);
		Server.Config.Height = OverrideHeight;
	}

	float OverrideFOV = Server.Config.FOV;
	if (FParse::Value(FCommandLine::Get(), TEXT("UnrealCVFOV"), OverrideFOV)) {
	UE_LOG(LogUnrealCV, Warning, TEXT("Overriding FOV to %f"), OverrideFOV);
		Server.Config.FOV = OverrideFOV;
	}

	bool OverrideEnableInput = Server.Config.EnableInput;
	if (FParse::Bool(FCommandLine::Get(), TEXT("UnrealCVEnableInput"), OverrideEnableInput)) {
		if (OverrideEnableInput)
		{
			UE_LOG(LogUnrealCV, Warning, TEXT("Overriding EnableInput to true"));
		}
		else
		{
			UE_LOG(LogUnrealCV, Warning, TEXT("Overriding EnableInput to false"));
		}
		Server.Config.EnableInput = OverrideEnableInput;
	}

	bool OverrideExitOnFailure = Server.Config.ExitOnFailure;
	if (FParse::Bool(FCommandLine::Get(), TEXT("UnrealCVExitOnFailure"), OverrideExitOnFailure)) {
		if (OverrideExitOnFailure)
		{
			UE_LOG(LogUnrealCV, Warning, TEXT("Overriding ExitOnFailure to true"));
		}
		else
		{
			UE_LOG(LogUnrealCV, Warning, TEXT("Overriding ExitOnFailure to false"));
		}
		Server.Config.ExitOnFailure = OverrideExitOnFailure;
	}

	bool StartSuccess = StartServerWithRetry(Server);
	if (!StartSuccess)
	{
		if (Server.Config.ExitOnFailure)
		{
			UE_LOG(LogUnrealCV, Warning, TEXT("Requesting exit"));
			FGenericPlatformMisc::RequestExit(false);
		}
	}

	// Inject a UObject into the world to listen for world event
}

void FUnrealCVPlugin::ShutdownModule()
{
	FMetaHumanCacheManager::Get().UnregisterFromAssetManager();
}

