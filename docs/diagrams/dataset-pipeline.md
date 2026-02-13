Dataset Generation Pipeline
=========================

End-to-end workflow from asset pool to layered video output.

Pipeline Stages
--------------

::

   ┌─────────────────────────────────────────────────────────────────────────────┐
   │                         Dataset Generation Pipeline                          │
   └─────────────────────────────────────────────────────────────────────────────┘

   ┌──────────┐     ┌──────────────┐     ┌──────────────────┐     ┌─────────┐
   │ Asset    │────▶│ Scene        │────▶│ Recording        │────▶│ Output  │
   │ Pool     │     │ Composition  │     │ & Capture        │     │ Files   │
   └──────────┘     └──────────────┘     └──────────────────┘     └─────────┘
        │                  │                      │                     │
        ▼                  ▼                      ▼                     ▼
   ┌──────────┐     ┌──────────────┐     ┌──────────────────┐     ┌─────────┐
   │ Pak Files │     │ Spawn        │     │ FusionCamSensor  │     │ 5-Layer │
   │ Dynamic   │     │ Foreground   │     │ Multi-pass       │     │ Video   │
   │ Loading   │     │ Occluders    │     │ Rendering        │     │ + JSON  │
   └──────────┘     └──────────────┘     └──────────────────┘     └─────────┘

Component Flow
--------------

::

   Asset Pool Manager                    Recording System
   ┌─────────────────┐                   ┌─────────────────────────┐
   │ Categories:     │                   │                         │
   │ - foreground    │                   │ ┌─────────────────────┐ │
   │ - occluder      │──▶ Spawn ───────▶│ │ FusionCamSensor     │ │
   │ - scene         │                   │ │ - Lit (RGB)         │ │
   │ - prop          │                   │ │ - Depth             │ │
   │ - character     │                   │ │ - Normal            │ │
   └─────────────────┘                   │ │ - Flow              │ │
                                         │ │ - Mask (Seg)        │ │
                                         │ └─────────────────────┘ │
                                         │           │               │
   Dataset Automation                     │           ▼               │
   ┌─────────────────┐                   │ ┌─────────────────────┐ │
   │ Task Queue      │                   │ │ FusionCamCaptureActor│ │
   │ - Config        │──▶ Execute ──────▶│ │ - 3-phase async     │ │
   │ - Commands      │                   │ │ - File encoding     │ │
   │ - Status        │                   │ │ - Metadata JSON     │ │
   └─────────────────┘                   │ └─────────────────────┘ │
                                         └─────────────────────────┘

Scene Generation Flow
--------------------

::

   ┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
   │ Spawn Random    │     │ Configure       │     │ Random          │
   │ Foreground      │────▶│ Annotations     │────▶│ Navigation      │
   └─────────────────┘     └─────────────────┘     └─────────────────┘
           │                        │                      │
           ▼                        ▼                      ▼
   ┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
   │ Spawn Random    │     │ Assign Unique   │     │ Move Agent to    │
   │ Occluder        │     │ Color/ID       │     │ Safe Point      │
   └─────────────────┘     └─────────────────┘     └─────────────────┘

Recording Flow
--------------

::

   ┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
   │ Prepare Sensor  │     │ Execute         │     │ Async           │
   │ Configuration   │────▶│ Trajectory      │────▶│ Readback        │
   └─────────────────┘     └─────────────────┘     └─────────────────┘
           │                                               │
           ▼                                               ▼
   ┌─────────────────┐                         ┌─────────────────┐
   │ Enable Layers:  │                         │ 3-Phase Pipeline│
   │ - Lit           │                         │ 1. Render       │
   │ - Depth         │                         │ 2. GPU Readback │
   │ - Normal        │                         │ 3. File Write  │
   │ - Flow          │                         └─────────────────┘
   │ - Mask          │
   └─────────────────┘

Output Structure
----------------

::

   output/
   ├── video_001/
   │   ├── overview.mp4           # Composite + audio
   │   ├── foreground_mask.mp4     # Instance segmentation
   │   ├── background_layer.mp4    # Inpainted background
   │   ├── foreground_layer.mp4    # Complete foreground
   │   └── metadata.json           # Object IDs, occlusion, etc.
   ├── video_002/
   └── ...
