# UnrealCV File Path Index

Auto-generated index of frequently accessed files. Last updated: 2026-02-24

Use these shorthand paths in prompts instead of copy-pasting full paths.

## Session Context Files (2026-02-24)

**Session Topic**: Global MQRC render quality configuration system - centralized runtime control for anti-aliasing, exposure, motion blur, Lumen, and color grading

### Sensor System (Rendering Configuration)
- `Source/UnrealCV/Public/Sensor/CameraSensor/MovieQualityRenderComponent.h` - FMQRCSettings struct with static GlobalSettings for unified render quality control (anti-aliasing, exposure, Lumen, color grading)
- `Source/UnrealCV/Private/Sensor/CameraSensor/MovieQualityRenderComponent.cpp` - Applies GlobalSettings to PostProcessSettings and AntiAliasingMethod (line 382, 470-550)

### Command Handlers (Render Quality)
- `Source/UnrealCV/Private/Commands/RenderQualityHandler.h` - FMQRCHandler class with TCP command methods for render quality configuration
- `Source/UnrealCV/Private/Commands/RenderQualityHandler.cpp` - /mqrc/* TCP commands: antialiasing, exposure_method/bias/min_brightness/max_brightness, motion_blur, lumen_quality, saturation, contrast, gamma, gain

### Server Core
- `Source/UnrealCV/Private/Server/UnrealcvServer.cpp` - Registered FMQRCHandler in CommandHandlers (line 126)

### UE5 Engine References (Post-Processing)
- `H:\UE_5.6\Engine\Source\Runtime\Engine\Classes\Engine\Scene.h` - FPostProcessSettings struct with AutoExposureMinBrightness/MaxBrightness properties

## Previous Session Files (2026-02-24)

**Session Topic**: Python client TCP resilience - fixing process hang on Ctrl+C and TCP disconnection in Windows

### Python Client Library (TCP Connection Management)
- `client/python/unrealcv/__init__.py` - TCP client with daemon thread + timeout join() to prevent hang on disconnect (daemon=True, join(timeout=2.0))
- `docs/python/run_matting_generation.py` - Dataset generation script with AssertionError exception handling for TCP failures

## Earlier Session Files (2026-02-21)

**Session Topic**: Asset discovery and spawn system - implementing vget /objects/scan_assets command and vset /objects/spawn_from_path with robust Cook-friendly loading

### Blueprint Function Libraries (Asset Management & Spawn)
- `Source/UnrealCV/Public/BPFunctionLib/AssetDiscoveryBPLib.h` - Asset registry scanning for spawnable resources (StaticMesh/SkeletalMesh/Blueprint)
- `Source/UnrealCV/Private/BPFunctionLib/AssetDiscoveryBPLib.cpp` - ScanSpawnableAssets with FAssetRegistryModule, Actor Blueprint filtering
- `Source/UnrealCV/Public/BPFunctionLib/SpawnBPLib.h` - Unified spawn from asset path API
- `Source/UnrealCV/Private/BPFunctionLib/SpawnBPLib.cpp` - 7-layer spawn strategy: StaticLoadClass→LoadObject→StaticLoadObject fallbacks
- `Source/UnrealCV/Private/BPFunctionLib/SceneCompositionBPLib.cpp` - SpawnActorFromMetadata refactored to use SpawnBPLib (line 1117)
- `Source/UnrealCV/Public/BPFunctionLib/SceneCompositionBPLib.h`

### Command Handlers (Object Management)
- `Source/UnrealCV/Private/Commands/ObjectHandler.cpp` - Added ScanAssets and SpawnFromPath commands (delegates to SpawnBPLib)
- `Source/UnrealCV/Private/Commands/ObjectHandler.h` - Object manipulation commands: spawn, destroy, location, rotation, bounds

## Earlier Session Files (2026-02-21)

**Session Topic**: TCP command execution pipeline optimization - analyzing vget /camera/0/lit request flow and reducing TCP latency

### Server Core & Command Dispatch
- `Source/UnrealCV/Private/Server/UnrealcvServer.cpp` - Main server with Tick-based request processing, HandleRawMessage, ProcessPendingRequest
- `Source/UnrealCV/Private/Server/UnixTcpServer.cpp` - TCP socket server with FSocket management, **TCP_NODELAY optimization added**
- `Source/UnrealCV/Public/Server/UnixTcpServer.h` - TCP server class definition
- `Source/UnrealCV/Public/Server/CommandDispatcher.h` - Command routing interface with BindCommand and Exec methods
- `Source/UnrealCV/Private/Server/CommandDispatcher.cpp` - Command regex matching and delegate dispatch (Exec method at line 184)

### Python Client
- `client/python/unrealcv/__init__.py` - Python TCP client with batch_cmd, request, **TCP_NODELAY optimization added** (line 219)
- `Source/uezoo/unrealcv/api.py` - High-level Python API wrapper with batch_cmd implementation (line 189-212)

### UE5 Engine References (Networking)
- `H:\UE_5.6\Engine\Source\Runtime\Sockets\Public\Sockets.h` - FSocket class definition with SetNoDelay() method
- `H:\UE_5.6\Engine\Source\Runtime\Sockets\Public\SocketSubsystem.h` - Socket subsystem interface

## Documentation
- `cmd.md` - TCP command reference (vget/vset commands)
- `SOW-基于CG的音视频分层数据生产-latest.md` - Project specification

## Frequently Modified Files (By Commit Count)

### Sensor System (Rendering Core)
- `Source/UnrealCV/Private/Sensor/CameraSensor/MovieQualityRenderComponent.cpp` - High-quality rendering (32 commits - added GlobalSettings configuration)
- `Source/UnrealCV/Public/Sensor/CameraSensor/MovieQualityRenderComponent.h`

### Blueprint Function Libraries
- `Source/UnrealCV/Private/BPFunctionLib/DatasetAutomationBPLib.cpp` - Batch generation (29 commits)
- `Source/UnrealCV/Public/BPFunctionLib/DatasetAutomationBPLib.h`
- `Source/UnrealCV/Private/BPFunctionLib/AutomationBPLib.cpp` - Automation utilities

### Recording & Capture
- `Source/UnrealCV/Private/Actor/FusionCamCaptureActor.cpp` - Recording lifecycle manager (26 commits)
- `Source/UnrealCV/Public/Actor/FusionCamCaptureActor.h`

### Utilities
- `Source/UnrealCV/Private/Utils/MetaHumanCacheManager.cpp` - MetaHuman optimization
