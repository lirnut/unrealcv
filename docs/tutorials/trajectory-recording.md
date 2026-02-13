Camera Trajectory Recording Tutorial
======================================

This guide covers camera trajectory recording in UnrealCV, including
all 15 trajectory types and their use cases.

Overview
--------

Trajectory recording moves the camera automatically during capture,
creating dynamic video sequences. This is essential for dataset diversity
in computer vision tasks.

**Key Components:**

- ``URecordingBPLib`` - Blueprint library for trajectory control
- ``AFusionCamCaptureActor`` - Recording actor that executes trajectories
- ``ECameraTrajectoryType`` - Enum defining 15 trajectory patterns

Recording Types
---------------

+--------------------+---------------------------------------------------+
| Type               | Description                                       |
+--------------------+---------------------------------------------------+
| Simple Recording   | Static camera, fixed duration                     |
| Trajectory Record  | Camera moves along predefined path                |
| Bullet-Time        | Slow-motion with camera rotation (SOW requirement)|
+--------------------+---------------------------------------------------+

Trajectory Types (15 Total)
---------------------------

### Fixed Rotation Trajectories

**rotate_left_45**
  Rotate camera 45 degrees counter-clockwise around target.
  Use for: Mild perspective changes, object side views.

**rotate_left_30**
  Rotate camera 30 degrees counter-clockwise.
  Use for: Subtle angle adjustments, smooth transitions.

**rotate_right_45**
  Rotate camera 45 degrees clockwise.
  Use for: Symmetric views to rotate_left_45.

**rotate_right_30**
  Rotate camera 30 degrees clockwise.
  Use for: Subtle angle adjustments in opposite direction.

**rotate_up_45**
  Pitch camera upward 45 degrees.
  Use for: Top-down object views, ceiling exploration.

**rotate_up_30**
  Pitch camera upward 30 degrees.
  Use for: Mild elevation changes.

**rotate_360**
  Full 360-degree orbit around target.
  Use for: Complete object views, omnidirectional datasets.

### Zoom Trajectories

**zoom_in**
  Smooth dolly-forward toward target.
  Use for: Object detail capture, scale variation.

**zoom_out**
  Smooth dolly-backward away from target.
  Use for: Context capture, scene overview.

### Random Direction Trajectories

**random_1**
  Randomized trajectory with seed 1.
  Use for: Unpredictable camera movement, diversity.

**random_2**
  Randomized trajectory with seed 2.
  Use for: Alternative random pattern.

**random_3**
  Randomized trajectory with seed 3.
  Use for: Alternative random pattern.

**random_4**
  Randomized trajectory with seed 4.
  Use for: Alternative random pattern.

### Render-Only Trajectories

**render_only**
  Capture frames without camera movement.
  Use for: Static scene capture, baseline comparison.

**render_only_5s**
  Capture 5 seconds without camera movement.
  Use for: Short static clips, testing.

API Reference
-------------

**StartTrajectoryRecording**

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

**Parameters:**

- ``CameraID`` - Target camera index
- ``FileName`` - Output file path
- ``TrajectoryType`` - String name (e.g., "rotate_360", "zoom_in")
- ``Target`` - Actor to orbit/track (nullptr = current position)
- ``FPS`` - Frames per second (default 30)
- ``DegreesPerSecond`` - Rotation speed (default 36)
- ``RandomSeed`` - For random trajectories (-1 = random)

**GetSupportedTrajectoryTypes**

.. code-block:: cpp

   UFUNCTION(BlueprintPure, Category = "UnrealCV|Recording|Trajectory")
   static TArray<FString> GetSupportedTrajectoryTypes();

Returns array of all valid trajectory type strings.

Usage Examples
-------------

**Basic Orbit Recording**

.. code-block:: cpp

   // Start 360-degree orbit around actor
   bool bSuccess = URecordingBPLib::StartTrajectoryRecording(
       0,                                      // Camera ID
       TEXT("orbit_sequence"),                  // FileName
       TEXT("rotate_360"),                     // TrajectoryType
       TargetActor,                            // Target to orbit
       30,                                     // FPS
       45.0f,                                  // Degrees per second
       -1                                      // Random seed
   );

**Zoom Sequence**

.. code-block:: cpp

   // Zoom in toward object
   URecordingBPLib::StartTrajectoryRecording(
       CameraID,
       TEXT("zoom_sequence"),
       TEXT("zoom_in"),
       ObjectActor,
       30,
       20.0f
   );

**Random Movement (Dataset Diversity)**

.. code-block:: cpp

   // Use different random seeds for variety
   for (int32 Seed = 1; Seed <= 4; Seed++)
   {
       FString TrajectoryName = FString::Printf(TEXT("random_%d"), Seed);
       URecordingBPLib::StartTrajectoryRecording(
           CameraID,
           FString::Printf(TEXT("random_seq_%d"), Seed),
           TrajectoryName,
           TargetActor,
           30,
           30.0f,
           Seed
       );
   }

**Bullet-Time Effect**

For SOW requirements (slow-motion with rotation), combine trajectories
with time dilation:

.. code-block:: cpp

   // Slow motion bullet-time
   URecordingBPLib::SetTimeDilation(0.1f);  // 10% speed

   URecordingBPLib::StartTrajectoryRecording(
       CameraID,
       TEXT("bullet_time"),
       TEXT("rotate_360"),
       TargetActor,
       120,  // Higher FPS for slow-mo
       90.0f
   );

   URecordingBPLib::SetTimeDilation(1.0f);  // Restore

TCP Command Usage
-----------------

Trajectories can also be triggered via TCP:

.. code-block:: bash

   vbp /camera/0/starttrajectoryrecording filename=test.mp4 trajectory=rotate_360 target=MyActor fps=30

Or via vset:

.. code-block:: bash

   vset /camera/0/starttrajectory type=rotate_360 target=MyActor fps=30

Trajectory File Format
---------------------

Custom trajectories can be loaded from JSON files:

.. code-block:: json

   [
     [
       {"x": 0.0, "y": 0.0, "z": 136.335},
       {"yaw": 297.717, "roll": 0.0, "pitch": 341.382}
     ],
     [
       {"x": 69.022, "y": -11.622, "z": 126.405},
       {"yaw": 72.858, "roll": 0.0, "pitch": 345.077}
     ]
   ]

Each entry is a keyframe with position (x, y, z) and rotation
(yaw, roll, pitch). The camera interpolates between keyframes.

SOW Camera Requirements
-----------------------

Per the SOW specification, capture from 6 fixed positions + 4 random
directions per scene:

**Fixed Directions:**

1. rotate_left_45 - Left perspective
2. rotate_right_45 - Right perspective
3. rotate_up_45 - Top-down view
4. rotate_360 - Full orbit
5. zoom_in - Close detail
6. zoom_out - Wide context

**Random Directions:**

Use random_1 through random_4 for additional diversity.

Best Practices
--------------

1. **Start Small**: Test with render_only before expensive trajectories
2. **Frame Count**: Use ``FPS * Duration = TotalFrames``
3. **Smoothness**: Lower DegreesPerSecond for smoother motion
4. **Target Selection**: Use a central actor as orbit target
5. **Seed Control**: Fix random seeds for reproducibility

**Recommended Settings for SOW:**

.. code-block:: cpp

   // Standard capture
   StartTrajectoryRecording(CameraID, Output, "rotate_360", Target, 30, 36);

   // Slow motion bullet-time
   SetTimeDilation(0.1f);
   StartTrajectoryRecording(CameraID, Output, "rotate_360", Target, 120, 360);
   SetTimeDilation(1.0f);

Troubleshooting
--------------

**Camera not moving:**
- Check IsRecording() returns true
- Verify Target actor exists
- Check DegreesPerSecond > 0

**Jittery motion:**
- Increase FPS
- Decrease DegreesPerSecond
- Use smoother trajectories (zoom vs rotate)

**No output file:**
- Verify file path is writable
- Check disk space
- Ensure recording completed (not interrupted)
