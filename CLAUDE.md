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
│   ├── BPFunctionLib/    # Blueprint library implementations
│   ├── Sensor/           # Camera sensor implementations & async capture
│   ├── Commands/         # Command handlers (CameraHandler, ObjectHandler, etc.)
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

3. **Command Handlers** in `Private/Commands/`:
   - `CameraHandler` - `/camera/*` (control, sensors, recording)
   - `ObjectHandler` - `/object/*` (visibility, transform, properties)
   - `ActionHandler` - `/action/*` (pause, level load, keyboard input)
   - `CaptureActorHandler` - `/captureactor/*` (dataset recording)
   - `AliasHandler`, `AgentNavHandler`, `PluginHandler`

4. **UFusionCamSensor** (FusionCamSensor.h) - Unified multi-pass rendering orchestrator
   - Manages 5 specialized sensor types in single render pass
   - Configurable visibility/lighting modes for layer isolation
   - Async GPU readback for high-throughput recording

5. **AFusionCamCaptureActor** (FusionCamCaptureActor.h) - Recording lifecycle management
   - Created by CameraHandler when recording starts
   - Automatically destroyed when recording stops
   - Writes frame data directly to disk (async file I/O)

**Annotation System**:
- **FObjectAnnotator** (ObjectAnnotator.h) - Static facade for annotation operations
- **IAnnotatorImpl** - Interface for annotation strategies
- **FDirectAnnotator** (DirectAnnotator.h) - Direct per-actor color assignment
- **FProxyAnnotator** (ProxyAnnotator.h) - Efficient batch annotation via post-process
- Switch between strategies via `FObjectAnnotator::SetAnnotationMode()`

**Blueprint Function Libraries** (BPFunctionLib/):
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
Each handler in `Private/Server/` registers commands via `RegisterCommands()`:
```cpp
CommandDispatcher->BindCommand(
    "vget /object/[str]/location",
    FDispatcherDelegate::CreateRaw(this, &FObjectHandler::GetLocation),
    "Get object location [x, y, z]"
);
```

Handler implementations return `FExecStatus`:
- `FExecStatus::OK(result)` - Success with optional string result
- `FExecStatus::Error(msg)` - Failure
- `FExecStatus::GetInvalidArgument()` - Argument parsing error

Command format: `vget /handler/subcommand [args]` or `vset /handler/subcommand [args]`

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
- **Integer format** (legacy): 0, 1, 2... (creation-order based, unstable)
- **CID format**: `CID-ActorName-UUID` (stable, tied to sensor instance)
- Both supported via `ParseCameraID()` utility

### Object Visibility Control
`SetActorHiddenInGame()` for rendering control (affects all cameras). Per-sensor filtering via UFusionCamSensor visibility modes.

### Asset Pool System
`FAssetPoolManager` (Utils/AssetPoolManager.h) manages runtime asset registration:
- Categories: foreground, occluder, scene, etc.
- `RegisterAsset(Category, AssetPath)` - Add asset at runtime
- `GetRandomAsset(Category)` - Random selection for scene generation

### Color Generation
`FColorGenerator` (ObjectAnnotator.h) generates deterministic annotation colors from object indices using channel-wise bit patterns.

### Async Capture Pipeline
1. **Render Thread**: `ENQUEUE_RENDER_COMMAND` - GPU readback via `FRHIGPUTextureReadback`
2. **Game Thread**: `AsyncTask` - PNG encoding and disk write (non-blocking)
3. **Next Frame**: Readback data available for use or file write

Key files:
- `Private/Sensor/AsyncCaptureHelper.h/.cpp`
- `Private/Sensor/CameraSensor/BaseCameraSensor.h/.cpp`
- `Private/Actor/FusionCamCaptureActor.cpp`

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

## Git Workflow

Branch: `shc/dev` for HUAWEI_Project dataset production (UE 5.2-5.6)
