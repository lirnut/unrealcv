Recording System Architecture
==========================

Recording system architecture showing FusionCamSensor orchestration
through 3-phase async pipeline to encoded output.

System Overview
--------------

::

   ┌──────────────────────────────────────────────────────────────────────────┐
   │                           Recording System                                │
   └──────────────────────────────────────────────────────────────────────────┘

   ┌──────────────────┐                    ┌──────────────────────────────┐
   │   Camera Handler  │                    │  FusionCamCaptureActor      │
   │                  │                    │  (Per Recording Session)     │
   │ TMap<CID, Actor> │──▶ Create ──────▶│                              │
   └──────────────────┘                    │  - Manages sensor lifecycle │
                                           │  - Handles file encoding    │
                                           │  - Writes metadata JSON     │
                                           └──────────────────────────────┘
                                                     │
   ┌─────────────────────────────────────────────────┘
   │
   ▼
   ┌──────────────────────────────────────────────────────────────────────────┐
   │                    FusionCamSensor Orchestrator                          │
   └──────────────────────────────────────────────────────────────────────────┘

   ┌─────────────────────────────────────────────────────────────────────────┐
   │                              Sensors                                     │
   └─────────────────────────────────────────────────────────────────────────┘

   ┌───────────┐ ┌───────────┐ ┌───────────┐ ┌───────────┐ ┌───────────┐
   │    Lit    │ │   Depth   │ │  Normal   │ │   Flow    │ │   Mask    │
   │  Sensor   │ │  Sensor   │ │  Sensor   │ │  Sensor   │ │  Sensor   │
   │  (RGB)    │ │  (F16)    │ │ (FColor)  │ │ (FColor)  │ │ (FColor)  │
   └─────┬─────┘ └─────┬─────┘ └─────┬─────┘ └─────┬─────┘ └─────┬─────┘
         │              │              │              │              │
         └──────────────┴──────────────┴──────────────┴──────────────┘
                                   │
                                   ▼
                        ┌────────────────────┐
                        │   Multi-Pass       │
                        │   Rendering        │
                        └────────────────────┘

3-Phase Async Pipeline
--------------------

::

   Frame N                    Frame N+1                  Frame N+2
   ─────────                  ──────────                 ──────────
   ┌─────────────┐           ┌─────────────┐           ┌─────────────┐
   │ Game Thread │           │ Render      │           │ Game Thread │
   │             │           │ Thread      │           │             │
   │ 1. Validate │    ──▶   │ 2. GPU      │    ──▶   │ 3. File I/O│
   │ 2. Enqueue  │           │    Readback │           │    (PNG/MP4)│
   │ 3. Tick     │           │             │           │             │
   └─────────────┘           └─────────────┘           └─────────────┘

Phase Details:

**Phase 1: Game Thread - Enqueue**
   - Validate recording state
   - Trigger sensor captures
   - Enqueue render command

**Phase 2: Render Thread - GPU Readback**
   - GPU renders all 5 passes
   - FRHIGPUTextureReadback captures pixel data
   - Non-blocking, parallel to game thread

**Phase 3: Game Thread - File I/O**
   - Readback ready signal
   - PNG encoding (async)
   - Disk write
   - Metadata update

Data Flow
---------

::

   Sensor Capture              GPU                          Disk
   ─────────────               ────                         ────

   ┌───────────┐          ┌─────────────┐            ┌─────────────┐
   │ Texture   │──Render──▶│ Render      │──Readback─▶│ PNG/MP4     │
   │ Render   │          │ Target      │            │ Files       │
   │ Target   │          │ Buffer      │            │             │
   └───────────┘          └─────────────┘            └─────────────┘

                          ┌─────────────┐            ┌─────────────┐
                          │ Async Task  │            │ Metadata    │
                          │ - Encode    │            │ JSON        │
                          │ - Compress  │            │             │
                          └─────────────┘            └─────────────┘

Multi-Sensor Orchestration
--------------------------

::

   ┌──────────────────────────────────────────────────────────────────┐
   │                     FusionCamSensor                              │
   └──────────────────────────────────────────────────────────────────┘

   ┌──────────────────────────────────────────────────────────────────┐
   │                    Visibility Control                            │
   └──────────────────────────────────────────────────────────────────┘

   ┌──────────────────────────────────────────────────────────────────┐
   │                    Lighting Control                              │
   └──────────────────────────────────────────────────────────────────┘

         │                        │                        │
         ▼                        ▼                        ▼
   ┌───────────┐           ┌───────────┐           ┌───────────┐
   │   Lit     │           │  Depth    │           │  Normal   │
   │  Pass     │           │  Pass     │           │  Pass     │
   │ Standard  │           │ PlaneDist │           │ View Space│
   │ Lighting  │           │ to Camera │           │           │
   └─────┬─────┘           └─────┬─────┘           └─────┬─────┘
         │                        │                        │
         └────────────────────────┼────────────────────────┘
                                  │
                                  ▼
                         ┌────────────────┐
                         │ Combined      │
                         │ Multi-Pass    │
                         │ Output        │
                         └────────────────┘
