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

**Recording System Architecture (UPDATED 2025-10-31):**
```
┌─────────────────────────────────────────┐
│  UMG UI Interface (Blueprint)           │
│  - WBP_DatasetRecorder                  │
│  - WBP_TrajectoryRecorder               │
│  - User controls & progress tracking    │
└────────────────┬────────────────────────┘
                 │ Blueprint Calls
┌────────────────▼────────────────────────┐
│  Blueprint Function Libraries (C++)     │
│  ├─ URecordingBPLib (existing)          │
│  ├─ USceneCompositionBPLib (new)        │
│  ├─ ULayeredRecordingBPLib (new)        │
│  └─ FAssetPoolManager (new)             │
└────────────────┬────────────────────────┘
                 │
┌────────────────▼────────────────────────┐
│  Recording System                       │
│  ├─ AFusionCamCaptureActor              │
│  │   (automatic lifecycle management)   │
│  ├─ Scene composition logic             │
│  └─ Multi-layer recording orchestration │
└────────────────┬────────────────────────┘
                 │
┌────────────────▼────────────────────────┐
│  Render Pipeline                        │
│  ├─ UFusionCamSensor (sensor fusion)    │
│  ├─ Actor visibility control            │
│  └─ Multi-pass rendering                │
└─────────────────────────────────────────┘

Optional Post-Processing:
┌─────────────────────────────────────────┐
│  Python Scripts (optional)              │
│  - Metadata validation                  │
│  - Quality control checks               │
│  - Dataset statistics                   │
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

3. ✅ **Scene Composition System (2025-10-31)**
   - `FAssetPoolManager` - Singleton asset pool manager with runtime registration
   - `USceneCompositionBPLib` - Blueprint-exposed scene generation functions
   - `RegisterAsset(Category, AssetPath)` - Dynamic asset registration from Blueprint/UI
   - `GenerateRandomScene()` - Automated scene generation (foreground + occluders + camera)
   - `SpawnRandomForeground/Occluders()` - Actor spawning from asset pools
   - `PositionCameraToViewTarget()` - Automatic camera positioning
   - `CreateFreeCamera()` - Runtime camera creation for scene generation
   - All functions Blueprint-callable for UE Editor UI integration

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

### Development Strategy (UPDATED 2025-10-31)

**NEW ARCHITECTURE: UE Editor-Centric (No Python Required for Recording)**

**Design Philosophy**:
- All interactions happen within UE Editor through Blueprint Function Libraries and UMG UI
- No TCP server dependency for dataset generation workflow
- Direct scene configuration and preview in UE Editor viewport
- Reduces complexity: 2 workspaces (UE Editor + optional post-processing) instead of 3

**Division of Labor**:
- **C++ Blueprint Function Libraries**: Scene composition, layered recording, asset pool management
- **UMG UI Interface**: User-friendly controls for batch generation within UE Editor
- **Optional Python Post-Processing**: Metadata analysis, quality validation (after recording)

**See**: `ARCHITECTURE.md` for detailed implementation plan

### TODO List - Phase 1 (Current Focus - ARCHITECTURE UPDATED 2025-10-31)

#### High Priority (Immediate)

**✅ Already Implemented:**
- `URecordingBPLib` - Blueprint-exposed recording functions (RecordingBPLib.h/.cpp)
  - `StartNormalRecording()` - Normal video recording
  - `StartBulletTimeRecording()` - 360° rotation recording
  - `StartTrajectoryRecording()` - 10 trajectory types (SOW requirement)
  - Camera management functions (`GetAllCameras`, `IsRecording`, etc.)
- `AFusionCamCaptureActor` - Automatic recording lifecycle management
- `UFusionCamSensor` - Sensor fusion for multi-pass rendering
- Object visibility control (`SetActorHiddenInGame`)

**✅ Scene Composition System (Implemented 2025-10-31):**

1. ✅ **FAssetPoolManager** (C++ Utility Class)
   - Location: `Source/UnrealCV/Private/Utils/AssetPoolManager.h/.cpp`
   - Singleton pattern with runtime asset registration
   - Methods: `RegisterAsset()`, `GetRandomAsset()`, `GetAssetsInCategory()`, `HasCategory()`
   - Empty constructor - assets registered dynamically from Blueprint/UI

2. ✅ **USceneCompositionBPLib** (Blueprint Function Library)
   - Location: `Source/UnrealCV/Public/BPFunctionLib/SceneCompositionBPLib.h/.cpp`
   - `RegisterAsset(Category, AssetPath)` - Dynamic asset registration
   - `GenerateRandomScene()` - Complete scene setup in XY area
   - `SpawnRandomForeground()` - Spawn from asset pool
   - `SpawnRandomOccluders()` - Spawn between camera and foreground
   - `PositionCameraToViewTarget()` - Automatic camera positioning
   - `ClearScene()` - Cleanup generated actors
   - `GetForegroundCategories()`, `GetOccluderCategories()` - Query available categories

3. ✅ **CreateFreeCamera()** (URecordingBPLib)
   - Runtime camera creation for scene generation
   - Returns camera ID for use with recording/scene functions

**🔧 Next Features Needed:**

1. [ ] **ULayeredRecordingBPLib** (Blueprint Function Library)
   - Location: `Source/UnrealCV/Public/BPFunctionLib/LayeredRecordingBPLib.h/.cpp`
   - Purpose: Record all layers for SOW dataset (5 files per video)
   - Key Functions:
     - `RecordAllLayers()` - Orchestrate 5-layer recording:
       1. Original composite video
       2. Foreground mask
       3. Background layer (foreground hidden)
       4. Complete foreground layer (only foreground visible)
       5. Metadata JSON export
     - `ExportMetadataToJSON()` - Save metadata struct to JSON file

4. [ ] **UMG UI Interface** (Blueprint Widgets)
   - Location: `Content/UnrealCV/UI/WBP_DatasetRecorder.uasset`
   - Purpose: User-friendly controls for batch generation within UE Editor
   - Features:
     - Scene configuration (spawn area, foreground type, occluder count)
     - Recording settings (duration, FPS, resolution, orientation)
     - Batch generation (target count, progress bar, start/stop controls)
     - Scene preview (occlusion ratio validation, regenerate button)
   - See: `ARCHITECTURE.md` for detailed UI mockup

**Implementation Order**:
1. Week 1: `FAssetPoolManager` + unit tests
2. Week 2: `USceneCompositionBPLib` + console testing
3. Week 3: `ULayeredRecordingBPLib` + manual Blueprint testing
4. Week 4: UMG UI + single scene workflow testing
5. Week 5-6: Batch testing (1000 videos) + optimization
6. Week 7-12: Full production (40K videos)

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
- ❌ Don't use TCP server for dataset generation workflow (deprecated in favor of UE Editor UI)
- ❌ Don't implement scene composition in Python - use Blueprint Function Libraries instead
- ❌ Don't create external automation scripts - use UMG UI for batch control

### Current Architecture Understanding (UE Editor-Centric)

**C++ Blueprint Function Libraries**:
- `URecordingBPLib` - Camera recording control (already exists)
- `USceneCompositionBPLib` - Scene generation automation (to be implemented)
- `ULayeredRecordingBPLib` - Multi-layer recording orchestration (to be implemented)
- `FAssetPoolManager` - Asset path management (to be implemented)

**UMG UI Interface (Blueprint)**:
- `WBP_DatasetRecorder` - Main UI for batch dataset generation
- `WBP_TrajectoryRecorder` - Camera trajectory recording UI
- User controls: scene configuration, recording settings, batch progress

**Core Actors (C++)**:
- `AFusionCamCaptureActor` - Recording lifecycle management (already exists)
- `UFusionCamSensor` - Sensor fusion for multi-pass rendering (already exists)

**Optional Python Post-Processing**:
- Metadata validation and statistics (after recording completes)
- Quality control checks (occlusion distribution, file completeness)
- Dataset analysis and reporting

**Division of Labor (UPDATED)**:
- **C++ BPLibs**: Scene composition, layered recording, asset management, occlusion calculation
- **UMG UI**: User interaction, batch control, progress tracking
- **Python (optional)**: Post-recording validation and analysis only

### Key Design Principles (UPDATED 2025-10-31)

1. **UE Editor-Centric Workflow**:
   - All dataset generation happens within UE Editor
   - No external dependencies (TCP server, Python) during recording
   - Immediate visual feedback and debugging

2. **Blueprint Function Libraries**:
   - Expose all functionality to Blueprints for UMG UI integration
   - C++ implementation for performance-critical operations
   - Easy to test in UE Editor console

3. **Modular Architecture**:
   - Each BPLib has single responsibility (recording, scene composition, layered output)
   - Asset pool separated from scene logic
   - Composable primitives for complex workflows

4. **Automation-First**:
   - Batch generation support (40K videos)
   - Progress tracking and error recovery
   - Metadata auto-generation

5. **Visual Validation**:
   - Preview generated scenes before recording
   - Real-time occlusion ratio feedback
   - Viewport visualization of randomized layout

### Notes for Future Development

- **Blueprint vs C++**: Use C++ for high-frequency/low-level functions, Blueprint for rapid prototyping
- **Performance**: Multi-threaded rendering, GPU optimization for large-scale production
- **Validation**: Automated quality checks (resolution, frame count, metadata accuracy)
- **Version Control**: Create checkpoint commits after major features
- **Testing**: Manual testing in UE editor due to complexity of UE5 build system
