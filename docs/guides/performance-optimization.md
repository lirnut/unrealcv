Performance Optimization Guide
===========================

This guide covers performance optimization techniques for UnrealCV,
focused on achieving high-throughput dataset generation (400K+ videos).

Overview
--------

UnrealCV provides several optimization pathways:

1. **Async GPU Readback** - Non-blocking pipeline
2. **Fast Capture Mode** - Reduced rendering overhead
3. **Annotation Mode Selection** - Direct vs Proxy
4. **Sensor Configuration** - Optimal settings per use case

Async GPU Readback Pipeline
--------------------------

Traditional capture blocks the game thread while waiting for GPU:

::

   Game Thread:  [Capture] -> [Wait GPU] -> [Process] -> [Wait]
                   50ms          20ms          10ms        Total: 80ms

Async readback uses a 3-phase pipeline:

::

   Frame N:      Render -> Enqueue Readback
   Frame N+1:    Render -> GPU Readback (in parallel)
   Frame N+2:    Game Thread: Readback Ready -> File I/O

**Result: 30-50% throughput improvement**

**Components:**

- ``FRHIGPUTextureReadback`` - GPU-side buffer
- ``AsyncTask`` - Game thread file I/O
- Non-blocking pipeline between frames

**Usage:**

The async pipeline is automatic when using:

- ``URecordingBPLib::StartRecording()``
- ``FusionCamCaptureActor`` recording

**Configuration:**

.. code-block:: cpp

   // In BaseCameraSensor
   void SetUseFastCapture(bool bInUseFast) { bUseFastCapture = bInUseFast; }

Capture Modes
------------

+--------------------+----------------+----------------+----------------+
| Mode               | Latency        | Throughput     | Use Case       |
+--------------------+----------------+----------------+----------------+
| Sync Capture       | High (blocks)  | Low            | Debug, single  |
| Fast Capture       | Low            | High           | Recording      |
| Async Readback     | Very Low       | Highest        | Batch, 400K    |
| MVRC Sync Capture  | Medium         | Medium         | Main viewport  |
+--------------------+----------------+----------------+----------------+

**Sync Capture (Legacy):**

.. code-block:: cpp

   void Capture(TArray<FColor>& ImageData, int& Width, int& Height);

Blocks game thread. Use only for debugging.

**Fast Capture:**

.. code-block:: cpp

   void CaptureFast(TArray<FColor>& ImageData, int& Width, int& Height);
   void CaptureFastToFile(const FString& Filename);

Non-blocking render command, but still waits for completion.

**Async Readback:**

Automatic with ``StartRecording()``. Configure via:

.. code-block:: cpp

   // In FusionCamCaptureActor
   MaxInFlight = 4;  // Frames in flight (default)

Higher MaxInFlight = better parallelism, more memory.

**MainViewportRenderComponent Sync Capture:**

For capturing main game viewport, MVRC provides configurable sync mode:

.. code-block:: python

   # Enable synchronous capture for main viewport
   client.request('vset /mvrc/use_sync_capture 1')

   # Disable (use async mode, default)
   client.request('vset /mvrc/use_sync_capture 0')

When enabled, ``CaptureFrame()`` automatically redirects to ``CaptureFrameSync()``
for immediate GPU readback. Use for frame-accurate captures from main viewport.

Annotation Mode Performance
--------------------------

Two annotation strategies with different performance profiles:

**Direct Annotator:**

- Each actor gets unique material instance
- O(N) draw calls for N annotated objects
- Better for: Small object counts (< 50)

**Proxy Annotator:**

- Single post-process pass
- O(1) draw calls
- Better for: Large object counts (> 50)

**Selection Guide:**

+---------------------------+----------------+----------------+
| Scenario                  | Mode           | Performance    |
+---------------------------+----------------+----------------+
| < 50 objects             | Direct         | Better quality |
| 50-200 objects           | Proxy          | Better FPS     |
| > 200 objects            | Proxy          | Required       |
| Real-time annotation     | Proxy          | Required       |
| High-quality segmentation | Direct         | Better masks   |
+---------------------------+----------------+----------------+

**Switching Modes:**

.. code-block:: cpp

   // Via ObjectAnnotator static facade
   FObjectAnnotator::SetAnnotationMode(EAnnotationMode::Direct);

Sensor Configuration
--------------------

**Resolution Trade-offs:**

+-------------+----------------+----------------+----------------+
| Resolution  | Memory/Frame  | Capture Time   | Quality        |
+-------------+----------------+----------------+----------------+
| 480p        | 0.9 MB        | Baseline       | Draft          |
| 720p        | 2.1 MB        | +40%          | Preview        |
| 1080p       | 4.4 MB        | +100%         | Production     |
| 4K          | 17.6 MB       | +400%         | Ultra-res      |
+-------------+----------------+----------------+----------------+

**Recommended Settings for SOW:**

- 480p (854x480) for dataset generation
- 720p for preview/validation
- Capture only needed layers (not all 5)

**Layer Selection:**

.. code-block:: cpp

   // Only capture needed layers
   FusionCamSensor->SetCaptureLit(true);
   FusionCamSensor->SetCaptureMask(true);   // Skip if not needed
   FusionCamSensor->SetCaptureNormal(false); // Skip if not needed
   FusionCamSensor->SetCaptureDepth(false);  // Skip if not needed
   FusionCamSensor->SetCaptureFlow(false);  // Skip if not needed

400K Dataset Generation Best Practices
------------------------------------

**1. Maximize Async Throughput**

.. code-block:: cpp

   // Increase in-flight captures
   AFusionCamCaptureActor* Actor = /* ... */;
   Actor->MaxInFlight = 8;  // More parallelism

**2. Minimize Per-Frame Overhead**

- Disable unused sensors
- Lower annotation quality if not needed
- Use smaller render targets

**3. Optimize Scene**

- Reduce dynamic lights
- Bake static lighting where possible
- Limit post-processing

**4. Batch Operations**

- Queue multiple recordings
- Use DatasetAutomationBPLib for orchestration
- Avoid per-frame TCP commands

**5. File I/O Strategy**

- Use SSD for output
- Consider RAM disk for temp files
- Parallel writes where possible

**6. Monitoring**

Track these metrics:

- Frames per second during capture
- GPU memory usage
- Pending readback count
- File I/O queue depth

Benchmark Results
----------------

*Typical performance on 16-core system (DebugGame):*

+---------------------------+----------------+----------------+
| Configuration             | FPS            | Notes          |
+---------------------------+----------------+----------------|
| Sync capture, 1 layer    | 5-10          | Baseline       |
| Async, 1 layer           | 15-25         | +150%          |
| Async, 3 layers          | 10-18         | +80%           |
| Async, 5 layers          | 8-15          | +50%           |
| Proxy annotator (200 objs)| +40%         | vs Direct      |
| 480p vs 1080p            | +220%         | Resolution     |
+---------------------------+----------------+----------------+

Performance Checklist
--------------------

**Before dataset generation:**

- [ ] Use async capture (default in recording)
- [ ] Select only needed sensor layers
- [ ] Choose appropriate annotation mode
- [ ] Configure resolution for output quality
- [ ] Increase MaxInFlight for parallelism
- [ ] Ensure SSD storage for output

**During capture:**

- [ ] Monitor FPS (target: >10 for 30fps output)
- [ ] Watch GPU memory usage
- [ ] Check readback queue depth
- [ ] Validate output file sizes

**If performance degrades:**

- [ ] Reduce number of active sensors
- [ ] Switch to Proxy annotation
- [ ] Lower resolution
- [ ] Reduce scene complexity
- [ ] Check for memory leaks

Common Issues
------------

**Low FPS during capture:**

- Disable unused sensor layers
- Reduce scene complexity
- Use Proxy annotation for many objects

**GPU memory exhaustion:**

- Reduce MaxInFlight
- Lower resolution
- Disable unused layers

**Readback stalls:**

- Increase MaxInFlight
- Ensure SSD storage
- Check for GPU bottlenecks

**File I/O bottleneck:**

- Use faster storage
- Reduce capture resolution
- Batch file operations
