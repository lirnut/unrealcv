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

### TODO List - Phase 1 (Current Focus - REVISED)

#### High Priority (Immediate)

**✅ Already Implemented - Leverage Existing APIs:**
- Camera control: `vset/vget /camera/[uint]/location`, `rotation`, `fov`, `moveto`, `size`, `projection_type`
- Recording: `vset /camera/[uint]/record`, `bullet_time_record`, `vget /camera/[uint]/record` (status)
- Object visibility: `vset /object/[str]/show`, `vset /object/[str]/hide`
- Render passes: `vget /camera/[uint]/lit`, `depth`, `normal`, `optical_flow`, `seg`, `object_mask`

- **Camera Intrinsics/Extrinsics Export API**
  - Commands:
    - `vget /camera/[uint]/intrinsics` → returns focal length, principal point, distortion
    - `vget /camera/[uint]/extrinsics` → returns rotation matrix (3x3) and translation vector
    - `vget /camera/[uint]/projection_matrix` → returns 4x4 projection matrix

- **Programmatic Camera Trajectory System**
  - Create `ACameraMotionController` actor to control camera movement
  - Decouples camera movement from recording (different actors, different concerns)
  - Supports the 10 required camera movements:
    - 6 fixed: RotateLeft45°, RotateRight45°, RotateUp45°, Rotate360°, ZoomIn, ZoomOut
    - 4 random direction movements
  - API commands:
    - `vset /camera/[uint]/motion/start [str] [params]` - start trajectory
    - `vset /camera/[uint]/motion/stop` - stop trajectory
    - `vget /camera/[uint]/motion/status` - check if moving
    - `vget /camera/[uint]/motion/progress` - get completion percentage
  - Compatible with existing `bullet_time_record` API

**🔧 New Features Needed:**
- [ ] **Multi-Layer Rendering Orchestration** (Python layer - uezoo)
  - **DECISION**: Implement in Python (uezoo), NOT C++
  - C++ plugin provides primitives, Python provides orchestration
  - Better separation of concerns: C++ = rendering, Python = composition logic
  - **Human Comment**: See ./Source/uezoo/start_mk_dataset.py, but it should be updated due to new features of "Programmatic Camera Trajectory System". It is still calling old api like start_bullet_time_record.

#### Medium Priority

- [ ] **Metadata Generation Utilities**
  - Helper functions in Python to calculate:
    - Occlusion ratio from masks
    - Scene/foreground categories
    - Frame count, resolution verification
  - Export to JSON/CSV
  - Implementation: Python utility functions in uezoo
  - **Human Comment**: Can be done in Python post-processing

- [ ] **Semantic Annotation Export** (if needed beyond existing seg/mask APIs)
  - Check if existing APIs (`seg`, `object_mask`) are sufficient
  - May need material parameters, instance IDs
  - Commands:
    - `vget /object/[str]/material_params` → returns material properties
    - `vget /object/[str]/instance_id` → returns unique instance ID
  - **Human Comment**: Evaluate if existing APIs are sufficient first

- [ ] **Batch Camera Parameter Export**
  - Export camera parameters for all frames in a trajectory
  - Utility command: `vget /camera/[uint]/trajectory/params [start_frame] [end_frame]`
  - Returns JSON array of intrinsics/extrinsics per frame
  - **Human Comment**: Useful for camera movement videos

#### Low Priority / Deferred

- [ ] **New Visibility API** (QUESTION: Is this needed?)
  - Existing APIs: `vset /object/[str]/show`, `vset /object/[str]/hide`
  - Proposed new APIs seem redundant with existing functionality
  - **DECISION**: Use existing APIs first, only add new ones if there's a specific gap
  - **Human Comment**: "see ObjectHandler.cpp(vset /object/[str]/hide), shall we make a new API? is that necessary?"

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
  - May require custom shaders or post-process materials

- [ ] **Dynamic Object Support** (Phase 2-3)
  - Animation playback control via Blueprint or C++ API
  - Physics simulation
  - Interaction scenarios (handshake, photo-taking, etc.)
  - Likely implemented in Python layer (uezoo)

- [ ] **Cloud Deployment Scripts** (Phase 2-3)
  - Batch rendering on cloud servers
  - Distributed job scheduling
  - Quality control & validation
  - Python automation layer (separate from UnrealCV plugin)

**What NOT to Do:**
- ❌ Don't create new visibility APIs - existing show/hide is sufficient
- ❌ Don't implement multi-layer orchestration in C++ - Python is better suited
- ❌ Don't implement metadata generation in C++ - Python post-processing is easier

### Current Architecture Understanding (Post-Discussion)

**Python Layer (uezoo - Source/uezoo/)**:
- `start_mk_dataset.py` - main entry point for dataset generation
- Uses gym-unrealcv for environment control
- Agents: PoseTracker, Nav2GoalAgent for character movement
- Calls UnrealCV APIs via TCP: start_record, get_record_status, batch_cmd
- Saves trajectories, metadata, renders videos (C++ side will do this if you call record series of commands, but if C++ side doesn't save video, you need to save it in Python side)
- **Future**: Add multi-layer rendering orchestration here

**C++ Plugin (UnrealCV - Source/UnrealCV/)**:
- Provides rendering primitives via TCP API
- CameraHandler: camera control, recording, render passes
- ObjectHandler: object manipulation, show/hide
- FusionCamCaptureActor: automatic recording lifecycle management
- **Future**: Add camera parameter export, motion controller

**Division of Labor (Confirmed)**:
- **C++**: Low-level rendering, camera control, object control, parameter export
- **Python**: Scene composition, multi-pass orchestration, metadata, batch automation

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
