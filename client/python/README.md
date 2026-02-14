# UnrealCV Python Client

Python client library for communicating with UnrealCV server running inside Unreal Engine.

## Installation

```bash
pip install unrealcv
```

## Quick Start

```python
from unrealcv import api

# Connect to UnrealCV server
client = api.UnrealCv_API(port=9000, ip='127.0.0.1', resolution=(640, 480))

# Get an RGB image
image = client.get_image(0, 'lit')

# Get object locations
objects = client.get_objects()
for obj in objects:
    location = client.get_obj_location(obj)
    print(f'{obj}: {location}')
```

## Connection Options

```python
# TCP connection (default)
client = api.UnrealCv_API(port=9000, ip='127.0.0.1')

# Unix socket (Linux only, faster for local connections)
client = api.UnrealCv_API(port=9000, ip='127.0.0.1', mode='unix')
```

## API Reference

### Camera Control

+---------------------------+----------------------------------------+
| Method                    | Description                            |
+---------------------------+----------------------------------------+
| ``get_image(cam_id, mode)`` | Get image from camera (lit, depth, etc.) |
| ``get_depth(cam_id)``    | Get depth image as numpy array       |
| ``set_cam_location()``   | Set camera position                  |
| ``get_cam_location()``   | Get camera position                  |
| ``set_cam_rotation()``   | Set camera rotation                  |
| ``get_cam_rotation()``   | Get camera rotation                  |
| ``set_cam_fov()``        | Set camera field of view             |
| ``get_cam_fov()``        | Get camera field of view             |
| ``get_camera_num()``     | Get number of cameras                |
| ``spawn_free_camera()``  | Create new camera                    |
+---------------------------+----------------------------------------+

### Object Manipulation

+---------------------------+----------------------------------------+
| Method                    | Description                            |
+---------------------------+----------------------------------------+
| ``get_objects()``        | List all objects in scene             |
| ``get_obj_location()``   | Get object position                   |
| ``set_obj_location()``   | Set object position                   |
| ``get_obj_rotation()``   | Get object rotation                   |
| ``set_obj_rotation()``   | Set object rotation                   |
| ``get_obj_scale()``      | Get object scale                      |
| ``set_obj_scale()``      | Set object scale                      |
| ``get_obj_color()``      | Get annotation color                  |
| ``set_obj_color()``      | Set annotation color                  |
| ``get_obj_bounds()``     | Get object bounding box                |
| ``set_hide_obj()``       | Hide object                           |
| ``set_show_obj()``       | Show object                           |
| ``destroy_obj()``        | Remove object from scene              |
| ``spawn_new_obj()``      | Spawn new object                      |
+---------------------------+----------------------------------------+

### Image Capture

**View Modes:**

- ``lit`` - RGB image
- ``depth`` - Depth map (numpy)
- ``normal`` - Normal map
- ``mask`` - Object segmentation mask
- ``optical_flow`` - Motion vectors

**Example:**

```python
# Get RGB image
rgb = client.get_image(0, 'lit')

# Get depth as numpy array
depth = client.get_depth(0)

# Get segmentation mask
mask = client.get_image(0, 'mask')

# Multi-camera capture
images = client.get_image_multicam([0, 1, 2], 'lit')

# Multi-modal capture (RGB + depth)
combined = client.get_image_multimodal(0, ['lit', 'depth'], ['bmp', 'npy'])
```

### Recording

+---------------------------+----------------------------------------+
| Method                    | Description                            |
+---------------------------+----------------------------------------+
| ``start_simple_recording()`` | Start video recording             |
| ``is_recording()``        | Check recording status                 |
| ``set_camera_fast_capture()`` | Enable fast capture mode          |
| ``get_camera_fast_capture()`` | Get fast capture mode status     |
| ``set_recording_time_dilation()`` | Slow-motion recording         |
+---------------------------+----------------------------------------+

**Example:**

```python
# Start recording
client.start_simple_recording(0, 'output_video', 30, 10.0)
# Records 300 frames at 30fps for 10 seconds

# Check status
while client.is_recording(0):
    time.sleep(0.1)

print('Recording complete')
```

### Bounding Boxes

+---------------------------+----------------------------------------+
| Method                    | Description                            |
+---------------------------+----------------------------------------+
| ``get_bbox()``           | Get bounding box for object           |
| ``get_obj_bboxes()``    | Get bounding boxes for multiple objects|
| ``build_color_dict()``   | Build color-to-object mapping         |
+---------------------------+----------------------------------------+

**Example:**

```python
# Get object mask
mask = client.get_image(0, 'mask')

# Get bounding box
bbox_mask, box = client.get_bbox(mask, 'tree')

# Get all bounding boxes
boxes = client.get_obj_bboxes(mask, ['tree', 'car', 'person'])
```

### Game Control

+---------------------------+----------------------------------------+
| Method                    | Description                            |
+---------------------------+----------------------------------------+
| ``set_pause()``          | Pause game simulation                 |
| ``set_resume()``         | Resume game simulation                |
| ``get_is_paused()``      | Check pause state                     |
| ``set_global_time_dilation()`` | Change simulation speed        |
| ``set_map()``            | Load different level                 |
+---------------------------+----------------------------------------+

### Message Decoder

The ``MsgDecoder`` class handles server responses:

```python
decoder = api.MsgDecoder()

# Decode various response types
location = decoder.string2floats("100.0 200.0 300.0")
color = decoder.string2color("(R=255,G=128,B=64)")
image = decoder.decode_png(response)
depth = decoder.decode_depth(response)
```

## Error Handling

```python
from unrealcv import api

client = api.UnrealCv_API(port=9000, ip='127.0.0.1')

try:
    image = client.get_image(0, 'lit')
except Exception as e:
    print(f'Error: {e}')
```

## Best Practices

1. **Connection**: Check connection before commands
   ```python
   client.check_connection()
   ```

2. **Batch Commands**: Use batch for multiple operations
   ```python
   cmds = ['vget /camera/0/location', 'vget /camera/1/location']
   locations = client.batch_cmd(cmds, [decoder.string2floats, decoder.string2floats])
   ```

3. **Image Formats**: Use BMP for speed, PNG for quality
   ```python
   image = client.get_image(0, 'lit', mode='bmp')  # Faster
   image = client.get_image(0, 'lit', mode='png')  # Better quality
   ```

4. **Async Commands**: Use async for non-blocking operations
   ```python
   client.set_obj_location('cube', [100, 200, 300])  # Async by default
   ```

## File Structure

```
client/python/
├── unrealcv/
│   ├── __init__.py      # Package init
│   ├── api.py           # Main API class
│   ├── launcher.py      # Server launcher
│   └── util.py          # Utilities
```

## Version Compatibility

- Python: 3.8+
- UnrealCV Server: 1.0.0+
- Dependencies: numpy, opencv-python, pillow
