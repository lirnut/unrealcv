# UnrealCV In-Game Recording Control UI - Setup Guide

## Overview

This guide explains how to create an in-game UI for controlling UnrealCV recording functions directly from the game, without needing the TCP server.

## What's Been Implemented (C++)

The following C++ components have been created:

1. **RecordingBPLib** (`Source/UnrealCV/Public/BPFunctionLib/RecordingBPLib.h`)
   - Blueprint Function Library with recording control functions
   - Functions: `StartNormalRecording`, `StartBulletTimeRecording`, `StopRecording`, `IsRecording`, etc.
   - Can be called from Blueprint or C++

2. **RecordingHUD** (`Source/UnrealCV/Public/UI/RecordingHUD.h`)
   - HUD class that spawns and manages the recording widget
   - Handles widget visibility toggling
   - Manages input mode (mouse cursor, UI focus)

3. **Dependencies**
   - Added `UMG` and `SlateCore` to Build.cs for UI support

## What You Need to Do in Unreal Editor

### Step 1: Create the UMG Widget Blueprint

1. **Open Unreal Editor** and load your project
2. **Create a folder** for UnrealCV UI assets:
   - Right-click in Content Browser
   - Create Folder → Name it `UnrealCV`
   - Inside `UnrealCV`, create another folder named `UI`

3. **Create the Widget Blueprint**:
   - Right-click in `Content/UnrealCV/UI`
   - User Interface → Widget Blueprint
   - Name it: `WBP_RecordingControl`

4. **Design the UI** (Open WBP_RecordingControl):

   **Recommended Layout:**
   ```
   Canvas Panel (Root)
   └── Vertical Box (center of screen)
       ├── Text Block (Title: "UnrealCV Recording Control")
       ├── Horizontal Box (Camera Selection)
       │   ├── Text Block ("Camera:")
       │   └── ComboBox (for camera selection)
       ├── Horizontal Box (File Path)
       │   ├── Text Block ("File Path:")
       │   └── Editable Text Box (FileName_TextBox)
       ├── Horizontal Box (Duration)
       │   ├── Text Block ("Duration (s):")
       │   └── Spin Box (Duration_SpinBox, default: 10.0)
       ├── Horizontal Box (FPS)
       │   ├── Text Block ("FPS:")
       │   └── Spin Box (FPS_SpinBox, default: 30, range: 1-60)
       ├── Horizontal Box (Target Actor - optional)
       │   ├── Text Block ("Target Actor:")
       │   └── Editable Text Box (TargetActor_TextBox)
       ├── Horizontal Box (Buttons)
       │   ├── Button (StartRecord_Button, text: "Start Recording")
       │   ├── Button (StartBulletTime_Button, text: "Start Bullet Time")
       │   └── Button (Stop_Button, text: "Stop Recording")
       ├── Text Block (Status_TextBlock, text: "Status: Idle")
       └── Button (Close_Button, text: "Close (F1)")
   ```

### Step 2: Implement Widget Logic (Blueprint Graph)

In the `WBP_RecordingControl` Graph Editor:

#### Event Construct (Initialize widget)
```
Event Construct
  → Get All Cameras (from RecordingBPLib)
  → Populate ComboBox with camera names
  → Set default values:
      - FileName: "C:/UnrealCV/output.mp4"
      - Duration: 10.0
      - FPS: 30
```

#### Button: Start Recording
```
On Clicked (StartRecord_Button)
  → Get selected Camera ID from ComboBox
  → Get FileName from FileName_TextBox
  → Get Duration from Duration_SpinBox
  → Get FPS from FPS_SpinBox
  → Call: Start Normal Recording (RecordingBPLib)
      - CameraID: <from ComboBox>
      - FileName: <from TextBox>
      - Duration: <from SpinBox>
      - FPS: <from SpinBox>
      - TargetToHide: None (or parse from TargetActor_TextBox)
  → If success:
      - Update Status_TextBlock: "Recording..."
  → Else:
      - Update Status_TextBlock: "Error: Failed to start"
```

#### Button: Start Bullet Time
```
On Clicked (StartBulletTime_Button)
  → Get selected Camera ID
  → Get FileName, Duration, FPS
  → Get Target Actor from TargetActor_TextBox
  → Call: Start Bullet Time Recording (RecordingBPLib)
  → Update status accordingly
```

#### Button: Stop Recording
```
On Clicked (Stop_Button)
  → Get selected Camera ID
  → Call: Stop Recording (RecordingBPLib)
  → Update Status_TextBlock: "Stopped"
```

#### Event Tick (Update status)
```
Event Tick
  → Get selected Camera ID
  → Call: Is Recording (RecordingBPLib)
  → If recording:
      - Status_TextBlock = "Recording..."
      - Enable Stop_Button
      - Disable StartRecord_Button and StartBulletTime_Button
  → Else:
      - Status_TextBlock = "Idle"
      - Disable Stop_Button
      - Enable StartRecord_Button and StartBulletTime_Button
```

### Step 3: Configure GameMode to Use Recording HUD

1. **Open your GameMode Blueprint** (or create one):
   - Content Browser → Blueprint Class → GameModeBase
   - Name it: `BP_UnrealCVGameMode`

2. **Set the HUD Class**:
   - Open BP_UnrealCVGameMode
   - In Class Defaults panel
   - Find "HUD Class"
   - Set to: `RecordingHUD` (the C++ class we created)

3. **Apply GameMode to Level**:
   - Open World Settings (Window → World Settings)
   - Under "Game Mode" section
   - Set "GameMode Override" to: `BP_UnrealCVGameMode`

### Step 4: Set Up Input Binding (Optional - for F1 toggle)

1. **Open Project Settings** (Edit → Project Settings)
2. **Go to Input** section
3. **Add Action Mapping**:
   - Name: `ToggleRecordingUI`
   - Key: F1

4. **Create Input Handler Blueprint**:
   - Open your PlayerController Blueprint (or create one)
   - Add event: `ToggleRecordingUI` (Action)
   - Get HUD → Cast to RecordingHUD
   - Call: `Toggle Recording Widget`

## Usage

### Running the Game with UI

1. **Play in Editor (PIE)** or Launch Game
2. **Press F1** to toggle the recording UI
3. **Configure recording**:
   - Select camera (0 = player camera, 1+ = spawned cameras)
   - Set output file path (e.g., `C:/UnrealCV/video.mp4`)
   - Set duration (seconds)
   - Set FPS (1-60)
4. **Click "Start Recording"** or **"Start Bullet Time"**
5. **Click "Stop Recording"** to stop early (optional)
6. **Press F1** to hide UI and continue playing

### Calling Recording Functions from Blueprint Directly

You don't need the UI! You can call these functions directly:

```
// Start normal recording
Start Normal Recording
  - Camera ID: 0
  - File Name: "C:/output.mp4"
  - Duration: 10.0
  - FPS: 30
  - Target To Hide: None

// Start bullet time recording
Start Bullet Time Recording
  - Camera ID: 0
  - File Name: "C:/bullettime.mp4"
  - Duration: 5.0
  - FPS: 30
  - Target: <ActorReference>

// Check if recording
Is Recording (Camera ID: 0) → Returns true/false

// Stop recording
Stop Recording (Camera ID: 0)
```

## Troubleshooting

### Widget doesn't appear
- Check that `RecordingHUD` is set as HUD Class in GameMode
- Check that GameMode is applied to the level (World Settings)
- Check Output Log for errors (Window → Developer Tools → Output Log)
- Try calling `ShowRecordingWidget()` manually from HUD

### "RecordingWidgetClass is not set" warning
- Make sure `WBP_RecordingControl` is created at `/Game/UnrealCV/UI/WBP_RecordingControl`
- Or override the widget class in RecordingHUD Blueprint

### Recording doesn't start
- Check Output Log for specific error messages
- Ensure camera ID is valid (use `GetCameraCount()` to check)
- Ensure file path is writable
- Check FPS is in valid range (1-60)
- For bullet time, ensure Target actor is valid

### Can't find RecordingBPLib functions in Blueprint
- Rebuild the project (compile C++ code)
- Restart Unreal Editor
- Search for "Recording" in the Blueprint palette

## Advanced: Console Command Alternative

If you don't want to create a full UI, you can use console commands instead:

1. **Add Console Commands** (TODO: implement if needed)
2. **Press `~` in-game** to open console
3. **Type commands**:
   ```
   unrealcv.recording.start 0 C:/output.mp4 10 30
   unrealcv.recording.stop 0
   unrealcv.recording.status 0
   ```

## File Structure

```
UnrealCV/
├── Source/UnrealCV/
│   ├── Public/
│   │   ├── BPFunctionLib/
│   │   │   └── RecordingBPLib.h ✅ (C++ - Already Created)
│   │   └── UI/
│   │       └── RecordingHUD.h ✅ (C++ - Already Created)
│   └── Private/
│       ├── BPFunctionLib/
│       │   └── RecordingBPLib.cpp ✅ (C++ - Already Created)
│       └── UI/
│           └── RecordingHUD.cpp ✅ (C++ - Already Created)
└── Content/UnrealCV/UI/ (TODO: Create in Editor)
    └── WBP_RecordingControl.uasset ❌ (Blueprint - YOU NEED TO CREATE)
```

## Summary

**C++ Code**: ✅ Complete (already implemented)
**UMG Widget**: ❌ You need to create this in Unreal Editor
**GameMode Setup**: ❌ You need to configure this
**Input Binding**: ❌ Optional, you need to set up if you want F1 toggle

Once you create the UMG widget and configure the GameMode, you'll have a fully functional in-game recording control UI!
