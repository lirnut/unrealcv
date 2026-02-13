Recording Metadata JSON Schema Reference
====================================

Overview
--------

This document describes the JSON metadata files generated during recording sessions.
UnrealCV produces two types of metadata files:

1. **overview.json** - Scene-level metadata (one per recording session)
2. **camera.json** - Per-frame camera and object data (generated each frame)

These files are essential for downstream processing in computer vision pipelines,
especially for applications like novel view synthesis, segmentation, and tracking.

Output Files Reference
---------------------

For each recording session, the following files are generated:

.. code::

   scene_001/
   ├── traj_rotate_left_45/
   │   ├── lit_0001.png          # RGB image
   │   ├── lit_0002.png
   │   ├── ...
   │   ├── mask_0001.png        # Segmentation mask
   │   ├── depth_0001.png       # Depth map
   │   ├── normal_0001.png      # Surface normals
   │   ├── flow_0001.png        # Optical flow
   │   ├── camera.json          # Camera pose per frame
   │   ├── overview.json        # Scene metadata (this doc)
   │   └── audio.wav            # Audio track
   └── ...

overview.json Schema
------------------

The overview.json file contains scene-level metadata for the entire recording session.

**File Location:** ``{OutputFolder}/{TrajectoryName}/overview.json``

**Schema:**

.. code-block:: json

   {
      "VideoName": "scene_001_traj_rotate_left_45",
      "Resolution": "854x480",
      "Width": 854,
      "Height": 480,
      "RecordedFPS": 30,
      "FOV": 90.0,
      "SceneCategory": "Scene_001",
      "ForegroundCategory": "Foreground_Human",
      "ForegroundSubcategory": "Adult_Male",
      "ForegroundObjectMetadata": {
         "asset_path": "/Game/Assets/Human_01",
         "animation": "Walk_Cycle"
      },
      "OccluderMetaDataList": [
         {
            "occluder_name": "chair_001",
            "category": "Occluder_Small",
            "asset_path": "/Game/Assets/Chair_01"
         },
         {
            "occluder_name": "table_001",
            "category": "Occluder_Medium",
            "asset_path": "/Game/Assets/Table_01"
         }
      ],
      "CameraSettings": {
         "ProjectionType": "Perspective",
         "ExposureMethod": "Auto",
         "MotionBlur": false
      },
      "CameraSettingsString": "Projection=Perspective|Exposure=Auto",
      "ForegroundColor": "1,2,4",
      "AnnotationColors": {
         "Human_01": "1,2,4",
         "Chair_01": "1,4,8",
         "Table_01": "1,8,16"
      },
      "RealWorldTimeRecordingStart": "2025-01-15 10:30:00"
   }

Field Descriptions
~~~~~~~~~~~~~~~~

**Recording Info:**

+-----------------------+--------+----------------------------------+
| Field                 | Type   | Description                      |
+=======================+========+==================================+
| VideoName             | string | Recording session identifier      |
+-----------------------+--------+----------------------------------+
| Resolution            | string | Width x Height (e.g., "854x480")|
+-----------------------+--------+----------------------------------+
| Width                 | int    | Image width in pixels            |
+-----------------------+--------+----------------------------------+
| Height                | int    | Image height in pixels          |
+-----------------------+--------+----------------------------------+
| RecordedFPS           | int    | Frames per second                |
+-----------------------+--------+----------------------------------+
| FOV                   | float  | Field of view in degrees        |
+-----------------------+--------+----------------------------------+

**Scene Composition:**

+-----------------------+--------+----------------------------------+
| Field                 | Type   | Description                      |
+=======================+========+==================================+
| SceneCategory         | string | Scene identifier                 |
+-----------------------+--------+----------------------------------+
| ForegroundCategory    | string | Foreground asset category        |
+-----------------------+--------+----------------------------------+
| ForegroundSubcategory | string | Specific foreground type         |
+-----------------------+--------+----------------------------------+
| ForegroundObjectMetadata | object | Foreground asset details     |
+-----------------------+--------+----------------------------------+
| OccluderMetaDataList  | array  | Array of occluder metadata      |
+-----------------------+--------+----------------------------------+

**Camera Settings:**

+-----------------------+--------+----------------------------------+
| Field                 | Type   | Description                      |
+=======================+========+==================================+
| CameraSettings        | object | Camera configuration object      |
+-----------------------+--------+----------------------------------+
| CameraSettingsString  | string | Pipe-separated settings          |
+-----------------------+--------+----------------------------------+

**Annotation:**

+-----------------------+--------+----------------------------------+
| Field                 | Type   | Description                      |
+=======================+========+==================================+
| ForegroundColor       | string | RGB values (e.g., "1,2,4")      |
+-----------------------+--------+----------------------------------+
| AnnotationColors      | object | Map of actor name to RGB color  |
+-----------------------+--------+----------------------------------+

**Timing:**

+----------------------------------+--------+-----------------------------+
| Field                            | Type   | Description                 |
+==================================+========+=============================+
| RealWorldTimeRecordingStart      | string | Recording start timestamp   |
+----------------------------------+--------+-----------------------------+

camera.json Schema
-----------------

The camera.json file contains per-frame camera and object data.

**File Location:** ``{OutputFolder}/{TrajectoryName}/camera.json``

**Schema:**

.. code-block:: json

   {
      "FrameNumber": 121,
      "CameraLocation": {"x": 100.0, "y": 200.0, "z": 167.5},
      "CameraRotation": {"pitch": 0, "yaw": 45, "roll": 0},
      "ForegroundLocation": {"x": 0.0, "y": 0.0, "z": 0.0},
      "ForegroundRotation": {"pitch": 0, "yaw": 0, "roll": 0},
      "IntrinsicsMatrix": [
         [854.0, 0.0, 427.0],
         [0.0, 854.0, 240.0],
         [0.0, 0.0, 1.0]
      ],
      "Extrinsics": {
         "RotationMatrix": [
            [0.707, 0.0, 0.707],
            [0.0, 1.0, 0.0],
            [-0.707, 0.0, 0.707]
         ],
         "Translation": [100.0, 200.0, 167.5],
         "w2c_colmap": [
            [0.707, 0.0, -0.707, 0.1],
            [0.0, 1.0, 0.0, 0.2],
            [0.707, 0.0, 0.707, 0.167],
            [0.0, 0.0, 0.0, 1.0]
         ]
      },
      "RealWorldTimeRecordingEnd": "2025-01-15 10:30:04",
      "RealWorldTimeDurationSeconds": 4.033,
      "RealWorldTimeFPS": 30.0
   }

Field Descriptions
~~~~~~~~~~~~~~~~

**Frame Data:**

+-----------------------+--------+----------------------------------+
| Field                 | Type   | Description                      |
+=======================+========+==================================+
| FrameNumber           | int    | Total number of frames captured  |
+-----------------------+--------+----------------------------------+

**Camera Transform (Unreal coordinates):**

+-----------------------+--------+----------------------------------+
| Field                 | Type   | Description                      |
+=======================+========+==================================+
| CameraLocation        | object | World position (x, y, z)        |
+-----------------------+--------+----------------------------------+
| CameraRotation        | object | Pitch, Yaw, Roll in degrees     |
+-----------------------+--------+----------------------------------+

**Foreground Transform:**

+-----------------------+--------+----------------------------------+
| Field                 | Type   | Description                      |
+=======================+========+==================================+
| ForegroundLocation    | object | Foreground actor position        |
+-----------------------+--------+----------------------------------+
| ForegroundRotation    | object | Foreground actor rotation        |
+-----------------------+--------+----------------------------------+

**Camera Intrinsics:**

The intrinsics matrix K is a 3x3 matrix:

.. code-block::

   K = [fx,  0, cx]
       [ 0, fy, cy]
       [ 0,  0,  1]

Where:
- ``fx = Width / (2 * tan(FOV/2))``
- ``fy = Height / (2 * tan(FOV/2)) * AspectRatio``
- ``cx = Width / 2`` (principal point x)
- ``cy = Height / 2`` (principal point y)

**Extrinsics:**

+-----------------------+--------+----------------------------------+
| Field                 | Type   | Description                      |
+=======================+========+==================================+
| RotationMatrix        | array  | 3x3 rotation matrix             |
+-----------------------+--------+----------------------------------+
| Translation           | array  | Translation vector [x, y, z]    |
+-----------------------+--------+----------------------------------+
| w2c_colmap           | array  | 4x4 Colmap-compatible transform |
+-----------------------+--------+----------------------------------+

**Coordinate System Transforms:**

The extrinsics are provided in multiple formats:

1. **Unreal RotationMatrix**: 3x3 rotation matrix in Unreal coordinates
2. **Unreal Translation**: Translation vector in Unreal units (cm)
3. **w2c_colmap**: 4x4 world-to-camera matrix in Colmap coordinate system

**Colmap Coordinate Transform:**

.. code-block::

   Unreal: X=right, Y=forward, Z=up
   Colmap: X=right, Y=down, Z=forward

   w2c_colmap = T_cam_unreal_to_colmap * w2c_unreal * T_world_colmap_to_unreal

**Timing:**

+----------------------------------+--------+-----------------------------+
| Field                            | Type   | Description                 |
+==================================+========+=============================+
| RealWorldTimeRecordingEnd        | string | Recording end timestamp     |
+----------------------------------+--------+-----------------------------+
| RealWorldTimeDurationSeconds     | float  | Actual wall-clock duration  |
+----------------------------------+--------+-----------------------------+
| RealWorldTimeFPS                 | float  | Actual capture FPS          |
+----------------------------------+--------+-----------------------------+

Python Usage Examples
-------------------

**Loading Metadata:**

.. code-block:: python

   import json

   with open('overview.json', 'r') as f:
       overview = json.load(f)

   with open('camera.json', 'r') as f:
       camera = json.load(f)

**Using Intrinsics for 3D Projection:**

.. code-block:: python

   import numpy as np

   # Load intrinsics
   with open('camera.json', 'r') as f:
       camera = json.load(f)

   K = np.array(camera['IntrinsicsMatrix'])

   # Project 3D point to 2D
   def project_point(point_3d, K, extrinsics):
       # point_3d: [x, y, z] in world coordinates
       # Convert to camera coordinates
       R = np.array(extrinsics['RotationMatrix'])
       t = np.array(extrinsics['Translation'])

       # World to camera transform
       point_cam = R @ np.array(point_3d) + t

       # Camera to image
       point_img = K @ point_cam
       point_img = point_img[:2] / point_img[2]

       return point_img

**Loading Annotation Colors:**

.. code-block:: python

   import json

   with open('overview.json', 'r') as f:
       overview = json.load(f)

   # Create color lookup
   color_map = {}
   for actor_name, color_str in overview['AnnotationColors'].items():
       r, g, b = map(int, color_str.split(','))
       color_map[actor_name] = [r, g, b]

   print(f"Foreground color: {overview['ForegroundColor']}")
   print(f"Total annotated objects: {len(color_map)}")

**Parsing Camera Trajectory:**

.. code-block:: python

   import json

   with open('camera.json', 'r') as f:
       camera_data = json.load(f)

   # Get camera positions (requires combining overview and camera data)
   # Note: camera.json contains per-frame data if saved per-frame

Occlusion Ratio Calculation
-------------------------

The occlusion ratio is calculated as:

.. code-block::

   OcclusionRatio = (ForegroundPixelsWithOccluder) / (TotalForegroundPixels)

Where:
- ``ForegroundPixelsWithOccluder``: Pixels belonging to foreground but occluded
- ``TotalForegroundPixels``: All pixels belonging to foreground

This value is stored in the overview metadata for quality filtering.

Quality Metrics
--------------

The following metrics can be derived from metadata:

**Temporal Consistency:**
- Check FrameNumber matches expected (e.g., 121 frames at 30fps = ~4s clip)
- Verify RealWorldTimeFPS is close to RecordedFPS

**Coverage:**
- Verify OccluderMetaDataList has expected count
- Check AnnotationColors covers all objects

**Completeness:**
- Verify Resolution matches actual image dimensions
- Confirm Intrinsics matrix matches FOV

See Also
--------

- :doc:`../architecture/recording-pipeline` - Recording system architecture
- :doc:`../reference/sensor-data-formats` - Sensor data format reference
- :doc:`../tutorials/dataset-generation` - Dataset generation workflow
