Recording System Architecture
==========================

Overview
--------

The Recording System is a multi-component architecture that enables high-throughput
capture of synchronized multi-modal data from FusionCamSensors. The system supports
9 simultaneous data types with async GPU readback for optimal performance.

**Key Components:**

- ``AFusionCamCaptureActor`` - Recording lifecycle management
- ``UFusionCamSensor`` - Multi-pass sensor orchestration
- ``FUnrealCVMP4Encoder`` - Video encoding
- ``AsyncCaptureHelper`` - Non-blocking GPU readback

Architecture Diagram
-------------------

.. code-block::

   +---------------------------+
   | RecordingBPLib API        |
   | (Blueprint Interface)     |
   +------------+--------------+
                |
                v
   +---------------------------+
   | AFusionCamCaptureActor    |
   | - Lifecycle Management    |
   | - Trajectory Control      |
   | - Data Type Selection     |
   +------------+--------------+
                |
                v
   +---------------------------+
   |   UFusionCamSensor        |
   |   (5-Pass Orchestrator)   |
   +---------------------------+
   | LitCamSensor   | RGB      |
   | DepthCamSensor | Depth    |
   | AnnotCamSensor | SegMask  |
   | NormalCamSensor| Normals  |
   | FlowCamSensor  | Flow     |
   +---------------------------+
                |
                v
   +---------------------------+
   | AsyncCaptureHelper        |
   | (3-Phase GPU Readback)    |
   +---------------------------+
                |
                v
   +---------------------------+
   | FUnrealCVMP4Encoder       |
   | (H.264 Encoding)         |
   +---------------------------+

AFusionCamCaptureActor
---------------------

``AFusionCamCaptureActor`` manages the complete recording lifecycle. It is created when
recording starts and automatically destroyed when recording stops.

**Header:** ``Source/UnrealCV/Public/Actor/FusionCamCaptureActor.h``

Lifecycle
~~~~~~~~

1. **Creation** - Created by CameraHandler or RecordingBPLib when recording starts
2. **Configuration** - Data types selected via properties
3. **Recording** - Captures frames from target sensor
4. **Cleanup** - Actor destroyed when recording completes

Key Properties
~~~~~~~~~~~~~

**Sensor Configuration:**

.. code-block:: cpp

   UPROPERTY(EditInstanceOnly, Category = "FusionCamCapture")
   class UFusionCamSensor* TargetSensor;

   UPROPERTY(EditInstanceOnly, Category = "FusionCamCapture")
   bool bUseMovieQualityRendering;

**Output Configuration:**

.. code-block:: cpp

   UPROPERTY(EditInstanceOnly, Category = "FusionCamCapture")
   FDirectoryPath DataFolder;

   UPROPERTY(EditInstanceOnly, Category = "FusionCamCapture")
   bool bAddTimestamp;

**Data Types (9 Simultaneous):**

.. code-block:: cpp

   UPROPERTY(EditInstanceOnly, Category = "FusionCamCapture| Data Types")
   bool bRecordRGB;              // Standard RGB image

   UPROPERTY(EditInstanceOnly, Category = "FusionCamCapture| Data Types")
   bool bRecordMask;             // Instance segmentation mask

   UPROPERTY(EditInstanceOnly, Category = "FusionCamCapture| Data Types")
   bool bRecordDepth;            // Depth map

   UPROPERTY(EditInstanceOnly, Category = "FusionCamCapture| Data Types")
   bool bRecordNormal;          // Surface normal map

   UPROPERTY(EditInstanceOnly, Category = "FusionCamCapture| Data Types")
   bool bRecordFlow;             // Optical flow

   UPROPERTY(EditInstanceOnly, Category = "FusionCamCapture| Data Types")
   bool bRecordOneObjectMask;   // Single object mask

   UPROPERTY(EditInstanceOnly, Category = "FusionCamCapture| Data Types")
   bool bRecordOneObjectLit;     // Single object lit

   UPROPERTY(EditInstanceOnly, Category = "FusionCamCapture| Data Types")
   bool bRecordShadowCatcher;    // Shadow catcher composite

   UPROPERTY(EditInstanceOnly, Category = "FusionCamCapture| Data Types")
   bool bRecordStencilMask;      // Stencil buffer mask

   UPROPERTY(EditInstanceOnly, Category = "FusionCamCapture| Data Types")
   bool bRecordMetadata;         // Camera pose metadata

   UPROPERTY(EditInstanceOnly, Category = "FusionCamCapture| Data Types")
   bool bRecordAudio;            // Audio capture

Recording Control:

.. code-block:: cpp

   UPROPERTY(EditInstanceOnly, Category = "FusionCamCapture| Recording")
   int32 WarmUpFrames;           // Warm-up frames before recording

   UPROPERTY(EditInstanceOnly, Category = "FusionCamCapture| Bullet Time")
   float BulletTimeSpeedDeg;     // Slow motion rotation speed

   UPROPERTY(EditInstanceOnly, Category = "FusionCamCapture| Data Types")
   float TimeDilation;          // Global time dilation

   UPROPERTY(EditInstanceOnly, Category = "FusionCamCapture| Video Generation")
   bool bAutoGenerateVideo;      // Generate MP4 after capture

Trajectory Recording
~~~~~~~~~~~~~~~~~~~~

The actor supports 15 trajectory types for camera movement:

.. code-block:: cpp

   UENUM(BlueprintType)
   enum class ECameraTrajectoryType : uint8
   {
      RotateLeft45,
      RotateLeft30,
      RotateRight45,
      RotateRight30,
      RotateUp45,
      RotateUp30,
      Rotate360,
      ZoomIn,
      ZoomOut,
      RandomDirection1,
      RandomDirection2,
      RandomDirection3,
      RandomDirection4,
      RenderOnly,        // Static camera
      RenderOnly5S        // Static 5-second clip
   };

Key Methods
~~~~~~~~~~

.. code-block:: cpp

   void StartTrajectoryRecord(
      const FString& FileName,
      ECameraTrajectoryType TrajectoryType,
      AActor* Target,
      int32 FPS = 30,
      int32 InNumFrames = 121,
      int32 RandomSeed = -1,
      bool bPauseWorldTime = false
   );

   void StartSimpleRecording(
      const FString& FileName,
      int32 FPS,
      float DurationSeconds
   );

   void StopRecord();

   bool IsRecording() const;

   void SetSceneHandle(const FSceneHandle& InSceneHandle);

UFusionCamSensor
---------------

``UFusionCamSensor`` orchestrates 5 specialized sensors in a single render pass.

**Header:** ``Source/UnrealCV/Public/Sensor/CameraSensor/FusionCamSensor.h``

Sensor Types
~~~~~~~~~~~~

1. **LitCamSensor** - Standard RGB rendering
2. **DepthCamSensor** - Distance from camera plane
3. **AnnotCamSensor** - Instance segmentation coloring
4. **NormalCamSensor** - Surface normal vectors
5. **FlowCamSensor** - Optical flow vectors

Each sensor can be independently enabled/disabled.

Async GPU Readback Pipeline
---------------------------

The system uses a 3-phase async pipeline for maximum throughput:

.. code-block::

   Frame N                    Frame N+1                  Frame N+2
   +----------+               +----------+               +----------+
   | Game     |               | Game     |               | Game     |
   | Thread   |               | Thread   |               | Thread   |
   +----+-----+               +----+-----+               +----+-----+
        |                          |                          |
        v                          v                          v
   +----+-----+               +----+-----+               +----+-----+
   | Render  |               | Render  |               | Render  |
   | Thread  |               | Thread  |               | Thread  |
   | GPU     |               | GPU     |               | GPU     |
   | Readback|               | Readback|               | Readback|
   +----+-----+               +----+-----+               +----+-----+
        |                          |                          |
        v                          v                          v
   +----+-----+               +----+-----+               +----+-----+
   | Async   |               | Async   |               | Async   |
   | File I/O|               | File I/O|               | File I/O|
   +----+-----+               +----+-----+               +----+-----+
        |                          |                          |
   Frame N data               Frame N-1 data           Frame N-2 data
   written to disk            written to disk          written to disk

**Phase 1: Game Thread**
- Validate recording state
- Enqueue render command

**Phase 2: Render Thread**
- GPU readback via ``FRHIGPUTextureReadback``

**Phase 3: Game Thread**
- PNG encoding
- Async file write (non-blocking)

Benefits:
- Non-blocking pipeline
- Parallel operations
- 30-50% performance improvement vs sync capture

Output Format
------------

Layered Video Output (5 files per video):

1. **Composite** - Full rendered video + audio
2. **Foreground Mask** - Instance segmentation mask
3. **Background Layer** - Scene without foreground
4. **Foreground Layer** - Isolated foreground with alpha
5. **Metadata JSON** - Object IDs, resolution, frames, categories

Metadata JSON Schema
~~~~~~~~~~~~~~~~~~~~

.. code-block:: json

   {
      "object_id": "foreground_001",
      "resolution": {"width": 854, "height": 480},
      "frames": 121,
      "categories": {
         "foreground": "Human",
         "occluders": ["Chair", "Table"]
      },
      "occlusion_ratio": 0.35,
      "camera": {
         "trajectory_type": "rotate_left_45",
         "fps": 30
      }
   }

CameraRecordingActors Map
------------------------

The CameraHandler maintains a tracking map:

.. code-block:: cpp

   TMap<int32, AFusionCamCaptureActor*> CameraRecordingActors;

This map tracks active recording actors by camera ID.

Video Generation
---------------

Post-recording video generation via genvid.py:

.. code-block:: cpp

   UPROPERTY(EditInstanceOnly, Category = "FusionCamCapture| Video Generation")
   bool bAutoGenerateVideo;

   UPROPERTY(EditInstanceOnly, Category = "FusionCamCapture| Video Generation")
   FString VideoGenScriptPath;

   UPROPERTY(EditInstanceOnly, Category = "FusionCamCapture| Video Generation")
   FString CondaEnvName;

See Also
--------

- :doc:`../api/recording-bplib` - RecordingBPLib API
- :doc:`../api/scene-composition-bplib` - Scene composition API
- :doc:`../reference/camera-commands-new` - Camera command reference
- :doc:`dataset-generation` - Dataset generation workflow
