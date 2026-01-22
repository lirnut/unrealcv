#include "AutomationBPLib.h"
#include "Utils/GenericTickableObject.h"
#include "UnrealcvLog.h"
#include "UnrealcvServer.h"
#include "Engine/World.h"

TQueue<FString> UAutomationBPLib::CommandQueue;
FGenericTickableObject* UAutomationBPLib::TickableObject = nullptr;
bool UAutomationBPLib::bIsActive = false;

void UAutomationBPLib::PushCommand(const FString& Command)
{
	CommandQueue.Enqueue(Command);
	UE_LOG(LogUnrealCV, Log, TEXT("AutomationBPLib: Command queued: %s"), *Command);
}

void UAutomationBPLib::StartTicking()
{
	if (TickableObject)
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("AutomationBPLib: Already ticking"));
		delete TickableObject;
		TickableObject = nullptr;
	}

	bIsActive = true;
	TickableObject = new FGenericTickableObject();
	TickableObject->SetTickCallback(&UAutomationBPLib::OnTick);
	TickableObject->Activate();
	UE_LOG(LogUnrealCV, Log, TEXT("AutomationBPLib: Ticking started"));


	PushCommand("vset /captureactor/spawn_free_cam");
}

void UAutomationBPLib::StopTicking()
{
	if (TickableObject)
	{
		delete TickableObject;
		TickableObject = nullptr;
	}

	bIsActive = false;
	CommandQueue.Empty();
	UE_LOG(LogUnrealCV, Log, TEXT("AutomationBPLib: Ticking stopped"));
}

bool UAutomationBPLib::IsTickingActive()
{
	if (bIsActive)
	{
		check(TickableObject)
	}
	else
	{
		check(!TickableObject)
	}
	return bIsActive;
}

void UAutomationBPLib::OnTick(double DeltaTime)
{
	if (!bIsActive)
	{
		UE_LOG(LogUnrealCV, Warning, TEXT("AutomationBPLib: Tick called but not active"));
		return;
	}

	ProcessCommands();
}

void UAutomationBPLib::ProcessCommands()
{
	FString Command;
	while (CommandQueue.Dequeue(Command))
	{
		UE_LOG(LogUnrealCV, Log, TEXT("AutomationBPLib: Executing console command: %s"), *Command);

		UWorld* World = FUnrealcvServer::Get().GetWorld();
		if (World && World->GetFirstPlayerController())
		{
			FString Result = World->GetFirstPlayerController()->ConsoleCommand(Command, true);
			if (!Result.IsEmpty())
			{
				UE_LOG(LogUnrealCV, Log, TEXT("AutomationBPLib: Command result: %s"), *Result);
			}
			else
			{
				UE_LOG(LogUnrealCV, Log, TEXT("AutomationBPLib: Command executed (no result)"));
			}
		}
		else
		{
			UE_LOG(LogUnrealCV, Error, TEXT("AutomationBPLib: Failed to get World or PlayerController"));
		}
	}
}
