Blueprint vs TCP API: Choosing the Right Interface
==================================================

UnrealCV provides two primary interfaces for controlling the engine:

1. **Blueprint Function Libraries** - Direct in-editor integration
2. **TCP Commands** - External client communication

This guide helps you choose the right approach for your workflow.

Overview Comparison
--------------------

+------------------+---------------------------+---------------------------+
| Aspect           | Blueprint Libraries        | TCP Commands              |
+------------------+---------------------------+---------------------------+
| Access           | UE Editor/Game            | External (Python, etc.)   |
| Latency          | Near-instant              | Network round-trip        |
| Complexity       | High-level APIs           | Low-level commands        |
| Setup            | No network required       | Requires TCP connection   |
| Best for         | UMG UI, Editor workflows  | External automation       |
+------------------+---------------------------+---------------------------+

Blueprint Libraries
------------------

Blueprint Function Libraries are C++ classes with ``UFUNCTION`` macros
exposing functions directly to the Blueprint visual scripting system.
They're called synchronously from within the editor or game.

**Advantages:**

- No network overhead - direct function calls
- Integration with UMG widgets
- Works in packaged builds
- Type-safe with compile-time checks
- Complex return types (structs, objects)

**Disadvantages:**

- Only works within UE Editor/Packaged game
- Cannot control remote instances
- Requires C++ compilation for new functions

**Example: Start Recording**

.. code-block:: cpp

   // C++ API
   UCLASS()
   class UNREALCV_API URecordingBPLib : public UBlueprintFunctionLibrary {
       GENERATED_BODY()
   public:
       UFUNCTION(BlueprintCallable, Category = "UnrealCV|Recording")
       static bool StartRecording(
           int32 CameraID,
           const FString& FileName,
           ERecordingType Type,
           const TArray<FString>& TrajectoryTypes
       );
   };

**In Blueprint:**

1. Add "Call Function" node
2. Select URecordingBPLib > StartRecording
3. Connect CameraID, FileName, Type, TrajectoryTypes
4. Get IsRecording return value

**In C++:**

.. code-block:: cpp

   bool bSuccess = URecordingBPLib::StartRecording(
       CameraID,
       TEXT("output.mp4"),
       ERecordingType::Normal,
       { TEXT("orbit"), TEXT("zoom_in") }
   );

TCP Commands
------------

TCP commands are string-based messages sent to the UnrealCV server.
They're parsed by the CommandDispatcher and routed to handler functions.

**Advantages:**

- Cross-platform clients (Python, C++, MATLAB)
- Can control remote Unreal instances
- No UE recompilation needed
- Testable from command line

**Disadvantages:**

- String parsing overhead
- Limited to string/primitive return types
- Network latency
- Requires connection management

**Format:**

- ``vget /handler/subcommand [args]`` - Query/retrieve data
- ``vset /handler/subcommand [args]`` - Execute actions

**Example: Get Camera List**

.. code-block:: bash

   # Connect via TCP, then send:
   vget /cameras

   # Response:
   ok [0, 1, 2]  # Camera IDs

**Example: Set Object Location**

.. code-block:: bash

   vset /object/MyActor/location 100 200 300
   ok

Python Client Usage
-------------------

The Python client wraps TCP commands:

.. code-block:: python

   from unrealcv import UnrealCV

   # Connect to running instance
   client = UnrealCV('localhost', 9000)
   client.connect()

   # Get camera list (TCP - string response)
   cameras = client.request('vget /cameras')
   # Returns: '[0, 1, 2]'

   # For complex operations, use Blueprint via vbp command
   client.request('vbp /camera/0/startrecording type=normal')

The ``vbp`` command bridges the gap - it invokes Blueprint library functions
via TCP:

.. code-block:: bash

   vbp /camera/0/startrecording type=normal trajectory="[orbit, zoom_in]"

Command Reference Mapping
-------------------------

+---------------------------+---------------------------+---------------------------+
| Feature                   | Blueprint Library         | TCP Command               |
+---------------------------+---------------------------+---------------------------+
| Recording                 | URecordingBPLib           | /camera/*                 |
| Scene Generation          | USceneCompositionBPLib    | /datasetautomation/*      |
| Annotation                | UAnnotationBPLib          | /object/* (colors)        |
| Light Control             | ULightBPLib               | /light/*                  |
| Animation                 | UAnimationBPLib           | /action/*                 |
| Pak Management            | UPakMountBPLib            | /pak/*                    |
| Navigation                | UNavigationBPLib          | /agentnav/*               |
| Object Manipulation       | - (via BPLib or C++)      | /object/*                 |
| Camera Control            | USensorBPLib              | /camera/*                 |
+---------------------------+---------------------------+---------------------------+

Decision Guide
--------------

**Use Blueprint Libraries when:**

- Building UMG interfaces for users
- Working within the UE Editor
- Need complex return types (structs, arrays)
- Low latency is critical
- Controlling a single local instance

**Use TCP Commands when:**

- Building external Python/MATLAB tools
- Need to control remote instances
- Integrating with existing automation pipelines
- Testing from outside UE
- Sharing control across multiple processes

**Use vbp (TCP to Blueprint bridge) when:**

- Need Blueprint API from external client
- Complex parameters that are cumbersome as raw strings
- Accessing high-level workflows from Python

Workflow Examples
----------------

**In-Editor Recording (Blueprint)**

1. User clicks "Record" button in UMG
2. UMG calls URecordingBPLib::StartRecording
3. FusionCamCaptureActor created
4. Recording runs in background
5. User clicks "Stop" to end

**External Python Automation (TCP)**

1. Python script connects via TCP
2. Sends vset /action/start
3. Loops through object list with vget /object/.../location
4. Sends vset /action/stop when done
5. Processes collected data

**Hybrid Approach (vbp)**

1. Python connects via TCP
2. Sends vbp /datasetautomation/run task=my_task
3. UE executes entire workflow via Blueprint libraries
4. Returns completion status
5. Python retrieves generated files

Common Patterns
---------------

**Get Status: TCP for speed, Blueprint for detail**

.. code-block:: bash

   # Quick TCP check
   vget /camera/0/recordingstatus
   # Returns: "recording" or "idle"

   # Full status via Blueprint
   vbp /camera/0/getrecordingstatus
   # Returns: struct with frame count, duration, etc.

**Async Operations: Use callbacks**

Blueprint libraries support delegate callbacks for async operations:

.. code-block:: cpp

   UFUNCTION(BlueprintCallable, Category = "UnrealCV|Recording")
   static void StartRecordingAsync(
       int32 CameraID,
       const FString& FileName,
       FRecordingCompleteDelegate OnComplete
   );

**Error Handling**

TCP: Check response prefix

.. code-block:: python

   response = client.request('vget /camera/999/lit')
   if response.startswith('error'):
       print(f"Camera not found: {response}")
   else:
       process_image(response)

Blueprint: Use return values and exceptions

.. code-block:: cpp

   if (URecordingBPLib::IsRecording(CameraID))
   {
       // Already recording
       return;
   }
   bool bSuccess = URecordingBPLib::StartRecording(...);

Performance Considerations
--------------------------

**Blueprint overhead:** ~0.001ms per call (function call)

**TCP overhead:** ~1-10ms per call (network round-trip)

For 400K dataset generation, prefer Blueprint for:
- Recording control (frequent calls)
- Scene composition (complex operations)

Use TCP for:
- Initial setup
- Status monitoring
- Data collection

Summary
-------

- Blueprint = Editor integration, type-safety, performance
- TCP = External control, flexibility, remote access
- vbp = Best of both worlds for Python clients
