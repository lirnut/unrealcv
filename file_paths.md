# UnrealCV File Path Index

Auto-generated index of frequently accessed files. Last updated: 2026-02-20

Use these shorthand paths in prompts instead of copy-pasting full paths.

## Session Context Files (2026-02-20)

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


### Sensor System (Rendering Core)
- `Source/UnrealCV/Private/Sensor/CameraSensor/MovieQualityRenderComponent.cpp` - High-quality rendering (31 commits)
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



