#pragma once

#include "CoreMinimal.h"

class FExecStatus;
DECLARE_DELEGATE_RetVal(FExecStatus, FPromiseDelegate); // Check task status

/**
 * Return by async task, used to check status to see whether the task is finished.
 */
class UNREALCV_API FPromise
{
private:
	/** The method to check whether this promise is alreay completed */
	FPromiseDelegate PromiseDelegate;
public:
	bool bIsValid;
	FDateTime InitTime;
	FPromise() { bIsValid = false; }
	FPromise(FPromiseDelegate InPromiseDelegate) : PromiseDelegate(InPromiseDelegate)
	{
		bIsValid = true;
		InitTime = FDateTime::Now();
	}
	/** Use PromiseDelegate to check whether the task already completed */
	FExecStatus CheckStatus();
	float GetRunningTime()
	{
		FTimespan Elapsed = FDateTime::Now() - InitTime;
		return Elapsed.GetTotalSeconds();
	}
};

enum FExecStatusType
{
	OK,
	ErrorMsg,
};

/**
 * Present the return value of a command. If the FExecStatusType is pending, check the promise value.
 */
class UNREALCV_API FExecStatus
{
public:
	// Define some typical FExecStatus for convinience
	/** OK : Message */
	static FExecStatus OK(FString Message="");
	/** Error : ErrorMessage */
	static FExecStatus Error(FString ErrorMessage);
	/** Error : Argument Invalid */
	static FExecStatus InvalidArgument;
	/** Error : Not implemented */
	static FExecStatus NotImplemented;
	/** Error : Invalid Pointer */
	static FExecStatus InvalidPointer;
	/** Binary : A binary array */
	static FExecStatus Binary(TArray<uint8>& InBinaryData, bool bMove = false);

	/** The message body of this ExecStatus, the full message will also include the ExecStatusType */
	FString MessageBody;
	/** An enum type to show the ExecStatus */
	FExecStatusType ExecStatusType;
	bool IsOK() { return (ExecStatusType == FExecStatusType::OK); }

	~FExecStatus();
	/** Convert this ExecStatus to String */
	FString GetMessage() const;

	/** Convert this ExecStatus to a binary array */
	TArray<uint8> GetData() const;

	/** Add this FExecStatus with other FExecStatus, useful for executing a few commands at the same time */
	FExecStatus& operator+=(const FExecStatus& InExecStatus);

	/** Return the promise of this FExecStatus, will only be set if the status is pending */
	FPromise& GetPromise();

	/** Convert string to binary array */
	static void BinaryArrayFromString(const FString& Message, TArray<uint8>& OutBinaryArray);

public:
	/** Binary data */
	// perf opt
	TArray<uint8> BinaryData;

	/** Move Constructor */
	// perf opt
	FExecStatus(FExecStatus&& InExecStatus);
	FExecStatus& operator = (FExecStatus&& InExecStatus);

	// delete Copy Constructor
	FExecStatus(const FExecStatus& InExecStatus) = delete;

public:
	static FExecStatus GetInvalidArgument() { return FExecStatus(FExecStatusType::ErrorMsg, "Argument Invalid"); }
	static FExecStatus GetNotImplemented() { return FExecStatus(FExecStatusType::ErrorMsg, "Not Implemented"); }
	static FExecStatus GetInvalidPointer() { return FExecStatus(FExecStatusType::ErrorMsg, "Pointer to object invalid, check log for details"); }

private:
	/** The promise to check result, only useful for async tasks */
	FPromise Promise;
	FExecStatus(FExecStatusType InExecStatusType, FString Message);
	// For query
	FExecStatus(FExecStatusType InExecStatusType, FPromise Promise);
	/** Construct from binary data */
	FExecStatus(FExecStatusType InExecStatusType, TArray<uint8>& InBinaryData, bool bMove);
};

bool operator==(const FExecStatus& ExecStatus, const FExecStatusType& ExecStatusEnum);
bool operator!=(const FExecStatus& ExecStatus, const FExecStatusType& ExecStatusEnum);
/** Move Assignment Operator */
// perf opt

