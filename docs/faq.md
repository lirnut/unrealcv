Frequently Asked Questions
=======================

Common issues and questions about UnrealCV.

Installation
-----------

**Q: What Visual Studio version do I need?**

A: Visual Studio version must match your UE version. For UE 5.2-5.4, use Visual Studio 2022. For UE 5.5+, use Visual Studio 2022 17.10+.

**Q: Plugin won't compile**

A: Check these:
1. UE version compatibility (5.2+ required)
2. Visual Studio version
3. Plugin is in project's Plugins folder, not Engine/Plugins
4. Clean and rebuild: ``UE5Editor.exe -projectclean``

**Q: pip install unrealcv fails**

A: Python client is separate. Install from source:

.. code-block:: bash

   pip install git+https://github.com/unrealcv/unrealcv.git

Connection
----------

**Q: Cannot connect to UnrealCV server**

A: Check:
1. UE game/editor is running
2. Plugin is enabled (check ``vget /unrealcv/status``)
3. Correct port (default 9000)
4. Firewall allows connection

**Q: Connection times out**

A: Increase timeout or check if server is responding:

.. code-block:: python

   client = unrealcv.Client(('localhost', 9000), timeout=10)

**Q: What is the default port?**

A: 9000. Can be configured in project settings.

Camera Issues
-------------

**Q: Camera ID not found**

A: Cameras are created dynamically. Use ``vget /cameras`` to list available cameras. Consider using CID format for stability.

**Q: How to get stable camera IDs?**

A: Use CID format instead of integer IDs:

.. code-block:: bash

   vget /cameras  # Returns CID strings like CID-CameraActor-12345678

**Q: Camera sensor returns empty image**

A: Check:
1. Camera has render target configured
2. Scene capture component is active
3. No occlusion by other objects

Recording
---------

**Q: Recording is slow**

A: Enable async capture and use fast mode:

.. code-block:: cpp

   Sensor->SetUseFastCapture(true);

See docs/guides/performance-optimization.md

**Q: No output file after recording**

A: Check:
1. Recording completed (not interrupted)
2. Output path is writable
3. Disk has space

**Q: Multi-layer capture not working**

A: Enable each layer explicitly:

.. code-block:: cpp

   FusionCamSensor->SetCaptureLit(true);
   FusionCamSensor->SetCaptureMask(true);
   // etc.

Object Manipulation
-------------------

**Q: Object not found**

A: Use exact object name from ``vget /objects``. Names are case-sensitive.

**Q: Object doesn't move**

A: Check:
1. Object is not static
2. Mobility is set correctly
3. Physics is enabled if needed

**Q: Object annotation color is wrong**

A: Colors are deterministic based on object ID. Use ``vget /object/{name}/color`` to get current color.

Annotation
-----------

**Q: Segmentation mask is empty**

A: Ensure annotation is enabled:

.. code-block:: cpp

   FObjectAnnotator::AnnotateWorld();

**Q: Performance issue with many objects**

A: Use Proxy annotator mode for >50 objects:

.. code-block:: cpp

   FObjectAnnotator::SetAnnotationMode(EAnnotationMode::Proxy);

Pak Files
---------

**Q: Pak file won't mount**

A: Check:
1. File path exists and is absolute
2. Pak is for correct platform (Win64 for Windows)
3. File is not in use by another process

**Q: Assets not found after mounting**

A: Run scan after mounting:

.. code-block:: bash

   vset /pak/scan /Game/MountedPath 1

**Q: AssetPool registration fails**

A: Verify:
1. AssetPoolManager is initialized
2. Category name is valid
3. Package path contains assets

UE5 Specific
------------

**Q: Lumen not working**

A: Lumen requires:
1. UE 5.0+
2. Ray tracing capable GPU
3. Lumen enabled in project settings

**Q: Groom physics not simulating**

A: Check:
1. Actor has GroomComponent
2. Simulation is enabled
3. GPU particle simulation is supported

**Q: Virtual Shadow Maps issues**

A: Virtual Shadow Maps require:
1. UE 5.0+
2. DirectX 12
3. Sufficient GPU memory

General
-------

**Q: How to debug commands?**

A: Use verbose logging:

.. code-block:: bash

   vget /unrealcv/status

**Q: Command returns error**

A: Check error message format: ``error: description``. Common causes:
- Invalid arguments
- Object not found
- Permission denied

**Q: Memory usage is high**

A: Reduce:
- Number of in-flight captures
- Render target resolution
- Active sensor layers

**Q: Where to report bugs?**

A: https://github.com/unrealcv/unrealcv/issues

Glossary
--------

+---------------------------+----------------------------------------+
| Term                      | Description                            |
+---------------------------+----------------------------------------+
| CID                       | Camera ID (new format: CID-xxx-uuid)   |
| FusionCamSensor           | Multi-pass sensor orchestrator         |
| FusionCamCaptureActor     | Recording actor                        |
| AssetPoolManager          | Runtime asset registry                 |
| Annotation                | Object segmentation coloring            |
| Trajectory               | Camera movement path                   |
| vget                     | Get/query command prefix               |
| vset                     | Set/action command prefix              |
| vbp                      | Blueprint call via TCP                 |
+---------------------------+----------------------------------------+
