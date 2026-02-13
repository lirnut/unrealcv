Dataset Generation Tutorial
=========================

Overview
--------

This tutorial covers the complete end-to-end workflow for generating layered audio-video
datasets using UnrealCV. The workflow integrates asset pools, scene composition, recording,
and batch automation to produce 400K+ layered video clips.

**Prerequisites:**

- Unreal Engine 5.2+ with UnrealCV plugin installed
- Assets registered in the asset pool
- PAK files mounted for dynamic loading (optional)

Workflow Overview
---------------

.. code-block::

   +-----------------+
   | Asset Pool      |
   | Setup           |
   +--------+--------+
            |
            v
   +-----------------+
   | Scene           |
   | Composition     |
   +--------+--------+
            |
            v
   +-----------------+
   | Recording       |
   | (Trajectory)    |
   +--------+--------+
            |
            v
   +-----------------+
   | Output          |
   | (5-Layer)      |
   +-----------------+

Step 1: Asset Pool Setup
-----------------------

Before generating scenes, ensure the asset pool is populated.

**Option A: Load Stable Assets Pack**

.. code-block:: cpp

   // In a Blueprint or C++ function
   USceneCompositionBPLib::LoadStableAssetsPack(GetWorld());

**Option B: Register Assets Manually**

.. code-block:: cpp

   // Static mesh without animation
   TMap<FString, FString> StaticMeshMetadata;
   StaticMeshMetadata.Add("Path", "/Game/Assets/Chair_01");
   StaticMeshMetadata.Add("Type", "StaticMesh");
   USceneCompositionBPLib::RegisterAssetWithMetadata("Occluder_Small", StaticMeshMetadata);

   // Blueprint actor with internal animation
   TMap<FString, FString> BlueprintMetadata;
   BlueprintMetadata.Add("Path", "/Game/Assets/Human_01_BP");
   BlueprintMetadata.Add("Type", "Blueprint");
   USceneCompositionBPLib::RegisterAssetWithMetadata("Foreground_Human", BlueprintMetadata);

   // Static mesh with animation sequence
   TMap<FString, FString> AnimMetadata;
   AnimMetadata.Add("Path", "/Game/Assets/Human_02");
   AnimMetadata.Add("Type", "SM+AnimSeq");
   AnimMetadata.Add("AnimSequence", "/Game/Animations/Walk/Walk_Cycle");
   USceneCompositionBPLib::RegisterAssetWithMetadata("Foreground_Human", AnimMetadata);

**Option C: PAK File Integration**

Mount PAK files and register assets:

.. code-block:: bash

   # Mount PAK
   vset /pak/mount D:/Assets/PackedAssets.pak

   # Register all assets in a package
   vset /pak/register /Game/PackedAssets Foreground_Human

   # List registered categories
   vget /assetpool/list

**Verify Asset Pool:**

.. code-block:: cpp

   // Get all categories
   TArray<FString> Categories = USceneCompositionBPLib::GetForegroundCategories();

   // Check asset count
   int32 Count = USceneCompositionBPLib::GetAssetCount("Foreground_Human");

Step 2: Scene Composition
------------------------

Generate random scenes programmatically.

**Method A: Programmatic Configuration**

.. code-block:: cpp

   FSceneGenerationParams Params;

   // Spawn area configuration
   Params.SpawnAreaMin = FVector2D(-500, -500);
   Params.SpawnAreaMax = FVector2D(500, 500);
   Params.GroundHeight = 0.0f;

   // Asset selection
   Params.ForegroundCategory = "Foreground_Human";
   Params.OccluderCategory = "Occluder_All";
   Params.OccluderCount = 5;

   // Camera configuration
   Params.CameraID = 1;
   Params.bAutoPositionCamera = true;
   Params.AutoPositionCameraHeight = 167.5f;
   Params.AutoPositionCameraDistance = 325.0f;

   // Generate scene
   FSceneHandle SceneHandle;
   USceneCompositionBPLib::GenerateRandomScene(GetWorld(), Params, SceneHandle);

**Method B: JSON Configuration**

Create JSON file (scene_config.json):

.. code-block:: json

   {
      "SpawnAreaMin": {"X": -500, "Y": -500},
      "SpawnAreaMax": {"X": 500, "Y": 500},
      "GroundHeight": 0.0,
      "ForegroundCategory": "Foreground_Human",
      "OccluderCategory": "Occluder_All",
      "OccluderCount": 5,
      "bAutoPositionCamera": true,
      "AutoPositionCameraHeight": 167.5,
      "AutoPositionCameraDistance": 325.0
   }

Load from JSON:

.. code-block:: cpp

   FSceneGenerationParams Params;
   USceneCompositionBPLib::CreateSceneParamsFromJson(
      GetWorld(),
      "D:/Config/scene_config.json",
      Params
   );

**Scene Handle Usage:**

The returned ``FSceneHandle`` contains references to all spawned actors:

.. code-block:: cpp

   // Access spawned actors
   AActor* Foreground = SceneHandle.ForegroundActor;
   TArray<AActor*> Occluders = SceneHandle.OccluderActors;

   // Recording metadata
   SceneHandle.SceneCategory = "Scene_001";
   SceneHandle.ForegroundSubcategory = "Human";
   SceneHandle.OccluderCategory = "Mixed";

   // Clean up when done
   USceneCompositionBPLib::ClearScene(SceneHandle);

Step 3: Recording Configuration
-------------------------------

Configure the capture actor for recording.

**Trajectory Recording (Camera Movement):**

.. code-block:: cpp

   // Start trajectory recording
   URecordingBPLib::StartTrajectoryRecording(
      1,                                    // CameraID
      "scene_001_traj_rotate_left_45",      // FileName
      "rotate_left_45",                     // TrajectoryType
      SceneHandle.ForegroundActor,          // Target
      30,                                  // FPS
      36.0f,                               // DegreesPerSecond
      12345                                // RandomSeed
   );

**Trajectory Types (10 per SOW):**

.. code-block::

   rotate_left_45      # 45° left rotation
   rotate_left_30      # 30° left rotation
   rotate_right_45     # 45° right rotation
   rotate_right_30     # 30° right rotation
   rotate_up_45        # 45° upward
   rotate_up_30        # 30° upward
   rotate_360          # Full 360° rotation
   zoom_in             # Move closer
   zoom_out            # Move away
   render_only         # Static camera (5 seconds)

**Simple Recording (No Camera Movement):**

.. code-block:: cpp

   URecordingBPLib::StartSimpleRecording(
      1,                      // CameraID
      "scene_001_simple",     // FileName
      30,                     // FPS
      5.0f,                   // DurationSeconds
      true,                   // bRecordLit
      true,                   // bRecordMask
      true,                   // bRecordNormal
      true,                   // bRecordDepth
      true                    // bRecordFlow
   );

**Configure Data Types:**

Set capture actor properties:

.. code-block:: cpp

   AFusionCamCaptureActor* CaptureActor = URecordingBPLib::GetCaptureActor(1);
   if (CaptureActor)
   {
      CaptureActor->bRecordRGB = true;
      CaptureActor->bRecordMask = true;
      CaptureActor->bRecordDepth = true;
      CaptureActor->bRecordNormal = true;
      CaptureActor->bRecordFlow = true;
      CaptureActor->bRecordMetadata = true;
      CaptureActor->bRecordAudio = true;

      CaptureActor->DataFolder.Path = TEXT("D:/Dataset/Output");
      CaptureActor->NumFrames = 121;
      CaptureActor->WarmUpFrames = 5;
   }

**Check Recording Status:**

.. code-block:: cpp

   if (URecordingBPLib::IsRecording(1))
   {
      // Recording in progress
   }

   // Stop recording
   URecordingBPLib::StopRecording(1);

Step 4: Batch Automation
------------------------

Automate the complete workflow for large-scale generation.

**Configure Automation:**

.. code-block:: cpp

   FAutomationConfig Config;

   // Scene configuration
   Config.TotalScenes = 100;
   Config.SceneParams = GeneratedParams;  // From Step 2
   Config.bLoadSceneParamsFromJson = false;

   // Recording configuration
   Config.OutputDirectory = "D:/Dataset/Output";
   Config.TrajectoryFPS = 30;
   Config.NumFrames = 121;
   Config.TrajectoryDegreesPerSecond = 36.0f;

   // Foreground movement
   Config.ForegroundMoveSpeed = 0.0f;
   Config.ForegroundMoveAngleOffset = 0.0f;

   // Start batch generation
   UDatasetAutomationBPLib::StartBatchGeneration(GetWorld(), Config);

**Monitor Progress:**

.. code-block:: cpp

   FAutomationStatus Status = UDatasetAutomationBPLib::GetAutomationStatus();

   // Check state
   if (Status.State == EDatasetGenerationState::Error)
   {
      UE_LOG(LogTemp, Error, TEXT("Error: %s"), *Status.ErrorMessage);
   }

   // Check progress
   float Progress = Status.Progress;  // 0.0 to 1.0
   int32 Current = Status.CurrentSceneIndex;
   int32 Total = Status.TotalScenes;

   // Check if running
   if (UDatasetAutomationBPLib::IsRunning())
   {
      // Batch generation active
   }

   // Stop batch generation
   UDatasetAutomationBPLib::StopBatchGeneration();

Step 5: Output Structure
-----------------------

Generated dataset structure:

.. code-block::

   D:/Dataset/Output/
   +-- scene_0001/
   |   +-- traj_rotate_left_45/
   |   |   +-- lit_0001.png
   |   |   +-- lit_0002.png
   |   |   +-- ...
   |   |   +-- mask_0001.png
   |   |   +-- ...
   |   |   +-- depth_0001.png
   |   |   +-- ...
   |   |   +-- normal_0001.png
   |   |   +-- ...
   |   |   +-- flow_0001.png
   |   |   +-- ...
   |   |   +-- camera.json
   |   |   +-- overview.json
   |   |   +-- audio.wav
   |   |   +-- composite.mp4
   |   +-- traj_rotate_right_45/
   |   +-- ...
   +-- scene_0002/
   +-- scene_0003/
   +-- ...

**Output Files:**

- ``lit_XXXX.png`` - RGB image
- ``mask_XXXX.png`` - Instance segmentation mask
- ``depth_XXXX.png`` - Depth map
- ``normal_XXXX.png`` - Surface normals
- ``flow_XXXX.png`` - Optical flow
- ``camera.json`` - Camera pose metadata
- ``overview.json`` - Scene metadata
- ``audio.wav`` - Audio track
- ``composite.mp4`` - Final video (if auto-generate enabled)

Complete Example Blueprint
------------------------

.. code-block::

   [Event BeginPlay]
      |
      +--> LoadStableAssetsPack
      |
      +--> CreateSceneParamsFromJson("config/scene_params.json")
      |
      +--> SetAutomationConfig
      |      TotalScenes = 100
      |      OutputDirectory = "D:/Dataset"
      |      TrajectoryFPS = 30
      |
      +--> StartBatchGeneration
      |
      +--> OnTick:
      |      GetAutomationStatus
      |      Update UI with progress
      |
      +--> If State == Completed:
      |      Log "Batch generation complete"
      |
      +--> If State == Error:
      |      Log error message
      |      StopBatchGeneration

Best Practices
--------------

1. **Asset Organization**: Use consistent category naming (Foreground_*, Occluder_*)

2. **Safe Points**: Pre-validate camera positions to avoid occlusion issues

3. **Occluder Count**: Start with 3-5 occluders, adjust based on dataset requirements

4. **Trajectory Variety**: Use all 10 trajectory types for diverse camera movements

5. **Batch Size**: Process in batches of 50-100 scenes to manage memory

6. **Monitoring**: Always check automation status during batch runs

7. **Cleanup**: Call ClearScene or ClearAllScenes between batches

Troubleshooting
--------------

**No assets in category:**
- Verify asset pool is loaded
- Check asset registration with metadata
- Ensure PAK files are mounted

**Scene generation fails:**
- Check SafePoints are valid locations
- Verify collision-free spawn locations
- Check GroundHeight matches terrain

**Recording produces no output:**
- Verify target sensor is set
- Check bRecord* flags are enabled
- Ensure output directory exists and is writable

**Batch generation stuck:**
- Check CurrentState for error
- Review ErrorMessage for details
- Verify all dependencies are loaded

See Also
--------

- :doc:`../api/recording-bplib` - Recording API reference
- :doc:`../api/scene-composition-bplib` - Scene composition API
- :doc:`../api/dataset-automation-bplib` - Batch automation API
- :doc:`../architecture/asset-pool` - Asset pool system
- :doc:`../architecture/recording-pipeline` - Recording architecture
- :doc:`../reference/pak-commands` - PAK management commands
