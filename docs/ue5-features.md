UE5-Specific Features
===================

This guide covers UnrealCV features specific to Unreal Engine 5.

UE5 Compatibility
----------------

UnrealCV supports UE 5.2 through 5.6 with these major features:

+---------------------------+----------------------------------------+
| Feature                   | Minimum UE Version                     |
+---------------------------+----------------------------------------+
| Lumen GI                  | UE 5.0                                 |
| Nanite                    | UE 5.0                                 |
| Groom/Hair                | UE 5.0                                 |
| Virtual Shadow Maps       | UE 5.0                                 |
+---------------------------+----------------------------------------+

Lumen Global Illumination
-------------------------

UE5 introduces Lumen for real-time global illumination.

**Available Methods:**

+---------------------------+----------------------------------------+
| Method                    | Description                            |
+---------------------------+----------------------------------------+
| ``none``                  | No GI                                 |
| ``lumen``                 | Lumen GI (UE5 default)                |
| ``screen_space``          | Screen Space GI (fallback)            |
| ``plugin``                | Plugin-based GI                      |
+---------------------------+----------------------------------------+

**Commands:**

.. code-block:: bash

   # Set GI method
   vset /camera/0/illumination lumen

   # Check current setting
   vget /camera/0/illumination
   ok lumen

**Capture Considerations:**

- Lumen requires GPU with ray tracing support
- Can be disabled for faster captures
- Affects lit sensor output quality

Reflection Methods
----------------

**Available Methods:**

+---------------------------+----------------------------------------+
| Method                    | Description                            |
+---------------------------+----------------------------------------+
| ``none``                  | No reflections                        |
| ``lumen``                 | Lumen reflections (UE5)              |
| ``screen_space``          | Screen Space Reflections (SSR)         |
+---------------------------+----------------------------------------+

**Commands:**

.. code-block:: bash

   # Set reflection method
   vset /camera/0/reflection lumen

   # Check current setting
   vget /camera/0/reflection
   ok lumen

Nanite Support
-------------

Nanite is UE5's virtualized geometry system.

**For Dataset Generation:**

- Nanite automatically enabled for imported meshes
- Works with FusionCamSensor capture
- No special configuration needed

**Limitations:**

- Some post-process effects may not work
- Dynamic geometry changes may cause artifacts

Groom/Hair Physics
------------------

UE5 introduces the Groom system for hair and fur simulation.

**GroomBPLib Functions:**

+---------------------------+----------------------------------------+
| Function                  | Description                            |
+---------------------------+----------------------------------------+
| ``SetHairGravity``        | Apply custom gravity vector           |
| ``SetHairAirDrag``        | Set air resistance                    |
| ``SetHairAirVelocity``    | Apply wind velocity                   |
| ``SetHairBendDamping``    | Control bending energy loss           |
| ``SetHairBendStiffness``  | Control hair stiffness                |
| ``SetHairStrandsViscosity``| Set strand viscosity                 |
| ``DisableHairGravity``    | Disable gravity simulation            |
| ``SetHairSimulationEnabled`` | Enable/disable simulation          |
| ``ResetHairSimulation``   | Reset to initial state                |
+---------------------------+----------------------------------------+

**Example - Wind Effect:**

.. code-block:: cpp

   // Apply wind to character hair
   UGroomBPLib::SetHairAirVelocity(
       CharacterActor,
       FVector(100.0f, 50.0f, 0.0f)
   );

   // Enable simulation
   UGroomBPLib::SetHairSimulationEnabled(CharacterActor, true);

**Example - Zero Gravity:**

.. code-block:: cpp

   // Floating hair effect
   UGroomBPLib::DisableHairGravity(CharacterActor);

Virtual Shadow Maps
------------------

UE5 uses Virtual Shadow Maps for improved shadow quality.

**Configuration:**

No direct UnrealCV commands. Configure in project settings:

- Project Settings > Rendering > Virtual Shadow Maps
- Enable for high-quality shadows

**With Sensors:**

Virtual Shadow Maps work automatically with FusionCamSensor.
No additional configuration needed.

Migration from UE4
-----------------

**Key Changes:**

+---------------------------+----------------+----------------+
| Feature                   | UE4            | UE5            |
+---------------------------+----------------+----------------+
| GI                        | SSAO/Lumen    | Lumen (default)|
| Reflections               | SSR           | Lumen          |
| Hair                      | Deprecated    | Groom system   |
| Geometry                  | Standard      | Nanite         |
| Shadows                   | Cascaded      | Virtual Maps   |
+---------------------------+----------------+----------------+

**Command Mapping:**

+---------------------------+----------------+----------------+
| UE4 Command               | UE5 Equivalent |
+---------------------------+----------------+----------------|
| vset /camera/0/gi ...     | vset /camera/0/illumination ... |
| vset /camera/0/reflect    | vset /camera/0/reflection ... |
+---------------------------+----------------+----------------+

Performance Notes
----------------

**Lumen Overhead:**

- Lumen adds 10-30ms per frame
- Can be disabled for speed
- Trade-off: quality vs throughput

**Groom Overhead:**

- Hair simulation is GPU-intensive
- Disable when not needed
- Use ResetHairSimulation for static shots

**Recommended Settings:**

+---------------------------+----------------+----------------+
| Use Case                  | GI Method      | Reflections    |
+---------------------------+----------------+----------------|
| Speed priority            | none           | none           |
| Balanced                  | screen_space   | screen_space   |
| Quality priority          | lumen          | lumen          |
+---------------------------+----------------+----------------+
