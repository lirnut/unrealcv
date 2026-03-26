# UnrealCV Plugin - Project Configuration

This is a template file. Copy it to `AGENTS.md` and fill in your own configuration.

## Canonical Repository
- **Remote Server**: <your-server-name>
- **UE Path**: `~/UE/` (on remote server)
- **Project Path**: <your-project-path>
- **Plugin Path**: <your-project-path>/Plugins/unrealcv/

## Environment
- **UE Version**: 5.6.1
- **Platform**: Linux (Ubuntu 24.04)
- **Build Tool**: `~/UE/Engine/Build/BatchFiles/RunUAT.sh`

## Workflow Commands

### Build Project
```bash
cd "<your-project-path>"
~/UE/Engine/Build/BatchFiles/Linux/Build.sh <ProjectName>Editor Linux Development "$(pwd)/<ProjectName>.uproject" -waitmutex
```

### Launch UE Editor (requires display)
```bash
export DISPLAY=:0
cd "<your-project-path>"
~/UE/Engine/Binaries/Linux/UnrealEditor "$(pwd)/<ProjectName>.uproject"
```

### Build Plugin Only
```bash
~/UE/Engine/Build/BatchFiles/RunUAT.sh BuildPlugin \
  -Plugin="<your-project-path>/Plugins/unrealcv/UnrealCV.uplugin" \
  -Package="/tmp/UnrealCV-Linux" \
  -TargetPlatforms=Linux
```

## Key Files
- Project: `<ProjectName>.uproject`
- Plugin: `Plugins/unrealcv/UnrealCV.uplugin`
- Source: `Plugins/unrealcv/Source/`
- Binaries: `Plugins/unrealcv/Binaries/Linux/`

## Notes
- Use your preferred SSH tool to run commands on remote server