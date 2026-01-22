#include "DatasetAutomationHandler.h"
#include "DatasetAutomationBPLib.h"
#include "UnrealcvLog.h"

void FDatasetAutomationHandler::RegisterCommands()
{
	FDispatcherDelegate Cmd;
	FString Help;

	Cmd = FDispatcherDelegate::CreateRaw(this, &FDatasetAutomationHandler::GetTaskName);
	Help = "Get current task name (Trajectory or Omnimatte)";
	CommandDispatcher->BindCommand(TEXT("vget /datasetautomation/task_name"), Cmd, Help);

	Cmd = FDispatcherDelegate::CreateRaw(this, &FDatasetAutomationHandler::SetTaskName);
	Help = "Set task name (Trajectory or Omnimatte)";
	CommandDispatcher->BindCommand(TEXT("vset /datasetautomation/task_name [str]"), Cmd, Help);
}

FExecStatus FDatasetAutomationHandler::GetTaskName(const TArray<FString>& Args)
{
	FString TaskName = UDatasetAutomationBPLib::GetTaskName();
	return FExecStatus::OK(TaskName);
}

FExecStatus FDatasetAutomationHandler::SetTaskName(const TArray<FString>& Args)
{
	if (Args.Num() == 1)
	{
		FString TaskName = Args[0];
		bool Success = UDatasetAutomationBPLib::SetTaskName(TaskName);

		if (Success)
		{
			return FExecStatus::OK(FString::Printf(TEXT("Task name set to '%s'"), *TaskName));
		}
		else
		{
			return FExecStatus::Error(TEXT("Invalid task name. Must be 'Trajectory' or 'Omnimatte'"));
		}
	}
	else
	{
		return FExecStatus::Error(TEXT("Expect argument: task_name (Trajectory or Omnimatte)"));
	}
}
