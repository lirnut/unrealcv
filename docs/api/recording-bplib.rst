RecordingBPLib API Reference
===========================

Overview
--------

``URecordingBPLib`` provides Blueprint-accessible functions for controlling camera recording
without requiring TCP server communication. This library wraps the recording functionality
from ``CameraHandler`` for direct Blueprint and C++ usage, enabling high-throughput
dataset generation workflows.

**Header:** ``Source/UnrealCV/Public/BPFunctionLib/RecordingBPLib.h``

Camera ID Format
----------------

Recording functions support two camera ID formats:

**Integer Format (Legacy)**
   Camera IDs assigned in creation order: ``0``, ``1``, ``2``, ...
   Unstable across sessions if camera creation order changes.

**CID Format (Recommended)**
   Stable identifier format: ``CID-ActorName-UUID``
   Tied to the sensor instance and persists across sessions.

All functions accept either format via overloaded signatures.

Recording Control Functions
---------------------------

StopRecording
~~~~~~~~~~~~~

.. code-block:: cpp

   UFUNCTION(BlueprintCallable, Category = "UnrealCV|Recording")
   static bool StopRecording(int32 CameraID);

   static bool StopRecording(const FString& IDString);

Stops an active recording session for the specified camera.

**Parameters:**

   - ``CameraID`` (int32): Integer camera identifier
   - ``IDString`` (FString): CID format camera identifier

**Returns:** ``true`` if recording was stopped successfully, ``false`` if no active recording found

**Example:**

   .. code-block:: blueprint

      // Stop recording on camera 1
      URecordingBPLib::StopRecording(1);

      // Stop using CID format
      URecordingBPLib::StopRecording("CID-FusionCamSensor-ABC123");

IsRecording
~~~~~~~~~~~

.. code-block:: cpp

   UFUNCTION(BlueprintPure, Category = "UnrealCV|Recording")
   static bool IsRecording(int32 CameraID);

   static bool IsRecording(const FString& IDString);

Checks whether a camera is currently recording.

**Parameters:**

   - ``CameraID`` (int32): Integer camera identifier
   - ``IDString`` (FString): CID format camera identifier

**Returns:** ``true`` if recording is in progress, ``false`` otherwise

Camera Discovery Functions
--------------------------

GetAllCameras
~~~~~~~~~~~~~

.. code-block:: cpp

   UFUNCTION(BlueprintPure, Category = "UnrealCV|Recording")
   static TArray<class UFusionCamSensor*> GetAllCameras();

Retrieves all active FusionCamSensor instances in the world.

**Returns:** Array of all UFusionCamSensor objects

GetCameraByID
~~~~~~~~~~~~~

.. code-block:: cpp

   UFUNCTION(BlueprintPure, Category = "UnrealCV|Recording")
   static class UFusionCamSensor* GetCameraByID(int32 CameraID);

Gets a specific camera by its integer ID.

**Parameters:**

   - ``CameraID`` (int32): Integer camera identifier

**Returns:** Pointer to the UFusionCamSensor, or nullptr if not found

GetCameraName
~~~~~~~~~~~~~

.. code-block:: cpp

   UFUNCTION(BlueprintPure, Category = "UnrealCV|Recording")
   static FString GetCameraName(int32 CameraID);

Gets the display name for a camera.

**Parameters:**

   - ``CameraID`` (int32): Integer camera identifier

**Returns:** Camera name string (e.g., "CID-FusionCamSensor-ABC123")

GetCameraCount
~~~~~~~~~~~~~~

.. code-block:: cpp

   UFUNCTION(BlueprintPure, Category = "UnrealCV|Recording")
   static int32 GetCameraCount();

Gets the total number of active cameras.

**Returns:** Count of active cameras

Camera Creation Functions
--------------------------

CreateFreeCamera
~~~~~~~~~~~~~~~~

.. code-block:: cpp

   UFUNCTION(BlueprintCallable, Category = "UnrealCV|Recording", meta = (WorldContext = "WorldContextObject"))
   static int32 CreateFreeCamera(
      UObject* WorldContextObject,
      FVector Location = FVector::ZeroVector,
      FRotator Rotation = FRotator::ZeroRotator
   );

Creates a new free camera at the specified location and rotation.

**Parameters:**

   - ``WorldContextObject`` (UObject*): World context for actor creation
   - ``Location`` (FVector): Initial camera position (default: zero vector)
   - ``Rotation`` (FRotator): Initial camera rotation (default: zero rotator)

**Returns:** Integer ID of the created camera

Trajectory Recording Functions
-----------------------------

StartTrajectoryRecording
~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

   UFUNCTION(BlueprintCallable, Category = "UnrealCV|Recording|Trajectory")
   static bool StartTrajectoryRecording(
      int32 CameraID,
      const FString& FileName,
      const FString& TrajectoryType,
      AActor* Target,
      int32 FPS = 30,
      float DegreesPerSecond = 36.0f,
      int32 RandomSeed = -1
   );

Starts trajectory-based recording with automated camera movement.

**Parameters:**

   - ``CameraID`` (int32): Camera identifier
   - ``FileName`` (FString): Output filename for recorded data
   - ``TrajectoryType`` (FString): Type of camera trajectory (see below)
   - ``Target`` (AActor*): Actor to track during recording
   - ``FPS`` (int32): Frames per second (default: 30)
   - ``DegreesPerSecond`` (float): Rotation speed for trajectory (default: 36.0)
   - ``RandomSeed`` (int32): Seed for random trajectory (default: -1 for random)

**Returns:** ``true`` if recording started successfully

**Trajectory Types:**

   - ``rotate_left_45`` - Rotate camera 45 degrees left around target
   - ``rotate_left_30`` - Rotate camera 30 degrees left around target
   - ``rotate_right_45`` - Rotate camera 45 degrees right around target
   - ``rotate_right_30`` - Rotate camera 30 degrees right around target
   - ``rotate_up_45`` - Rotate camera 45 degrees upward
   - ``rotate_up_30`` - Rotate camera 30 degrees upward
   - ``rotate_360`` - Full 360 degree rotation around target
   - ``zoom_in`` - Gradually move camera closer to target
   - ``zoom_out`` - Gradually move camera away from target
   - ``random_1`` - Random trajectory variation 1
   - ``random_2`` - Random trajectory variation 2
   - ``random_3`` - Random trajectory variation 3
   - ``random_4`` - Random trajectory variation 4
   - ``render_only`` - Static camera, no movement
   - ``render_only_5s`` - Static camera, 5 second duration

**Example:**

   .. code-block:: blueprint

      URecordingBPLib::StartTrajectoryRecording(
          1,                              // Camera ID
          "scene_001_traj_rotate_left_45", // FileName
          "rotate_left_45",               // TrajectoryType
          ForegroundActor,                 // Target
          30,                             // FPS
          36.0f,                          // DegreesPerSecond
          12345                           // RandomSeed
      );

GetSupportedTrajectoryTypes
~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

   UFUNCTION(BlueprintPure, Category = "UnrealCV|Recording|Trajectory")
   static TArray<FString> GetSupportedTrajectoryTypes();

Returns all supported trajectory type strings.

**Returns:** Array of trajectory type identifiers

Simple Recording Functions
---------------------------

StartSimpleRecording
~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

   UFUNCTION(BlueprintCallable, Category = "UnrealCV|Recording|Simple")
   static bool StartSimpleRecording(
      int32 CameraID,
      const FString& FileName,
      int32 FPS,
      float DurationSeconds
   );

   static bool StartSimpleRecording(
      const FString& IDString,
      const FString& FileName,
      int32 FPS,
      float DurationSeconds,
      bool bRecordLit = true,
      bool bRecordMask = false,
      bool bRecordNormal = false,
      bool bRecordDepth = false,
      bool bRecordFlow = false
   );

Starts simple recording without camera movement.

**Parameters:**

   - ``CameraID`` / ``IDString``: Camera identifier (integer or CID format)
   - ``FileName`` (FString): Output filename
   - ``FPS`` (int32): Frames per second
   - ``DurationSeconds`` (float): Recording duration in seconds
   - ``bRecordLit`` (bool): Record RGB/color data (default: true)
   - ``bRecordMask`` (bool): Record segmentation mask (default: false)
   - ``bRecordNormal`` (bool): Record normal map (default: false)
   - ``bRecordDepth`` (bool): Record depth data (default: false)
   - ``bRecordFlow`` (bool): Record optical flow (default: false)

**Returns:** ``true`` if recording started successfully

Time Dilation Functions
----------------------

GetTimeDilation
~~~~~~~~~~~~~~~~

.. code-block:: cpp

   static float GetTimeDilation();

Gets the current global time dilation value.

**Returns:** Current time dilation (1.0 = normal speed)

SetTimeDilation
~~~~~~~~~~~~~~~

.. code-block:: cpp

   static void SetTimeDilation(float Value);

Sets the global time dilation for slow-motion effects (bullet-time).

**Parameters:**

   - ``Value`` (float): Time dilation value (e.g., 0.1 for 10x slow motion)

Internal Helper Functions
--------------------------

PrepareRecording
~~~~~~~~~~~~~~~

.. code-block:: cpp

   static AFusionCamCaptureActor* PrepareRecording(int32 CameraID);

Internal function to prepare recording infrastructure.

**Parameters:**

   - ``CameraID`` (int32): Camera identifier

**Returns:** Pointer to prepared capture actor

GetCaptureActor
~~~~~~~~~~~~~~~

.. code-block:: cpp

   static AFusionCamCaptureActor* GetCaptureActor(FString CID);

Gets the capture actor for a camera.

**Parameters:**

   - ``CID`` (FString): Camera CID string

**Returns:** Pointer to the capture actor, or nullptr

ParseTrajectoryType
~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

   static bool ParseTrajectoryType(const FString& TrajectoryTypeStr, ECameraTrajectoryType& OutTrajectoryType);

Parses trajectory type string to enum value.

**Parameters:**

   - ``TrajectoryTypeStr`` (FString): Trajectory type identifier
   - ``OutTrajectoryType`` (ECameraTrajectoryType&): Output enum value

**Returns:** ``true`` if parsing successful

See Also
--------

- :doc:`recording-pipeline` - Recording system architecture
- :doc:`camera-commands-new` - TCP command reference for camera control
- :doc:`dataset-generation` - Complete dataset generation workflow
