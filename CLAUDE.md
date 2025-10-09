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
Now we are in a development branch 'shc/ue5.6' forked from the '5.2' branch, wich means we need to support from ue5.2 to ue5.6, but not previous ue versions. You can just Focus on ue5 rather than ue4. This branch of mine is focusing on support optical-flow image output (already implemented) and support ue5.6 (hopefully every compile warning is handled and the executable is tested). The optical-flow feature's PR has been merged to the official repo's 5.2 branch. Now let's focus on my work, wich is not mainly for PR, but for my 'HUAWEI_Project', I will construct a huge video dataset, the detailed information can be found in './SOW-基于CG的音视频分层数据生产-latest.md'. To record this big dataset, I have my another branch named 'shc/dev', that was a older branch, so be careful to merge it to the current branch. It has implemented an video record pipline in FusionCamSensor.h/.cpp, but that was seperate from the original graceful implementation in DataCaptureActor.cpp, because I did not see the ADataCaptureActor in DataCaptureActor.h/.cpp. So I think we might need to do:
1. research the record logic in both files [completed]
2. merge the 'shc/dev' branch to the current branch 'shc/ue5.6' [completed]
