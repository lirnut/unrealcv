SceneCompositionBPLib API Reference
=================================

Overview
--------

``USceneCompositionBPLib`` provides Blueprint-accessible functions for automated scene
generation with randomized foreground actors, occluders, and camera placement. This library
is essential for SOW (Statement of Work) dataset production workflows.

**Header:** ``Source/UnrealCV/Public/BPFunctionLib/SceneCompositionBPLib.h``

Scene Generation Structures
---------------------------

FSceneGenerationParams
~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

   USTRUCT(BlueprintType)
   struct FSceneGenerationParams
   {
      UPROPERTY(BlueprintReadWrite, Category = "UnrealCV|SceneComposition")
      FVector2D SpawnAreaMin;

      UPROPERTY(BlueprintReadWrite, Category = "UnrealCV|SceneComposition")
      FVector2D SpawnAreaMax;

      UPROPERTY(BlueprintReadWrite, Category = "UnrealCV|SceneComposition")
      float GroundHeight;

      UPROPERTY(BlueprintReadWrite, Category = "UnrealCV|SceneComposition")
      FString ForegroundPathSpec;

      UPROPERTY(BlueprintReadWrite, Category = "UnrealCV|SceneComposition")
      FString ForegroundCategory;

      UPROPERTY(BlueprintReadWrite, Category = "UnrealCV|SceneComposition")
      FString OccluderPathSpec;

      UPROPERTY(BlueprintReadWrite, Category = "UnrealCV|SceneComposition")
      FString OccluderCategory;

      UPROPERTY(BlueprintReadWrite, Category = "UnrealCV|SceneComposition")
      int32 OccluderCount;

      UPROPERTY(BlueprintReadWrite, Category = "UnrealCV|SceneComposition")
      int32 CameraID;

      UPROPERTY(BlueprintReadWrite, Category = "UnrealCV|SceneComposition")
      bool bAutoPositionCamera;

      UPROPERTY(BlueprintReadWrite, Category = "UnrealCV|SceneComposition")
      float ForegroundYaw;

      UPROPERTY(BlueprintReadWrite, Category = "UnrealCV|SceneComposition")
      TArray<FVector> SafePoints;

      UPROPERTY(BlueprintReadWrite, Category = "UnrealCV|SceneComposition")
      float AutoPositionCameraHeight;

      UPROPERTY(BlueprintReadWrite, Category = "UnrealCV|SceneComposition")
      float AutoPositionCameraDistance;

      UPROPERTY(BlueprintReadWrite, Category = "UnrealCV|SceneComposition")
      float AutoPositionCameraAngleOffset;
   };

Configuration structure for automated scene generation.

**Fields:**

   - ``SpawnAreaMin`` (FVector2D): Minimum X,Y bounds for actor spawning (default: 0,0)
   - ``SpawnAreaMax`` (FVector2D): Maximum X,Y bounds for actor spawning (default: 1000,1000)
   - ``GroundHeight`` (float): Base ground level for spawned actors (default: 100.0)
   - ``ForegroundPathSpec`` (FString): Asset path pattern for foreground selection
   - ``ForegroundCategory`` (FString): Category name for foreground assets (default: "Foreground_Human")
   - ``OccluderPathSpec`` (FString): Asset path pattern for occluder selection
   - ``OccluderCategory`` (FString): Category name for occluder assets (default: "Occluder_All")
   - ``OccluderCount`` (int32): Number of occluders to spawn (default: 3)
   - ``CameraID`` (int32): Camera ID to use for the scene (default: 1)
   - ``bAutoPositionCamera`` (bool): Auto-position camera to view target (default: true)
   - ``ForegroundYaw`` (float): Rotation for foreground actor (-1 = random, default: -1)
   - ``SafePoints`` (TArray<FVector>): Pre-validated spawn locations for camera
   - ``AutoPositionCameraHeight`` (float): Camera height when auto-positioning (default: 167.5)
   - ``AutoPositionCameraDistance`` (float): Camera distance from target (default: 325.0)
   - ``AutoPositionCameraAngleOffset`` (float): Camera angle offset (default: 0.0)

FSceneHandle
~~~~~~~~~~~~

.. code-block:: cpp

   USTRUCT(BlueprintType)
   struct FSceneHandle
   {
      UPROPERTY(BlueprintReadOnly, Category = "UnrealCV|SceneComposition")
      FString SceneID;

      UPROPERTY(BlueprintReadOnly, Category = "UnrealCV|SceneComposition")
      AActor* ForegroundActor;

      UPROPERTY(BlueprintReadOnly, Category = "UnrealCV|SceneComposition")
      TArray<AActor*> OccluderActors;

      UPROPERTY(BlueprintReadOnly, Category = "UnrealCV|SceneComposition")
      AActor* DirectionalLight;

      UPROPERTY(BlueprintReadOnly, Category = "UnrealCV|SceneComposition")
      int32 CameraID;

      UPROPERTY(BlueprintReadOnly, Category = "UnrealCV|SceneComposition")
      FString ForegroundCategory;

      UPROPERTY(BlueprintReadOnly, Category = "UnrealCV|SceneComposition")
      class ANavAgentController* NavController;

      UPROPERTY(BlueprintReadOnly, Category = "UnrealCV|SceneComposition")
      bool bHasNavigation;

      // Recording Metadata
      UPROPERTY(BlueprintReadWrite, Category = "UnrealCV|Recording")
      FString SceneCategory;

      UPROERTY(BlueprintReadWrite, Category = "UnrealCV|Recording")
      FString ForegroundSubcategory;

      UPROPERTY(BlueprintReadWrite, Category = "UnrealCV|Recording")
      FString OccluderCategory;

      UPROPERTY(BlueprintReadWrite, Category = "UnrealCV|Recording")
      TArray<FOccluderMetadata> OccluderMetadataList;

      UPROPERTY(BlueprintReadWrite, Category = "UnrealCV|Recording")
      TMap<FString, FString> ForegroundObjectMetadata;
   };

Reference to a generated scene for later manipulation and cleanup.

FOccluderMetadata
~~~~~~~~~~~~~~~~~

.. code-block:: cpp

   USTRUCT(BlueprintType)
   struct FOccluderMetadata
   {
      UPROPERTY(BlueprintReadWrite, Category = "UnrealCV|Recording")
      TMap<FString, FString> Metadata;
   };

Metadata wrapper for occluder actors.

Core Scene Generation Functions
-------------------------------

GenerateRandomScene
~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

   UFUNCTION(BlueprintCallable, Category = "UnrealCV|SceneComposition", meta = (WorldContext = "WorldContextObject"))
   static bool GenerateRandomScene(
      UObject* WorldContextObject,
      const FSceneGenerationParams& Params,
      FSceneHandle& OutSceneHandle
   );

Generates a complete random scene with foreground, occluders, and camera setup.

**Parameters:**

   - ``WorldContextObject`` (UObject*): World context for actor operations
   - ``Params`` (FSceneGenerationParams): Scene generation configuration
   - ``OutSceneHandle`` (FSceneHandle&): Output scene reference

**Returns:** ``true`` if scene generated successfully

**Process:**

   1. Spawns foreground actor from specified category
   2. Spawns specified number of occluders at collision-free locations
   3. Optionally positions camera to view target
   4. Returns scene handle for later recording/cleanup

**Example:**

   .. code-block:: blueprint

      FSceneGenerationParams Params;
      Params.ForegroundCategory = "Foreground_Human";
      Params.OccluderCategory = "Occluder_All";
      Params.OccluderCount = 5;
      Params.SpawnAreaMin = FVector2D(0, 0);
      Params.SpawnAreaMax = FVector2D(1000, 1000);

      FSceneHandle SceneHandle;
      USceneCompositionBPLib::GenerateRandomScene(GetWorld(), Params, SceneHandle);

CreateSceneParamsFromJson
~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

   UFUNCTION(BlueprintCallable, Category = "UnrealCV|SceneComposition", meta = (WorldContext = "WorldContextObject"))
   static bool CreateSceneParamsFromJson(
      UObject* WorldContextObject,
      const FString& JsonFilePath,
      FSceneGenerationParams& OutParams
   );

Creates scene parameters from JSON configuration file.

**Parameters:**

   - ``WorldContextObject`` (UObject*): World context
   - ``JsonFilePath`` (FString): Path to JSON configuration file
   - ``OutParams`` (FSceneGenerationParams&): Output parameters structure

**Returns:** ``true`` if JSON parsed successfully

**JSON Format:**

   .. code-block:: json

      {
         "SpawnAreaMin": {"X": 0, "Y": 0},
         "SpawnAreaMax": {"X": 1000, "Y": 1000},
         "GroundHeight": 100.0,
         "ForegroundCategory": "Foreground_Human",
         "OccluderCategory": "Occluder_All",
         "OccluderCount": 5,
         "bAutoPositionCamera": true,
         "AutoPositionCameraHeight": 167.5,
         "AutoPositionCameraDistance": 325.0
      }

LoadStableAssetsPack
~~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

   UFUNCTION(BlueprintCallable, Category = "UnrealCV|SceneComposition", meta = (WorldContext = "WorldContextObject"))
   static void LoadStableAssetsPack(UObject* WorldContextObject);

Loads the predefined stable assets pack into the asset pool.

**Parameters:**

   - ``WorldContextObject`` (UObject*): World context

**Note:** This must be called before scene generation to ensure assets are available.

Scene Cleanup Functions
------------------------

ClearScene
~~~~~~~~~~

.. code-block:: cpp

   UFUNCTION(BlueprintCallable, Category = "UnrealCV|SceneComposition")
   static void ClearScene(const FSceneHandle& SceneHandle);

Destroys all actors associated with a scene.

**Parameters:**

   - ``SceneHandle`` (FSceneHandle): Scene to clear

ClearAllScenes
~~~~~~~~~~~~~~

.. code-block:: cpp

   UFUNCTION(BlueprintCallable, Category = "UnrealCV|SceneComposition", meta = (WorldContext = "WorldContextObject"))
   static void ClearAllScenes(UObject* WorldContextObject);

Destroys all actors from all generated scenes.

**Parameters:**

   - ``WorldContextObject`` (UObject*): World context

Safe Point Functions
--------------------

AddSafePointToScene
~~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

   UFUNCTION(BlueprintCallable, Category = "UnrealCV|SceneComposition")
   static bool AddSafePointToScene(const FString& SceneName, FVector Location);

Adds a validated camera position to a scene's safe points list.

**Parameters:**

   - ``SceneName`` (FString): Scene identifier
   - ``Location`` (FVector): Camera position to add

**Returns:** ``true`` if added successfully

AddSafePointToCurrentScene
~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

   UFUNCTION(BlueprintCallable, Category = "UnrealCV|SceneComposition", meta = (WorldContext = "WorldContextObject"))
   static bool AddSafePointToCurrentScene(UObject* WorldContextObject, FVector Location);

Adds a safe point to the most recently created scene.

**Parameters:**

   - ``WorldContextObject`` (UObject*): World context
   - ``Location`` (FVector): Camera position to add

**Returns:** ``true`` if added successfully

GetSafePointsForScene
~~~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

   UFUNCTION(BlueprintPure, Category = "UnrealCV|SceneComposition")
   static TArray<FVector> GetSafePointsForScene(const FString& SceneName);

Gets all safe points for a scene.

**Parameters:**

   - ``SceneName`` (FString): Scene identifier

**Returns:** Array of safe camera positions

Actor Spawning Functions
------------------------

SpawnRandomOccluders
~~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

   UFUNCTION(BlueprintCallable, Category = "UnrealCV|SceneComposition", meta = (WorldContext = "WorldContextObject"))
   static TArray<AActor*> SpawnRandomOccluders(
      UObject* WorldContextObject,
      int32 Count,
      FVector CameraPosition,
      FVector ForegroundPosition,
      const FString& OccluderPathSpec,
      const FString& OccluderCategory,
      FSceneHandle& OutSceneHandle
   );

Spawns random occluders between camera and foreground positions.

**Parameters:**

   - ``WorldContextObject`` (UObject*): World context
   - ``Count`` (int32): Number of occluders to spawn
   - ``CameraPosition`` (FVector): Camera location
   - ``ForegroundPosition`` (FVector): Foreground actor location
   - ``OccluderPathSpec`` (FString): Asset path pattern
   - ``OccluderCategory`` (FString): Occluder category name
   - ``OutSceneHandle`` (FSceneHandle&): Output scene handle

**Returns:** Array of spawned occluder actors

Asset Pool Management Functions
------------------------------

RegisterAsset
~~~~~~~~~~~~~

.. code-block:: cpp

   UFUNCTION(BlueprintCallable, Category = "UnrealCV|SceneComposition")
   static void RegisterAsset(const FString& Category, const FString& AssetPath);

Registers an asset in the asset pool.

**Parameters:**

   - ``Category`` (FString): Asset category (e.g., "Foreground_Human")
   - ``AssetPath`` (FString): Full asset path (e.g., "/Game/Assets/Human_Mesh")

**Note:** Minimal registration with default metadata. Use ``RegisterAssetWithMetadata``
for full control.

RegisterAssetWithMetadata
~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

   UFUNCTION(BlueprintCallable, Category = "UnrealCV|SceneComposition")
   static void RegisterAssetWithMetadata(const FString& Category, const TMap<FString, FString>& Metadata);

Registers an asset with full metadata.

**Parameters:**

   - ``Category`` (FString): Asset category
   - ``Metadata`` (TMap<FString, FString>): Asset metadata map

**Required Metadata Keys:**

   - ``"Path"``: Main asset path (REQUIRED)
   - ``"Type"``: Asset type (REQUIRED: "StaticMesh", "Blueprint", or "SM+AnimSeq")
   - ``"AnimSequence"``: Animation path (REQUIRED for "SM+AnimSeq" type only)

**Example:**

   .. code-block:: blueprint

      TMap<FString, FString> Metadata;
      Metadata.Add("Path", "/Game/Assets/Human_01");
      Metadata.Add("Type", "SM+AnimSeq");
      Metadata.Add("AnimSequence", "/Game/Animations/Walk_Cycle");
      USceneCompositionBPLib::RegisterAssetWithMetadata("Foreground_Human", Metadata);

GetForegroundCategories
~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

   UFUNCTION(BlueprintPure, Category = "UnrealCV|SceneComposition")
   static TArray<FString> GetForegroundCategories();

Gets all registered foreground categories.

**Returns:** Array of category names

GetOccluderCategories
~~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

   UFUNCTION(BlueprintPure, Category = "UnrealCV|SceneComposition")
   static TArray<FString> GetOccluderCategories();

Gets all registered occluder categories.

**Returns:** Array of category names

GetAssetCount
~~~~~~~~~~~~

.. code-block:: cpp

   UFUNCTION(BlueprintPure, Category = "UnrealCV|SceneComposition")
   static int32 GetAssetCount(const FString& Category);

Gets the number of assets in a category.

**Parameters:**

   - ``Category`` (FString): Category name

**Returns:** Count of assets in category

HasCategory
~~~~~~~~~~~

.. code-block:: cpp

   UFUNCTION(BlueprintPure, Category = "UnrealCV|SceneComposition")
   static bool HasCategory(const FString& Category);

Checks if a category exists in the asset pool.

**Parameters:**

   - ``Category`` (FString): Category name

**Returns:** ``true`` if category exists

Camera Positioning Functions
----------------------------

PositionCameraToViewTarget
~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

   UFUNCTION(BlueprintCallable, Category = "UnrealCV|SceneComposition")
   static bool PositionCameraToViewTarget(
      int32 CameraID,
      AActor* TargetActor,
      float MinDistance = 200.0f,
      float MaxDistance = 700.0f,
      float MinAngle = -20.0f,
      float MaxAngle = 20.0f
   );

Positions camera to view a target actor within specified constraints.

**Parameters:**

   - ``CameraID`` (int32): Camera identifier
   - ``TargetActor`` (AActor*): Actor to frame in camera view
   - ``MinDistance`` (float): Minimum camera-to-target distance (default: 200)
   - ``MaxDistance`` (float): Maximum camera-to-target distance (default: 700)
   - ``MinAngle`` (float): Minimum Yaw offset from target (default: -20)
   - ``MaxAngle`` (float): Maximum Yaw offset from target (default: 20)

**Returns:** ``true`` if camera positioned successfully

See Also
--------

- :doc:`../architecture/asset-pool` - Asset Pool Manager architecture
- :doc:`../architecture/recording-pipeline` - Recording system overview
- :doc:`dataset-generation` - Complete workflow tutorial
