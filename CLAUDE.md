# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

UnrealCV is an Unreal Engine 5.2+ plugin for computer vision research providing:
- TCP server for external program communication
- Camera and object manipulation commands
- Blueprint function libraries for easy integration
- Python client library for remote control
- High-throughput recording system for dataset generation (40K-400K layered videos)

## Development Workflow

### Build & Compilation

**UE5 Project-Integrated Build** (Recommended):
1. Place plugin in UE project's `Plugins` folder
2. Open C++ project in Visual Studio (version must match UE version)
3. Plugin compiles as part of standard project build
4. Binary builds to `Plugins/UnrealCV/Binaries/`

**Build Time**: ~17 seconds on 16-core system (DebugGame configuration)

**No external build system needed** - UE's UnrealBuildTool (Source/UnrealCV/UnrealCV.Build.cs) handles all compilation

### Running Tests

Python tests require the game to be running (either compiled binary or editor in Play mode):

```bash
# Install dependencies
pip install -r test/requirements.txt

# Run all tests (stops on first failure)
pytest test/ -x

# Run specific test file
pytest test/server/camera_test.py

# Run specific test function
pytest test/server/camera_test.py::test_camera_control

# Verbose output with diagnostic info
pytest test/ -v -s

# Show print statements during test
pytest test/ -s
```

Tests communicate via TCP to UnrealCV server on the running game instance. See `test/README.md` for details.

## Code Structure & Key Modules

**Source Organization**:
```
Source/UnrealCV/
├── Private/              # Implementation files
│   ├── BPFunctionLib/    # Blueprint library implementations (11 libraries)
│   ├── Sensor/           # Camera sensor implementations & async capture
│   ├── Commands/         # 11 command handlers (ActionHandler, CameraHandler, etc.)
│   ├── Controller/       # WorldController, ObjectAnnotator, PlayerViewMode
│   ├── Server/           # TCP server, CommandDispatcher, handlers
│   ├── Actor/            # Recording actors, camera actors, puppeteers
│   ├── Component/        # UE5 components for CV functionality
│   ├── Utils/            # Utility functions (AssetPoolManager, etc.)
│   └── UI/               # UMG widget implementations
└── Public/               # Header files (mirrors Private structure)
```

## Architecture

**Core Components**:

1. **FUnrealcvServer** (UnrealcvServer.h) - Main server class implementing FTickableGameObject
   - Initializes TCP server on plugin startup
   - Ticks command dispatcher each frame

2. **CommandDispatcher** (CommandDispatcher.h) - Routes TCP commands to handlers
   - Parses incoming "vget"/"vset" commands
   - Dispatches to appropriate handler based on command prefix

3. **11 Command Handlers** - Each handles specific command category:
   - `CameraHandler` - `/camera/*` (control, sensors, recording)
   - `ObjectHandler` - `/object/*` (visibility, transform, properties)
   - `ActionHandler` - `/action/*` (pause, level load, keyboard input)
   - `CaptureActorHandler` - `/captureactor/*` (dataset recording)
   - `AliasHandler`, `AgentNavHandler`, `PluginHandler`, etc.

4. **UFusionCamSensor** (FusionCamSensor.h) - Unified multi-pass rendering orchestrator
   - Manages 5 specialized sensor types in single render pass
   - Configurable visibility/lighting modes for layer isolation
   - Async GPU readback for high-throughput recording

5. **AFusionCamCaptureActor** (FusionCamCaptureActor.h) - Recording lifecycle management
   - Created by CameraHandler when recording starts
   - Automatically destroyed when recording stops
   - Writes frame data directly to disk (async file I/O)

**Blueprint Function Libraries** (11 total in BPFunctionLib/):
- `URecordingBPLib` - Camera recording control (normal, bullet-time, trajectory modes)
- `USceneCompositionBPLib` - Automated scene generation from asset pools
- `UDatasetAutomationBPLib` - High-level batch generation orchestration
- `USensorBPLib` - Multi-sensor capture control
- `UAnnotationBPLib` - Segmentation and mask utilities
- Plus: AnimationBPLib, MetaHumanBPLib, NavigationBPLib, SerializeBPLib, VisionBPLib, JsonObjectBP

**Camera Sensor System** (in Sensor/CameraSensor/):
- `UFusionCamSensor` - Unified 5-sensor orchestrator with multi-pass rendering
- `UBaseCameraSensor` - Base class with GPU readback, async capture pipeline
- Specialized sensor types:
  - `ULitCamSensor` - RGB data (FColor)
  - `UDepthCamSensor` - Depth data (Float16)
  - `UAnnotationCamSensor` - Instance segmentation masks (FColor)
  - `UNormalCamSensor` - Normal maps (FColor)
  - `UFlowCamSensor` - Optical flow (FColor)

## Key Patterns & Conventions

### Command System Pattern
Each handler in `Private/Server/` implements command processing:
```cpp
FString HandleCommand(const TArray<FString>& Cmd);  // "vget /camera/0/location" → "100.0 200.0 50.0"
```

- `vget` = Query (returns data)
- `vset` = Modify (returns success status)
- Command format: `vget|vset /handler/subcommand [args]`
- Return format: Single line string or error message prefixed with "ERROR:"

### Async GPU Readback Pattern (2025-11-12)
Recording uses 3-phase pipeline to maximize throughput:

1. **Game Thread**: Validate, enqueue render command
2. **Render Thread**: GPU readback (FRHIGPUTextureReadback)
3. **Game Thread**: Async file I/O (PNG encoding + disk write)

Benefits: Non-blocking, parallel pipeline, 30-50% performance improvement vs sync capture

See `Source/UnrealCV/Private/Sensor/AsyncCaptureHelper.h` for implementation.

### Blueprint Function Library Pattern
All public C++ functions exposed to Blueprints use `UBlueprintFunctionLibrary`:
```cpp
UCLASS()
class UNREALCV_API UMyLib : public UBlueprintFunctionLibrary {
    GENERATED_BODY()
    UFUNCTION(BlueprintCallable, Category = "UnrealCV|MyCategory")
    static void MyFunction(/* params */);
};
```

- `BlueprintCallable` = Can be called from Blueprint
- `BlueprintPure` = No side effects, can be inlined
- Category prefix always "UnrealCV|"

## Recording System Architecture

```
┌──────────────────────────────────────┐
│  UMG UI Interface (Blueprint)        │
│  - WBP_DatasetRecorder               │
│  - User controls & batch generation  │
└──────────────┬───────────────────────┘
               │
┌──────────────▼───────────────────────┐
│  Blueprint Function Libraries        │
│  - URecordingBPLib (recording API)   │
│  - USceneCompositionBPLib (scenes)   │
│  - UDatasetAutomationBPLib (batches) │
└──────────────┬───────────────────────┘
               │
┌──────────────▼───────────────────────┐
│  Recording Actor                     │
│  - AFusionCamCaptureActor            │
│  - 1 per active recording session    │
│  - Auto-destroyed when done          │
└──────────────┬───────────────────────┘
               │
┌──────────────▼───────────────────────┐
│  Multi-Layer Rendering               │
│  - UFusionCamSensor orchestration     │
│  - 5 sensors per pass                │
│  - Visibility control for layer sep. │
└──────────────────────────────────────┘
```

Camera ID-based API:
- User provides camera ID (integer or CID-ActorName-UUID format)
- CameraHandler creates AFusionCamCaptureActor when recording starts
- Actor automatically destroyed when recording ends
- Tracking via `TMap<int32, AFusionCamCaptureActor*> CameraRecordingActors`

## Common Development Tasks

### Adding a New Command Handler

1. Create handler in `Source/UnrealCV/Private/Server/MyHandler.h/.cpp`
2. Add `FCommandDelegate Register()` method
3. Register handlers in `CommandDispatcher::Setup()`
4. Return command results as formatted strings (space-separated values or "ERROR: message")

Example: See `ObjectHandler.cpp` for typical implementation pattern

### Adding Blueprint-Exposed Functionality

1. Create new BPLib in `Source/UnrealCV/Public/BPFunctionLib/MyBPLib.h`
2. Inherit from `UBlueprintFunctionLibrary`
3. Mark functions with `UFUNCTION(BlueprintCallable, Category = "UnrealCV|MyCategory")`
4. Implement in `Source/UnrealCV/Private/BPFunctionLib/MyBPLib.cpp`

Example: See `RecordingBPLib.h` for recording API implementation

### Sensor-Level Frame Capture

For direct pixel data access in C++:

```cpp
// Sync path (blocks game thread)
UBaseCameraSensor* Sensor = GetSensor(...);
TArray<FColor> PixelData;
Sensor->Capture(PixelData, Width, Height);

// Async path (non-blocking, async file I/O)
Sensor->CaptureLitToFile(OutputPath);  // Use for recording
```

For TCP responses, use sync path. For recording, use async `CaptureToFile` pattern.

### Testing New Features

1. Create test file in `test/server/` with `_test.py` suffix
2. Tests use Python `unrealcv` client library
3. Require running game instance with UnrealCV active
4. Run with: `pytest test/server/myfeature_test.py -v -s`

See `test/server/camera_test.py` for examples

## Important Implementation Details

### Camera ID Format
- **Old format** (integers): 0, 1, 2... (based on creation order, unstable if sensors destroyed)
- **New format** (CID): `CID-ActorName-UUID` (stable, tied to sensor instance)
- Both formats supported in command handlers via `ParseCameraID()` utility

### Object Visibility Control
Use `SetActorHiddenInGame()` for rendering control (affects all camera sensors). For per-sensor filtering, modify UFusionCamSensor visibility modes.

### Asset Pool System
`FAssetPoolManager` (Utils/AssetPoolManager.h) manages runtime asset registration:
- Categories: foreground, occluder, scene, etc.
- `RegisterAsset(Category, AssetPath)` - Add asset at runtime
- `GetRandomAsset(Category)` - Random selection for scene generation

### Performance Considerations

- **GPU Readback**: Use async `CaptureToFile` for high-throughput recording (30-50% faster)
- **Multi-threading**: Async file I/O offloads disk writes from game thread
- **Render Targets**: UFusionCamSensor allocates 5 targets per sensor pair
- **Build Time**: C++ changes require editor recompile (~17 sec on 16-core system)

## Async Capture Implementation Details (Advanced)

The async pipeline separates GPU readback from file I/O:

1. **ENQUEUE_RENDER_COMMAND** on render thread - Read GPU texture via FRHIGPUTextureReadback
2. **AsyncTask(GameThread)** - PNG encoding and disk write (async, non-blocking)
3. **Next Frame**: Readback data available, can be used or written to file

This allows the game thread to continue while the render thread is busy with GPU operations.

Files involved:
- `Private/Sensor/AsyncCaptureHelper.h/.cpp` - Low-level async readback pool
- `Private/Sensor/CameraSensor/BaseCameraSensor.h/.cpp` - CaptureToFile implementations
- `Private/Actor/FusionCamCaptureActor.cpp` - Records frames using async pipeline

## Build Configuration

**UnrealCV.Build.cs** defines:
- **Public Dependencies**: Engine, RenderCore, Networking, Sockets, UMG, ImageWrapper, AVEncoder, AudioCapture, etc.
- **Editor Dependencies**: UnrealEd (if bBuildEditor == true)
- **PCH Usage**: ExplicitOrSharedPCHs
- **Dynamically Loaded**: Renderer

No manual build steps required - standard UE project build handles everything.

## Plugin Descriptor

**UnrealCV.uplugin** (JSON):
- Version: 5 (UE 5.2+)
- Module: UnrealCV (Runtime, PreDefault loading phase)
- Module: UnrealCVEditor (Editor, PostEngineInit loading phase)
- Depends on: AudioCapture (optional audio layer support)
- Enabled by default: true

## Project Mission Context

**Goal**: Generate 400K+ layered audio-video datasets for video inpainting/Omnimatte research

**Layered Video Output** (5 files per video):
1. Composite video + audio
2. Foreground mask (segmentation)
3. Background layer (inpainted)
4. Complete foreground layer
5. Metadata JSON (object ID, resolution, frames, categories, occlusion ratio)

**Key Requirements**:
- 30fps, 480p, 10-second clips
- 100+ object categories (humans, pets, vehicles, objects, scenes)
- 100+ distinct occlusion relationships
- Camera movements (6 fixed + 4 random directions per scene)
- Semantic annotations (instance, material, scale)

**Architecture**: UE Editor-centric (no external TCP dependency during recording). All scene composition and batch generation via Blueprint Function Libraries + UMG UI.

## Notes

- **No documentation files**: Code is self-documenting through clear naming
- **Code speaks for itself**: Prefer readable code structure over comments
- **Iterative development**: Working implementation > perfect design docs
- **Testing**: Manual testing in UE editor due to UE5 build system complexity
- **Git workflow**: Branch `shc/dev` for HUAWEI_Project dataset production (UE 5.2-5.6)
