Blueprint Libraries Reference
==========================

This reference covers the remaining Blueprint Function Libraries in UnrealCV,
organized by category.

AnimationBPLib
--------------

**Category:** ``UnrealCV|Animation``

Animation control for skeletal meshes and actors.

**Functions:**

+----------------------------------------+-----------------------------------+
| Function                               | Description                       |
+----------------------------------------+-----------------------------------+
| ``GetAnimationName``                    | Get current animation name        |
| ``GetAllAnimationOfSkeleton``           | Get all animation assets for bone |
| ``GetAllAnimationSequenceOfSkeleton``   | Get animation sequences only     |
| ``SetActorAnimationBlueprint``          | Assign AnimBP to actor           |
| ``SetSkeletalMeshAnimationBlueprint``  | Assign AnimBP to mesh component  |
| ``GetCurrentAnimInstance``              | Get active AnimInstance          |
| ``SetActorAnimationSequence``           | Play specific animation          |
+----------------------------------------+-----------------------------------+

**Example - Play Animation:**

.. code-block:: cpp

   UAnimationBPLib::SetActorAnimationSequence(
       CharacterActor,
       TEXT("/Game/Animations/WalkCycle")
   );

**Example - Get Active Animation:**

.. code-block:: cpp

   FString AnimName = UAnimationBPLib::GetAnimationName(
       CharacterMeshComponent
   );

GroomBPLib
----------

**Category:** ``UnrealCV|Groom``

UE5 Groom (hair/fur) physics control.

**Functions:**

+--------------------------+-----------------------------------+
| Function                 | Description                       |
+--------------------------+-----------------------------------+
| ``SetHairGravity``       | Set gravity vector for hair      |
| ``SetHairAirDrag``       | Set air drag coefficient         |
| ``SetHairAirVelocity``   | Set air velocity vector          |
| ``SetHairBendDamping``   | Set bending damping              |
| ``SetHairBendStiffness`` | Set bending stiffness            |
| ``SetHairStrandsViscosity`` | Set strands viscosity          |
| ``DisableHairGravity``    | Disable gravity simulation       |
| ``SetHairSimulationEnabled`` | Enable/disable simulation      |
| ``ResetHairSimulation``  | Reset hair to initial state      |
+--------------------------+-----------------------------------+

**Example - Wind Effect:**

.. code-block:: cpp

   // Apply wind force to hair
   UGroomBPLib::SetHairAirVelocity(
       CharacterActor,
       FVector(100, 0, 0)
   );

**Example - Disable Gravity:**

.. code-block:: cpp

   // Floating hair effect
   UGroomBPLib::DisableHairGravity(CharacterActor);

StencilBPLib
------------

**Category:** ``UnrealCV|Stencil``

Custom depth/stencil buffer control for object masking.

**Functions:**

+----------------------------+-----------------------------------+
| Function                   | Description                       |
+----------------------------+-----------------------------------+
| ``EnableCustomDepthForActor``    | Enable custom depth mask    |
| ``DisableCustomDepthForActor``    | Disable custom depth       |
| ``EnableCustomDepthForActors``    | Enable for multiple actors |
| ``DisableCustomDepthForActors``   | Disable for multiple       |
| ``SetCustomDepthStencilValue``    | Set stencil value (0-255)  |
| ``GetCustomDepthStencilValue``    | Get stencil value          |
+----------------------------+-----------------------------------+

**Use Cases:**

- Object isolation in post-processing
- Selective depth of field
- Custom stencil masks for compositing

**Example:**

.. code-block:: cpp

   // Mark actor with stencil value 5
   UStencilBPLib::EnableCustomDepthForActor(
       TargetActor,
       5  // Stencil value
   );

MaterialBPLib
-------------

**Category:** ``UnrealCV|Material``

Material override control for scene manipulation.

**Functions:**

+----------------------------+-----------------------------------+
| Function                   | Description                       |
+----------------------------+-----------------------------------+
| ``ShowOnlyActorMaterial``  | Show single actor with material  |
| ``ShowOnlyActorsMaterial`` | Show multiple actors             |
| ``RestoreAllActorMaterials`` | Restore original materials     |
| ``CacheSceneActors``       | Cache actors for material ops   |
| ``ClearCache``             | Clear cached actors/materials   |
+----------------------------+-----------------------------------+

**Example - Isolate Actor:**

.. code-block:: cpp

   // Show only target actor
   UMaterialBPLib::ShowOnlyActorMaterial(TargetActor, World);

   // ... do capture ...

   // Restore original materials
   UMaterialBPLib::RestoreAllActorMaterials();

PanoramicBPLib
--------------

**Category:** ``UnrealCV|Panoramic``

360-degree panoramic capture.

**Functions:**

+----------------------------+-----------------------------------+
| Function                   | Description                       |
+----------------------------+-----------------------------------+
| ``SpawnPanoramicCamera``   | Create panoramic sensor          |
| ``CapturePanoramicToFile``  | Save equirectangular to disk    |
| ``GetPanoramicPixelData``  | Get pixel data in-memory        |
| ``DestroyPanoramicCamera``  | Clean up sensor                 |
+----------------------------+-----------------------------------+

**Example - Capture Panorama:**

.. code-block:: cpp

   // Spawn at location
   UPanoramicCamSensor* Sensor = UPanoramicBPLib::SpawnPanoramicCamera(
       GetWorld(),
       FVector(0, 0, 100),
       1024  // Cubemap resolution
   );

   // Capture to file
   UPanoramicBPLib::CapturePanoramicToFile(
       Sensor,
       TEXT("D:/Captures/panorama.png"),
       4096,  // Equirect width
       2048   // Equirect height
   );

   // Cleanup
   UPanoramicBPLib::DestroyPanoramicCamera(Sensor);

LineTraceBPLib
--------------

**Category:** ``FusionCam|Collision``

Safe camera placement via collision sweep.

**Functions:**

+----------------------------+-----------------------------------+
| Function                   | Description                       |
+----------------------------+-----------------------------------+
| ``SolveCameraSweepSlide``  | Find safe position avoiding       |
|                            | collisions between start/end     |
+----------------------------+-----------------------------------+

**Example - Safe Camera Placement:**

.. code-block:: cpp

   FHitResult Hit;
   bool bHit = false;

   FVector SafeLocation = ULineTraceBPLib::SolveCameraSweepSlide(
       GetWorld(),
       CurrentLocation,      // Start
       DesiredLocation,      // End
       50.0f,                // Radius
       ECC_WorldStatic,
       bHit,
       Hit
   );

AutomationBPLib
---------------

**Category:** ``UnrealCV|Automation``

Command queue for timed automation sequences.

**Functions:**

+----------------------------+-----------------------------------+
| Function                   | Description                       |
+----------------------------+-----------------------------------+
| ``PushCommand``            | Queue a TCP command string       |
| ``StartTicking``           | Start processing queue           |
| ``StopTicking``           | Stop processing                 |
| ``IsTickingActive``       | Check if processing             |
+----------------------------+-----------------------------------+

**Example - Timed Sequence:**

.. code-block:: cpp

   // Queue commands
   UAutomationBPLib::PushCommand(TEXT("vset /object/actor1/visible 0"));
   UAutomationBPLib::PushCommand(TEXT("vset /object/actor2/visible 1"));
   UAutomationBPLib::PushCommand(TEXT("vget /camera/0/lit"));

   // Start processing
   UAutomationBPLib::StartTicking();

PawnBPLib
---------

**Category:** ``UnrealCV|Pawn``

Player pawn control.

**Functions:**

+----------------------------+-----------------------------------+
| Function                   | Description                       |
+----------------------------+-----------------------------------+
| ``SetPawnLocation``        | Teleport pawn                     |
| ``GetPawnLocation``       | Get current position             |
| ``SetPawnRotation``       | Set pawn orientation             |
| ``GetPawnRotation``       | Get current rotation             |
| ``GetPlayerPawn``         | Get player pawn reference        |
+----------------------------+-----------------------------------+

**Example - Position Control:**

.. code-block:: cpp

   FVector CurrentPos = UPawnBPLib::GetPawnLocation(GetWorld());

   UPawnBPLib::SetPawnRotation(
       GetWorld(),
       FRotator(0, 180, 0)
   );

SensorBPLib
-----------

**Category:** ``unrealcv``

Camera sensor ID management and resolution to CID format.

**Functions:**

+----------------------------+-----------------------------------+
| Function                   | Description                       |
+----------------------------+-----------------------------------+
| ``GetFusionSensorList``    | Get all sensor objects           |
| ``GetSensorById``          | Get by integer ID                |
| ``GetSensorByAnyID``       | Get by integer or CID string    |
| ``GetIndexByAnyID``        | Get integer index from ID string |
| ``GetFusionSensorListWithNewIDs`` | Get sensors with CID     |
| ``GetSensorNewFormatID``   | Get CID for sensor               |
| ``PrintCameraIDMappings``  | Debug print ID mappings          |
+----------------------------+-----------------------------------+

**FCameraIDManager Singleton:**

Provides stable camera identification:

.. code-block:: cpp

   // Get sensor by CID
   UFusionCamSensor* Sensor = USensorBPLib::GetSensorByAnyID(
       TEXT("CID-CameraActor-12345678")
   );

   // Get integer ID from CID
   int32 Index = USensorBPLib::GetIndexByAnyID(
       TEXT("CID-CameraActor-12345678")
   );

NavigationBPLib
----------------

**Category:** ``UnrealCV|Navigation``

NavMesh-based agent navigation control.

**Agent Control Functions:**

+----------------------------+-----------------------------------+
| Function                   | Description                       |
+----------------------------+-----------------------------------+
| ``StartAutonomousNavigation`` | Start random exploration       |
| ``NavigateAgentToPosition``  | Navigate to specific point    |
| ``StopNavigation``          | Stop active navigation         |
| ``IsNavigating``           | Check navigation state         |
| ``GetNavController``       | Get controller for agent       |
+----------------------------+-----------------------------------+

**Batch Control Functions:**

+----------------------------+-----------------------------------+
| Function                   | Description                       |
+----------------------------+-----------------------------------+
| ``StartAutonomousNavigationBatch`` | Control multiple   |
| ``StopNavigationBatch``    | Stop multiple agents             |
| ``GetAllNavControllers``   | List all controllers             |
+----------------------------+-----------------------------------+

**Utility Functions:**

+----------------------------+-----------------------------------+
| Function                   | Description                       |
+----------------------------+-----------------------------------+
| ``IsPositionReachable``    | Check if point on NavMesh        |
| ``GetRandomReachablePosition`` | Get random valid point      |
| ``ProjectPositionToNavMesh`` | Snap to nearest NavMesh point |
+----------------------------+-----------------------------------+

**Example - Autonomous Agent:**

.. code-block:: cpp

   // Start random exploration
   ANavAgentController* Controller = UNavigationBPLib::StartAutonomousNavigation(
       GetWorld(),
       AgentActor,
       1000.0f  // Radius
   );

   // Check status
   if (UNavigationBPLib::IsNavigating(GetWorld(), AgentActor))
   {
       // Agent is exploring
   }

**Example - Navigate to Target:**

.. code-block:: cpp

   ANavAgentController* Controller = UNavigationBPLib::NavigateAgentToPosition(
       GetWorld(),
       AgentActor,
       FVector(500, 500, 0)
   );

Summary Table
-------------

+--------------------+-------------------+---------------------------+
| Library            | Category          | Primary Use               |
+--------------------+-------------------+---------------------------+
| AnimationBPLib     | UnrealCV|Animation| Skeletal animation control|
| GroomBPLib         | UnrealCV|Groom    | Hair physics              |
| StencilBPLib       | UnrealCV|Stencil  | Custom depth masks        |
| MaterialBPLib      | UnrealCV|Material | Material overrides        |
| PanoramicBPLib     | UnrealCV|Panoramic| 360 capture               |
| LineTraceBPLib     | FusionCam|Collision| Safe camera placement     |
| AutomationBPLib    | UnrealCV|Automation| Command queuing           |
| PawnBPLib          | UnrealCV|Pawn     | Player control            |
| SensorBPLib        | unrealcv         | Camera ID management      |
| NavigationBPLib    | UnrealCV|Navigation| NavMesh navigation       |
+--------------------+-------------------+---------------------------+
