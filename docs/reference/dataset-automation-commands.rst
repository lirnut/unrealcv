Dataset Automation Commands
=========================

This section documents batch dataset generation commands for automated
trajectory recording and scene generation (UE5.2+).

Overview
--------

The Dataset Automation system provides TCP commands for:

- **Batch trajectory recording**: Automated camera trajectory capture
- **Scene parameter configuration**: Foreground, occluder, camera settings
- **Command queue execution**: JSON-based command sequences
- **Progress tracking**: Real-time status and command history

This system is the core of the 400K+ layered video dataset generation pipeline.

.. _datasetautomation-task-commands:

Task Management Commands
-----------------------

vget /datasetautomation/task_name
    Get current task type.

    Returns: ``Trajectory`` or ``Omnimatte``

vset /datasetautomation/task_name [TaskType]
    Set current task type.

    Arguments:
    - ``TaskType``: ``Trajectory`` or ``Omnimatte``

    Example: ``vset /datasetautomation/task_name Trajectory``

.. _datasetautomation-sequence-commands:

Command Sequence Commands
-------------------------

vset /datasetautomation/sequence [JSON]
    Set command sequence from JSON string.

    The JSON format defines a sequence of commands to execute:

    .. code-block:: json

        {
            "commands": [
                {"command": "spawn_foreground", "category": "Foreground_Human"},
                {"command": "spawn_occluder", "category": "Occluder_Building"},
                {"command": "move_camera", "trajectory": "orbit_360"}
            ]
        }

    Returns: ``Command sequence parsed successfully`` or error message

vget /datasetautomation/sequence
    Get current command queue summary.

    Returns: Summary of queued commands

vget /datasetautomation/history
    Get command execution history with timestamps.

    Returns: Multi-line output showing executed commands

.. _datasetautomation-control-commands:

Automation Control Commands
--------------------------

vset /datasetautomation/start [MapName]
    Start batch dataset generation.

    Arguments:
    - ``MapName``: Level/map name to load (optional)

    Returns: ``Started: N scenes to OutputDirectory``

vset /datasetautomation/stop
    Stop currently running automation.

    Returns: ``Automation stopped``

vget /datasetautomation/status
    Get current automation status.

    Returns: Current status (idle, running, completed, error)

.. _datasetautomation-scene-commands:

Scene Parameter Commands
-----------------------

Foreground Actor
~~~~~~~~~~~~~~~

vset /datasetautomation/currentscene/foreground_actor [ActorName]
    Set the foreground actor for the current scene.

    Example: ``vset /datasetautomation/currentscene/foreground_actor BP_Human_01``

Primary Camera
~~~~~~~~~~~~~

vset /datasetautomation/currentscene/primary_camera [CameraID]
    Set primary camera ID for recording.

    Example: ``vset /datasetautomation/currentscene/primary_camera 0``

Scene Category
~~~~~~~~~~~~~

vset /datasetautomation/currentscene/scene_category [Category]
    Set scene category.

    Available categories:
    - ``Scene_Indoor`` - Indoor environments
    - ``Scene_Outdoor`` - Outdoor environments
    - ``Scene_Street`` - Street scenes
    - ``Scene_Office`` - Office environments

Foreground Subcategory
~~~~~~~~~~~~~~~~~~~~

vset /datasetautomation/currentscene/foreground_subcategory [SubCategory]
    Set foreground actor subcategory.

    Example: ``Adult``, ``Child``, ``Vehicle_Car``, etc.

Occluder Category
~~~~~~~~~~~~~~~~

vset /datasetautomation/currentscene/occluder_category [Category]
    Set occluder category for occlusion relationships.

    Available categories:
    - ``Occluder_Building`` - Buildings
    - ``Occluder_Vehicle`` - Vehicles
    - ``Occluder_Object`` - Random objects
    - ``Occluder_Tree`` - Trees/vegetation

.. _datasetautomation-config-commands:

Configuration Commands
---------------------

Output Settings
~~~~~~~~~~~~~~~

vset /datasetautomation/config/total_scenes [N]
    Set total number of scenes to generate.

    Example: ``vset /datasetautomation/config/total_scenes 1000``

vset /datasetautomation/config/output_directory [Path]
    Set output directory for generated datasets.

    Example: ``vset /datasetautomation/config/output_directory D:/Datasets/``

Trajectory Settings
~~~~~~~~~~~~~~~~~~

vset /datasetautomation/config/trajectory_fps [FPS]
    Set trajectory recording FPS.

    Default: 30

vset /datasetautomation/config/trajectory_degrees_per_second [Degrees]
    Set camera rotation speed for trajectory recording.

    Default: 15.0

vset /datasetautomation/config/num_frames [N]
    Set number of frames per trajectory.

    Default: 300 (10 seconds at 30fps)

Foreground Movement
~~~~~~~~~~~~~~~~~~

vset /datasetautomation/config/foreground_move_speed [Speed]
    Set foreground actor movement speed in cm/s.

    Example: ``vset /datasetautomation/config/foreground_move_speed 50.0``

vset /datasetautomation/config/foreground_move_angle_offset [Degrees]
    Set foreground movement angle offset.

    Values:
    - ``0`` = forward
    - ``90`` = right
    - ``-90`` = left
    - ``180`` = backward

JSON Configuration
~~~~~~~~~~~~~~~~~

vset /datasetautomation/config/b_load_scene_params_from_json [true|false]
    Enable/disable loading scene parameters from JSON configuration.

    When enabled, scene parameters are loaded from an external JSON file
    instead of being set via TCP commands.

.. _datasetautomation-workflow:

Workflow Example
----------------

**Step 1: Configure task and output**

::

    vset /datasetautomation/task_name Trajectory
    vset /datasetautomation/config/total_scenes 100
    vset /datasetautomation/config/output_directory D:/Output/
    vset /datasetautomation/config/trajectory_fps 30
    vset /datasetautomation/config/num_frames 300

**Step 2: Set scene parameters**

::

    vset /datasetautomation/currentscene/scene_category Scene_Street
    vset /datasetautomation/currentscene/foreground_subcategory Adult
    vset /datasetautomation/currentscene/occluder_category Occluder_Building

**Step 3: Set optional command sequence**

::

    vset /datasetautomation/sequence {"commands": [...]}

**Step 4: Start automation**

::

    vset /datasetautomation/start

**Monitor progress**

::

    vget /datasetautomation/status
    vget /datasetautomation/history

.. _datasetautomation-status-values:

Status Values
-------------

The ``vget /datasetautomation/status`` command returns one of:

- ``Idle`` - No automation running
- ``Running`` - Currently generating scenes
- ``Paused`` - Automation paused
- ``Completed`` - All scenes generated
- ``Error`` - Error occurred (check logs)

See Also
--------

- :doc:`../guides/dataset-generation` - Complete dataset generation guide
- :doc:`../architecture/recording-pipeline` - Recording system architecture
- :doc:`camera-commands-new` - Camera commands for trajectory recording
- :doc:`scene-composition` - Scene composition and asset pooling
