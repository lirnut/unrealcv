DatasetAutomationBPLib API Reference
===================================

Overview
--------

``UDatasetAutomationBPLib`` provides high-level batch generation orchestration for automated
dataset production. This library manages the complete workflow from scene generation through
trajectory recording, enabling scalable dataset generation without manual intervention.

**Header:** ``Source/UnrealCV/Public/BPFunctionLib/DatasetAutomationBPLib.h``

Automation States
----------------

EDatasetGenerationState
~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

   UENUM(BlueprintType)
   enum class EDatasetGenerationState : uint8
   {
      Idle,                // No automation running
      ExecutingCommand,    // Currently executing a command
      WaitingAsync,        // Waiting for async operation to complete
      Completed,           // All scenes completed successfully
      Error               // Error occurred during generation
   };

State machine for batch generation progress.

Automation Structures
--------------------

FAutomationStep
~~~~~~~~~~~~~~~

.. code-block:: cpp

   USTRUCT(BlueprintType)
   struct FAutomationStep
   {
      UPROPERTY(BlueprintReadWrite, Category = "Automation")
      FString Command;

      UPROPERTY(BlueprintReadWrite, Category = "Automation")
      FString StringParam;
   };

Single command step in the automation sequence.

FCompletedCommand
~~~~~~~~~~~~~~~~~

.. code-block:: cpp

   USTRUCT(BlueprintType)
   struct FCompletedCommand
   {
      UPROPERTY(BlueprintReadWrite, Category = "Automation")
      FAutomationStep Step;

      UPROPERTY(BlueprintReadWrite, Category = "Automation")
      FDateTime Timestamp;
   };

Record of a completed command with execution time.

FAutomationConfig
~~~~~~~~~~~~~~~~

.. code-block:: cpp

   USTRUCT(BlueprintType)
   struct FAutomationConfig
   {
      UPROPERTY(BlueprintReadWrite, Category = "Automation")
      int32 TotalScenes = 100;

      UPROPERTY(BlueprintReadWrite, Category = "Automation")
      FSceneGenerationParams SceneParams;

      UPROPERTY(BlueprintReadWrite, Category = "Automation")
      bool bLoadSceneParamsFromJson = true;

      UPROPERTY(BlueprintReadWrite, Category = "Automation")
      FString OutputDirectory = TEXT("C:/Dataset");

      UPROPERTY(BlueprintReadWrite, Category = "Automation")
      int32 TrajectoryFPS = 30;

      UPROPERTY(BlueprintReadWrite, Category = "Automation")
      float TrajectoryDegreesPerSecond = 36.0f;

      UPROPERTY(BlueprintReadWrite, Category = "Automation")
      int32 NumFrames = 121;

      UPROPERTY(BlueprintReadWrite, Category = "Automation")
      float ForegroundMoveSpeed = 0.0f;

      UPROPERTY(BlueprintReadWrite, Category = "Automation")
      float ForegroundMoveAngleOffset = 0.0f;
   };

Configuration for batch dataset generation.

**Fields:**

   - ``TotalScenes`` (int32): Number of scenes to generate (default: 100)
   - ``SceneParams`` (FSceneGenerationParams): Base scene configuration
   - ``bLoadSceneParamsFromJson`` (bool): Load scene params from JSON (default: true)
   - ``OutputDirectory`` (FString): Base output directory for recordings (default: "C:/Dataset")
   - ``TrajectoryFPS`` (int32): Recording frame rate (default: 30)
   - ``TrajectoryDegreesPerSecond`` (float): Camera rotation speed (default: 36.0)
   - ``NumFrames`` (int32): Frames per trajectory recording (default: 121)
   - ``ForegroundMoveSpeed`` (float): Foreground movement speed during recording (default: 0.0)
   - ``ForegroundMoveAngleOffset`` (float): Foreground movement angle (default: 0.0)

FAutomationStatus
~~~~~~~~~~~~~~~~~

.. code-block:: cpp

   USTRUCT(BlueprintType)
   struct FAutomationStatus
   {
      UPROPERTY(BlueprintReadOnly, Category = "Automation")
      EDatasetGenerationState State = EDatasetGenerationState::Idle;

      UPROPERTY(BlueprintReadOnly, Category = "Automation")
      int32 CurrentSceneIndex = 0;

      UPROPERTY(BlueprintReadOnly, Category = "Automation")
      int32 TotalScenes = 0;

      UPROPERTY(BlueprintReadOnly, Category = "Automation")
      float Progress = 0.0f;

      UPROPERTY(BlueprintReadOnly, Category = "Automation")
      FString CurrentFileName = TEXT("");

      UPROPERTY(BlueprintReadOnly, Category = "Automation")
      FString ErrorMessage = TEXT("");

      UPROPERTY(BlueprintReadOnly, Category = "Automation")
      FIntPoint ChosenRes = FIntPoint(0, 0);

      UPROPERTY(BlueprintReadOnly, Category = "Automation")
      float ChosenFOV = 90.0f;

      UPROPERTY(BlueprintReadOnly, Category = "Automation")
      float RandomTargetHeight = 0.0f;
   };

Real-time status of batch generation.

**Fields:**

   - ``State`` (EDatasetGenerationState): Current automation state
   - ``CurrentSceneIndex`` (int32): Index of current scene being processed
   - ``TotalScenes`` (int32): Total scenes configured
   - ``Progress`` (float): Overall progress (0.0 to 1.0)
   - ``CurrentFileName`` (FString): Currently recording filename
   - ``ErrorMessage`` (FString): Error message if state is Error
   - ``ChosenRes`` (FIntPoint): Chosen output resolution
   - ``ChosenFOV`` (float): Chosen camera field of view
   - ``RandomTargetHeight`` (float): Random height offset for target

Batch Generation Functions
--------------------------

StartBatchGeneration
~~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

   UFUNCTION(BlueprintCallable, Category = "UnrealCV|Automation", meta = (WorldContext = "WorldContextObject"))
   static bool StartBatchGeneration(
      UObject* WorldContextObject,
      const FAutomationConfig& Config
   );

Initiates automated batch dataset generation.

**Parameters:**

   - ``WorldContextObject`` (UObject*): World context for scene operations
   - ``Config`` (FAutomationConfig): Generation configuration

**Returns:** ``true`` if batch generation started successfully

**Workflow:**

   1. Validates configuration
   2. Loads scene parameters (from JSON if configured)
   3. Iterates through all scenes:
      a. Generates random scene
      b. Records all trajectories (10 per scene)
      c. Clears scene
   4. Reports completion

**Example:**

   .. code-block:: blueprint

      FAutomationConfig Config;
      Config.TotalScenes = 100;
      Config.OutputDirectory = "C:/Dataset/Scene_001";
      Config.TrajectoryFPS = 30;
      Config.NumFrames = 121;

      UDatasetAutomationBPLib::StartBatchGeneration(GetWorld(), Config);

StopBatchGeneration
~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

   UFUNCTION(BlueprintCallable, Category = "UnrealCV|Automation")
   static void StopBatchGeneration();

Stops the currently running batch generation.

**Note:** Does not clean up partially generated scenes. Call ``ClearAllScanes`` afterward.

Status Query Functions
---------------------

GetAutomationStatus
~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

   UFUNCTION(BlueprintPure, Category = "UnrealCV|Automation")
   static FAutomationStatus GetAutomationStatus();

Gets the current automation status.

**Returns:** FAutomationStatus with current progress

GetAutomationStatusString
~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

   UFUNCTION(BlueprintPure, Category = "UnrealCV|Automation")
   static FString GetAutomationStatusString();

Gets human-readable automation status summary.

**Returns:** Status string for UI display

IsRunning
~~~~~~~~

.. code-function:: cpp

   UFUNCTION(BlueprintPure, Category = "UnrealCV|Automation")
   static bool IsRunning();

Checks if batch generation is currently active.

**Returns:** ``true`` if automation is running

Command Queue Functions
----------------------

SetMap
~~~~~~

.. code-block:: cpp

   UFUNCTION(BlueprintCallable, Category = "UnrealCV|Automation", meta = (WorldContext = "WorldContextObject"))
   static bool SetMap(UObject* WorldContextObject, const FString& MapName);

Sets the map to use for scene generation.

**Parameters:**

   - ``WorldContextObject`` (UObject*): World context
   - ``MapName`` (FString): Name of the map to load

**Returns:** ``true`` if map set successfully

SetTaskName
~~~~~~~~~~~

.. code-block:: cpp

   UFUNCTION(BlueprintCallable, Category = "UnrealCV|Automation")
   static bool SetTaskName(const FString& InTaskName);

Sets the task name for batch generation.

**Parameters:**

   - ``InTaskName`` (FString): Task identifier

**Returns:** ``true`` if set successfully

GetTaskName
~~~~~~~~~~~

.. code-block:: cpp

   UFUNCTION(BlueprintPure, Category = "UnrealCV|Automation")
   static FString GetTaskName();

Gets the current task name.

**Returns:** Current task name

SetExternalCommandSequence
~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

   static bool SetExternalCommandSequence(const TArray<FAutomationStep>& Sequence);

Sets a custom command sequence for execution.

**Parameters:**

   - ``Sequence`` (TArray<FAutomationStep>): Array of commands to execute

**Returns:** ``true`` if sequence set successfully

ParseCommandSequenceJson
~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

   UFUNCTION(BlueprintCallable, Category = "UnrealCV|Automation")
   static bool ParseCommandSequenceJson(const FString& JsonContent, FString& OutErrorMessage);

Parses command sequence from JSON content.

**Parameters:**

   - ``JsonContent`` (FString): JSON string containing command sequence
   - ``OutErrorMessage`` (FString&): Error message if parsing fails

**Returns:** ``true`` if parsed successfully

**JSON Format:**

   .. code-block:: json

      [
         {"Command": "generate_scene", "StringParam": "Foreground_Human"},
         {"Command": "record_trajectory", "StringParam": "rotate_left_45"},
         {"Command": "clear_scene"}
      ]

GetCommandQueueSummary
~~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

   UFUNCTION(BlueprintPure, Category = "UnrealCV|Automation")
   static FString GetCommandQueueSummary();

Gets summary of commands in the queue.

**Returns:** Summary string for display

GetCommandHistory
~~~~~~~~~~~~~~~~

.. code-block:: cpp

   UFUNCTION(BlueprintPure, Category = "UnrealCV|Automation")
   static const TArray<FCompletedCommand>& GetCommandHistory();

Gets all completed commands with timestamps.

**Returns:** Array of completed commands

Internal State Management
------------------------

The following functions manage internal automation state:

**ProcessState** - Handles state transitions based on current state
**BuildCommandSequenceForScene** - Builds commands for a single scene
**ExecuteNextCommand** - Executes next command in queue
**ExecuteCommand** - Executes a single automation command
**GenerateSceneID** - Generates unique scene identifier
**GenerateOutputPath** - Generates output path for recording
**StartTrajectoryRecording** - Initiates trajectory recording

**Status Management:**

- ``CurrentConfig`` - Active automation configuration
- ``CurrentStatus`` - Active automation status
- ``CurrentScene`` - Current scene being processed
- ``CommandQueue`` - Pending commands
- ``CommandHistory`` - Completed commands

See Also
--------

- :doc:`../reference/dataset-automation-commands` - TCP command reference for automation
- :doc:`scene-composition-bplib` - Scene composition API
- :doc:`recording-bplib` - Recording API
- :doc:`../architecture/recording-pipeline` - Recording system architecture
- :doc:`dataset-generation` - Complete workflow tutorial
