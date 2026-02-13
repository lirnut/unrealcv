Camera Commands Reference (2024-2026 Additions)
==============================================

This section documents camera commands added since v0.4.0 (2024-2026).

Camera ID Format
----------------

UnrealCV supports two camera ID formats:

1. **Integer format (legacy)**: 0, 1, 2, ... (creation-order based, unstable)
2. **CID format (new)**: ``CID-ActorName-UUID`` (stable, tied to sensor instance)

New commands support both formats via ``[camera_id]`` parameter:
- ``vget /camera/0/lit lit.png`` (legacy)
- ``vget /camera/CID-FusionCameraActor-abc123/lit lit.png`` (new CID format)

For new projects, use CID format for persistent references.

Get all cameras in scene:
   - ``vget /cameras`` - List all cameras with CID format
   - ``vget /cameras_legacy`` - List all cameras with old format (deprecated)
   - ``vset /cameras/spawn`` - Spawn a new camera actor

.. _camera-capture-commands:

Capture Commands
---------------

These commands capture different data types from the camera sensor.

vget /camera/[camera_id]/lit [filename]
    Capture rendered image (RGB). Saves as PNG/BMP file.

    Example: ``vget /camera/0/lit scene.png``

vget /camera/[camera_id]/depth [filename]
    Capture depth map. Saves as NPY file with float32 depth values.

    Example: ``vget /camera/0/depth depth.npy``

vget /camera/[camera_id]/normal [filename]
    Capture surface normals. Saves as PNG/BMP with encoded normals.

    Example: ``vget /camera/0/normal normal.png``

vget /camera/[camera_id]/optical_flow [filename]
    Capture optical flow between frames. Saves as NPY.

    Example: ``vget /camera/0/optical_flow flow.npy``

    .. note::
       Requires motion between frames. Returns zero flow if camera/object static.

vget /camera/[camera_id]/seg [filename]
    Capture instance segmentation mask. Saves as PNG/BMP.

    Example: ``vget /camera/0/seg seg.png``

vget /camera/[camera_id]/object_mask [filename]
    Alias for ``/camera/[camera_id]/seg``.

vget /camera/[camera_id]/oneobjmask [filename] [object_id]
    Capture mask for a single object, excluding all other objects.

    Example: ``vget /camera/0/oneobjmask car_mask.png BP_Car_1``

    Returns clean foreground mask of only the specified object.

.. _camera-control-commands:

Camera Pose Commands
--------------------

vget /camera/[camera_id]/location
    Get camera location in world space. Returns: ``x y z``

vset /camera/[camera_id]/location [x] [y] [z]
    Teleport camera to specified location (instant, no collision).

vset /camera/[camera_id]/moveto [x] [y] [z]
    Move camera to location with collision checking.
    Camera will stop if blocked by objects.

vget /camera/[camera_id]/rotation
    Get camera rotation. Returns: ``pitch yaw roll``

vset /camera/[camera_id]/rotation [pitch] [yaw] [roll]
    Set camera rotation.

vget /camera/[camera_id]/pose
    Get camera location and rotation. Returns: ``x y z pitch yaw roll``

vset /camera/[camera_id]/pose [x] [y] [z] [pitch] [yaw] [roll]
    Set camera location and rotation in one command.

Camera Settings Commands
-----------------------

Field of View
~~~~~~~~~~~~~

vget /camera/[camera_id]/fov
    Get camera field of view (horizontal, in degrees).

vset /camera/[camera_id]/fov [fov]
    Set camera field of view. Typical values: 60-120 degrees.

Image Size
~~~~~~~~~~

vget /camera/[camera_id]/size
    Get camera image resolution. Returns: ``width height``

vset /camera/[camera_id]/size [width] [height]
    Set camera image resolution.

    Example: ``vset /camera/0/size 1920 1080``

Projection Type
~~~~~~~~~~~~~~~

vset /camera/[camera_id]/projection_type [type]
    Set camera projection type.

    Available types:
    - ``perspective`` - Standard perspective projection
    - ``orthographic`` - Orthographic projection

vset /camera/[camera_id]/ortho_width [width]
    Set orthographic camera width (only for orthographic projection).

.. _camera-rendering-commands:

Rendering Quality Commands
--------------------------

Fast Capture Mode
~~~~~~~~~~~~~~~~

vget /camera/[camera_id]/use_fast_capture
    Get fast capture mode status. Returns: ``0`` or ``1``.

vset /camera/[camera_id]/use_fast_capture [0|1]
    Enable/disable fast capture mode.

    Fast capture mode (enabled=1) uses optimized GPU readback for higher
    throughput. Recommended for recording and batch capture.

    Example: ``vset /camera/0/use_fast_capture 1``

Lit Source
~~~~~~~~~~

vset /camera/[camera_id]/lit_source [source]
    Set the capture source for lit images.

    Available sources:
    - ``ftc_hdr`` - Final Tone Curve HDR (default)
    - ``fc_hdr`` - Final Color HDR
    - ``sc_hdr`` - Scene Color HDR
    - ``scna_hdr`` - Scene Color HDR No Alpha
    - ``ldr`` - Final Color LDR
    - ``base`` - Base Color
    - ``scene_depth`` - Scene Depth
    - ``normal`` - Normal

    Example: ``vset /camera/0/lit_source ldr``

Reflection Method
~~~~~~~~~~~~~~~~

vset /camera/[camera_id]/reflection [method]
    Set reflection rendering method.

    Available methods:
    - ``none`` - No reflections
    - ``lumen`` - Lumen reflections (UE5)
    - ``screen_space`` - Screen Space Reflections (SSR)

    Example: ``vset /camera/0/reflection lumen``

Global Illumination
~~~~~~~~~~~~~~~~~~~

vset /camera/[camera_id]/illumination [method]
    Set global illumination method.

    Available methods:
    - ``none`` - No GI
    - ``lumen`` - Lumen GI (UE5)
    - ``screen_space`` - Screen Space GI
    - ``plugin`` - Plugin-based GI

    Example: ``vset /camera/0/illumination lumen``

.. _camera-exposure-commands:

Exposure Commands
-----------------

vset /camera/[camera_id]/exposure_method [method]
    Set auto exposure method.

    Available methods:
    - ``histogram`` - Histogram-based exposure
    - ``basic`` - Basic exposure
    - ``manual`` - Manual exposure (use exposure_bias)

vset /camera/[camera_id]/exposure_bias [bias]
    Set exposure compensation bias for manual mode.

vset /camera/[camera_id]/auto_speed [speed_down] [speed_up]
    Set auto-exposure speed adjustments.

vset /camera/[camera_id]/auto_brightness [min] [max]
    Set auto-exposure brightness range.

vset /camera/[camera_id]/physical_exposure [0|1]
    Enable/disable physical camera exposure.

.. _camera-effects-commands:

Effects Commands
----------------

Motion Blur
~~~~~~~~~~~

vset /camera/[camera_id]/motion_blur [amount] [max] [per_object] [fps]
    Configure motion blur parameters.

    Parameters:
    - ``amount`` - Blur intensity (0.0-1.0)
    - ``max`` - Maximum blur distortion
    - ``per_object`` - Per-object blur amount
    - ``fps`` - Motion blur simulation FPS

Focus
~~~~~

vset /camera/[camera_id]/focal [distance] [range]
    Set camera focus parameters.

    Parameters:
    - ``distance`` - Focus distance (units)
    - ``range`` - Focus range

.. _camera-panoramic-commands:

Panoramic Camera Commands
-------------------------

vget /panoramic/spawn [x] [y] [z]
    Spawn panoramic camera at specified location.

vget /panoramic/spawn [x] [y] [z] [resolution]
    Spawn panoramic camera with custom cubemap resolution.

vget /panoramic/capture [filename]
    Capture equirectangular panorama to file.

vget /panoramic/capture [filename] [width] [height]
    Capture panorama with custom output resolution.

    Example::

        vget /panoramic/spawn 0 0 100 2048
        vget /panoramic/capture panorama.png 4096 2048

.. _camera-viewmode-commands:

View Mode Commands
------------------

vset /viewmode [viewmode]
    Set view mode for viewport rendering.

    Available view modes:
    - ``lit`` - Final rendered image
    - ``normal`` - Surface normals
    - ``depth`` - Depth visualization
    - ``object_mask`` - Instance segmentation

vget /viewmode
    Get current view mode.

.. _camera-deprecated-commands:

Deprecated Commands
-------------------

The following commands are deprecated but still functional:

vget /camera/[id]/horizontal_fieldofview
    Deprecated. Use ``vget /camera/[id]/fov`` instead.

vset /camera/[id]/horizontal_fieldofview [FOV]
    Deprecated. Use ``vset /camera/[id]/fov [FOV]`` instead.

vget /cameras_legacy
    Deprecated. Use ``vget /cameras`` instead.
