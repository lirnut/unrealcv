# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

UnrealCV is an Unreal Engine (5.2+) plugin for computer vision research that provides:
- TCP server for external program communication
- Camera and object manipulation commands
- Blueprint function libraries for easy integration
- Python client library for remote control

## Architecture

**Core Components:**
- `FUnrealcvServer` (Source/UnrealCV/Public/Server/UnrealcvServer.h:34) - Main server class implementing FTickableGameObject
- TCP Server (UnixTcpServer/UUnixTcpServer) - Handles network connections
- Command Dispatcher - Routes incoming commands to appropriate handlers
- World Controller - Manages UE5 world state and objects

**Key Modules:**
- Actor: Camera sensors, data capture, puppeteer actors
- BPFunctionLib: Blueprint function libraries (Animation, JSON, Sensor, Serialize)
- Component: UE5 components for CV functionality
- Controller: World and object controllers
- Sensor: Camera and sensor systems
- Server: TCP server and command handling
- Utils: Utility functions

**Recording System Architecture (NEW - 2025):**
```
┌─────────────────────────────────────────┐
│  Python Automation Layer                │
│  (unreal_zoo project - external)        │
│  - Scene composition                    │
│  - Batch rendering control              │
│  - Asset randomization                  │
└────────────────┬────────────────────────┘
                 │ TCP/IP
┌────────────────▼────────────────────────┐
│  UnrealCV Server (C++ Plugin)           │
│  - CameraHandler                        │
│  - ObjectHandler                        │
│  - Recording API commands               │
└────────────────┬────────────────────────┘
                 │
┌────────────────▼────────────────────────┐
│  Recording System                       │
│  ├─ AFusionCamCaptureActor              │
│  │   (automatic lifecycle management)   │
│  ├─ ACameraMotionController (TODO)      │
│  └─ AAudioLayerRecorder (Phase 2)       │
└────────────────┬────────────────────────┘
                 │
┌────────────────▼────────────────────────┐
│  Render Pipeline                        │
│  ├─ UFusionCamSensor (sensor fusion)    │
│  ├─ Actor visibility control            │
│  └─ Multi-pass rendering (TODO)         │
└─────────────────────────────────────────┘
```

**Camera ID-Based Recording API:**
- User interacts with camera IDs (not actor indices)
- `CameraHandler` automatically creates `AFusionCamCaptureActor` when recording starts
- `AFusionCamCaptureActor` automatically destroyed when recording completes
- Tracking via `TMap<int32, AFusionCamCaptureActor*> CameraRecordingActors` in CameraHandler
- Supports normal recording and bullet-time recording modes

## Build System

**Plugin Build:**
```bash
 dotnet "I:\UE_5.6\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll" HUAWEI_Project Win64 DebugGame -Project="H:\HUAWEI_Project_UE56\HUAWEI_Project.uproject" -WaitMutex -FromMsBuild -architecture=x64
```

**Manual Build:**
1. Place plugin in UE project's `Plugins` folder
2. Open project in Visual Studio (version must match UE version)
3. Compile as part of UE project build

## Testing
Test for the executable will be manually done, due to the complexity of the UE5 build system.


## Development Workflow

1. **Code Structure:** Follow existing patterns in Source/UnrealCV/Private and Public directories
2. **New Features:** Add to appropriate module (Actor, Component, Sensor, etc.)
3. **API Design:** Commands follow REST-like pattern via TCP server
4. **Testing:** Add tests to `test/` directory with `_test.py` suffix

## Key Files

- `UnrealCV.uplugin`: Plugin descriptor
- `Source/UnrealCV/UnrealCV.Build.cs`: Build configuration
- `Source/UnrealCV/Public/Server/UnrealcvServer.h`: Main server class
- `client/python/unrealcv/`: Python client library
- `test/requirements.txt`: Test dependencies


## Human Guide and TODO List

### Current Project Context (Updated 2025-10-10)

**Branch**: `shc/dev` (working branch for HUAWEI_Project dataset production)
- Supports UE 5.2 - 5.6 (not UE4)
- Optical flow feature already implemented and merged to official 5.2 branch
- Focus: Large-scale audio-video layered dataset production for video inpainting/Omnimatte research

### Recent Achievements (Completed)

1. ✅ **Recording Architecture Refactoring**
   - Created `AFusionCamCaptureActor` (FusionCamCaptureActor.h/.cpp) - dedicated recording actor
   - Implemented camera ID-based API with automatic lifecycle management
   - Removed old recording code from `UFusionCamSensor` (385 lines deleted)
   - Better OOD: separation of concerns between sensor fusion and recording
   - Commits: 1d1f8cd (implementation) + 041a2ec (cleanup)

2. ✅ **Build System**
   - All builds successful, no compilation errors
   - Build time: ~17 seconds on 16-core system

### Project Mission: SOW Requirements

**See**: `./SOW-基于CG的音视频分层数据生产-latest.md` for full details

**Goal**: Produce large-scale layered audio-video dataset for video inpainting/Omnimatte research

**Timeline**: 9 months, 3 phases
- Phase 1 (0-3 months): 40K layered videos + 10K camera movements + 10K semantic annotations
- Phase 2 (3-6 months): 80K layered audio-videos + 20K camera movements + 20K semantic annotations
- Phase 3 (6-9 months): 400K layered audio-videos + 100K camera movements + 100K semantic annotations

**Final Deliverables** (Phase 3):
- **400K layered audio-video groups** (30fps, 480p, 10s, horizontal/vertical)
- **100K camera movement videos** (121 frames, 1080p, 10 movements per scene)
- **100K semantic annotation videos** (instance, material, scale, 25K with dynamic objects)

**Each layered video group contains**:
1. Original composite video + audio
2. Foreground mask (segmentation)
3. Background layer (foreground removed + inpainted)
4. Complete foreground layer (with shadows, reflections, disturbances)
5. Foreground mask annotation
6. Metadata: video name, object ID, resolution, frames, scene/foreground categories, occluder, occlusion ratio

**Camera movements** (10 per scene):
- 6 fixed: rotate left/right/up 45°, zoom in/out, 360° rotation
- 4 random direction movements
- Include camera intrinsics & extrinsics

**Asset Requirements**:
- Humans: multiple ages, skin colors, genders, clothing, body types
- Pets: cats, dogs, birds (5+ breeds each), hamsters, turtles, lizards, snakes, spiders
- Objects: cars, buildings, food, books (20+), tableware, plants (10+)
- Scenes: 50+ types (urban, indoor, outdoor, natural, weather variations)
- Occlusions: 100 distinct relationships, 20%+ in 5-20% occlusion range

### Development Strategy

**Python Project**: `unreal_zoo` (to be placed in work directory)
- Handles scene composition, asset randomization, batch control
- Calls UnrealCV TCP server API from external Python process
- C++ plugin focuses on rendering primitives and server API

**Division of Labor**:
- **C++ Plugin (UnrealCV)**: Core rendering capabilities, server API, low-level control
- **Python Layer (unreal_zoo)**: High-level automation, scene composition, batch orchestration

### TODO List - Phase 1 (Current Focus)

#### High Priority (Immediate)

- [ ] **Multi-Layer Rendering System**
  - Design: Create `ALayeredCaptureActor` that orchestrates multiple render passes
  - For each frame, capture:
    - Composite (full scene)
    - Foreground + environmental effects (hide background)
    - Foreground mask (segmentation)
    - Background (hide foreground)
    - Background inpainted (fill occlusion - may need special shader)
  - Synchronize frame capture across all render passes
  - Control actor visibility per render pass
  - Files: Create LayeredCaptureActor.h/.cpp in Source/UnrealCV/Public/Actor/
  - Human Comment: maybe we should implement this in Python layer? in uezoo. I think we should control the scene composition in Python layer.

- [ ] **Camera Movement Controller**
  - Create `ACameraMotionController` actor
  - Implement 6 fixed trajectories:
    - RotateLeft45(), RotateRight45(), RotateUp45()
    - Rotate360() (ensure first/last frame camera match)
    - ZoomIn(), ZoomOut()
  - Implement 4 random trajectory generators
  - Export camera parameters (intrinsics/extrinsics) per frame
  - Files: Create CameraMotionController.h/.cpp
  - Human Comment: This is great, Because we can 兼容现有的API: vset /camera/[uint]/bullet_time_record
  - Human Comment: 上述时保守的做法，我觉得你是想实现相机运动控制和录制的解耦，也就是用一个Actor来控制相机运动，另一个Actor来负责录制。这是很酷的想法，或许我们也可以这样做，用不同的API来分别控制相机运动和录制，也就是下面的Camera Movement API Commands

- [ ] **Camera Movement API Commands**
  - Add commands to CameraHandler or create new MotionHandler
  - Commands:
    - `vset /camera/{id}/motion/start {type} {params}`
    - `vget /camera/{id}/motion/status`
    - `vget /camera/{id}/motion/params` (get intrinsics/extrinsics)
  - Document API in comments
  - Human Comment: This is great, 实现了相机运动控制和录制的解耦

- [ ] **Actor Visibility Control API**
  - Commands to show/hide actors or groups
  - `vset /object/{id}/visibility {true|false}`
  - `vset /object/group/{tag}/visibility {true|false}`
  - Support for render-pass-specific visibility (e.g., hide in certain capture passes)
  - Human Comment: see  ObjectHandler.cpp(vset /object/[str]/hide), shall we make a new API? is that necessary?

#### Medium Priority

- [ ] **Metadata Generation System**
  - Automatic metadata export per video group
  - Fields: video_name, object_id, resolution, frame_count, scene_category, foreground_category, foreground_subtype, occluder_list, avg_occlusion_ratio
  - Export format: JSON or CSV
  - API: `vget /recording/{id}/metadata`

- [ ] **Semantic Annotation Export**
  - Instance label export per frame
  - Material parameter export
  - Depth/scale information export
  - API: `vget /object/{id}/semantic_info`

- [ ] **Occlusion Ratio Calculator**
  - Calculate occlusion ratio from masks
  - Track occlusion over time
  - Report average occlusion ratio
  - Utility function in CaptureActor

- [ ] **Python API Enhancement**
  - Extend `client/python/unrealcv/` with new commands
  - Test scripts for layered recording workflow
  - Test scripts for camera movement workflow

#### Low Priority / Phase 2

- [ ] **Audio Layer Recording** (Phase 2)
  - Bind audio sources to foreground actors
  - Separate audio recording per layer
  - Spatial audio positioning
  - Audio-video synchronization
  - May reuse `GetAudioMixer()` pattern from removed code

- [ ] **Shadow/Reflection Separation** (Phase 2)
  - Render pass for shadows only
  - Render pass for reflections only
  - Composite control for environmental effects

- [ ] **Dynamic Object Support** (Phase 2-3)
  - Animation playback control
  - Physics simulation
  - Interaction scenarios (handshake, photo-taking, etc.)

- [ ] **Cloud Deployment Scripts** (Phase 2-3)
  - Batch rendering on cloud servers
  - Distributed job scheduling
  - Quality control & validation

### Key Design Principles

1. **Separation of Concerns**:
   - C++ for rendering primitives
   - Python for automation/composition

2. **Camera ID-Based API**:
   - User-friendly interface
   - Automatic resource management

3. **Modular Architecture**:
   - Each actor has single responsibility
   - Compose complex behaviors from simple primitives

4. **Automation-First**:
   - Every operation should be scriptable
   - Batch processing support
   - Minimal manual intervention

### Notes for Future Development

- **Blueprint vs C++**: Use C++ for high-frequency/low-level functions, Blueprint for rapid prototyping
- **Performance**: Multi-threaded rendering, GPU optimization for large-scale production
- **Validation**: Automated quality checks (resolution, frame count, metadata accuracy)
- **Version Control**: Create checkpoint commits after major features
- **Testing**: Manual testing in UE editor due to complexity of UE5 build system
