UnrealCV Documentation Index
===========================

This is the master index for UnrealCV documentation.

Getting Started
--------------

- **Installation** - Plugin setup guide
- **Quick Start** - Your first UnrealCV commands
- **Tutorial: Dataset Generation** - Complete workflow guide

Reference
---------

**Commands:**

- `reference/commands.rst` - Complete command reference
- `reference/camera-commands-new.rst` - New camera commands
- `reference/light-commands.rst` - Light control
- `reference/agent-nav-commands.rst` - Navigation agents
- `reference/dataset-automation-commands.rst` - Batch generation
- `reference/pak-commands.rst` - Pak file management
- `reference/sensor-data-formats.rst` - Data format encoding
- `reference/metadata-schema.rst` - Recording metadata JSON

**Blueprint API:**

- `api/recording-bplib.rst` - Recording functions
- `api/scene-composition-bplib.rst` - Scene generation
- `api/dataset-automation-bplib.rst` - Batch automation
- `api/blueprint-libraries.md` - All Blueprint libraries

Architecture
------------

- `architecture/recording-pipeline.rst` - Recording system
- `architecture/sensor-system.rst` - Camera sensors
- `architecture/annotation-system.rst` - Object annotation
- `architecture/asset-pool.rst` - Asset management

Tutorials
---------

- `tutorials/blueprint-vs-tcp.md` - API comparison
- `tutorials/trajectory-recording.md` - Camera movements
- `tutorials/pak-workflow.md` - Dynamic assets
- `tutorials/dataset-generation.rst` - Dataset workflow

Migration
---------

- `migration/camera-id-format.rst` - CID format migration

Guides
------

- `guides/performance-optimization.md` - Speed optimization

Integration
-----------

- `integration/gym-unrealcv.md` - RL environments

Diagrams
--------

- `diagrams/dataset-pipeline.md` - Data flow
- `diagrams/recording-architecture.md` - Recording system
- `diagrams/annotation-decision.md` - Mode selection
- `diagrams/command-dispatch.md` - Command flow

Other
-----

- `faq.md` - Common questions
- `ue5-features.md` - UE5-specific features

Documentation Map
-----------------

::

   ┌─────────────────────────────────────────────────────────────┐
   │                    Getting Started                           │
   │         Installation → Quick Start → Tutorial               │
   └─────────────────────────────────────────────────────────────┘
                              │
                              ▼
   ┌─────────────────────────────────────────────────────────────┐
   │                       Reference                            │
   │    Commands              │              Blueprint API    │
   │    - Camera              │              - Recording      │
   │    - Object             │              - Scene          │
   │    - Light              │              - Automation      │
   │    - Dataset            │              - Libraries       │
   └─────────────────────────────────────────────────────────────┘
                              │
                              ▼
   ┌─────────────────────────────────────────────────────────────┐
   │                    Architecture                             │
   │    Recording Pipeline  │  Sensor System                  │
   │    Annotation System   │  Asset Pool                     │
   └─────────────────────────────────────────────────────────────┘
                              │
                              ▼
   ┌─────────────────────────────────────────────────────────────┐
   │                     Tutorials                               │
   │    Blueprint vs TCP  │  Trajectory Recording             │
   │    Pak Workflow      │  Dataset Generation               │
   └─────────────────────────────────────────────────────────────┘

By Use Case
-----------

**Dataset Generation:**

1. `tutorials/dataset-generation.rst` - Overview
2. `architecture/asset-pool.rst` - Assets
3. `api/scene-composition-bplib.rst` - Scene creation
4. `api/recording-bplib.rst` - Recording
5. `reference/metadata-schema.rst` - Output format
6. `guides/performance-optimization.md` - Speed

**Adding New Commands:**

1. `reference/commands.rst` - Existing patterns
2. `diagrams/command-dispatch.md` - How commands work
3. CLAUDE.md - Code conventions

**UE5 Integration:**

1. `ue5-features.md` - UE5 features
2. `migration/camera-id-format.rst` - CID format
3. `reference/sensor-data-formats.rst` - Data formats

**Debugging:**

1. `faq.md` - Common issues
2. `reference/commands.rst` - Command reference
3. CLAUDE.md - Architecture overview

External Resources
-----------------

- **GitHub:** https://github.com/unrealcv/unrealcv
- **PyPI:** https://pypi.org/project/unrealcv/
- **UnrealZoo (RL):** https://github.com/UnrealZoo/unrealzoo-gym
- **Paper:** UnrealCV: Virtual Worlds for Computer Vision (ACM MM 2017)

Version Compatibility
--------------------

+---------------------------+--------+
| Feature                   | UE Ver |
+---------------------------+--------+
| Lumen GI                 | 5.0+   |
| Nanite                   | 5.0+   |
| Groom Physics            | 5.0+   |
| Virtual Shadow Maps      | 5.0+   |
| CID Camera IDs           | 2024+   |
| Async GPU Readback      | 2025+   |
+---------------------------+--------+

Documentation Updates
--------------------

This documentation is maintained in the UnrealCV repository.
Submit PRs for corrections or additions.

Last Updated: 2026-02-13
