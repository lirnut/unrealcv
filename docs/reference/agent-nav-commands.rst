Agent Navigation Commands
====================

This section documents agent navigation commands for autonomous agent movement (UE5.2+).

Overview
--------

UnrealCV provides navigation commands for controlling agent movement:

- **Autonomous Navigation**: Random movement within a radius
- **Target Navigation**: Navigate to specific coordinates
- **Status Query**: Get current navigation state

Agents are controlled via NavAgentController and use UE5's Navigation System
(Navigation Mesh / NavMesh) for pathfinding.

.. note::
   Agents must have a valid Navigation Mesh in the level for navigation to work.
   Ensure ``bCanNavWalking`` is set for walking agents.

TCP Commands
-----------

Autonomous Navigation
~~~~~~~~~~~~~~~~~~

vset /agent/[AgentName]/nav/start [Radius]
    Start autonomous navigation for an agent.

    Arguments:
    - ``AgentName``: Name of the actor to navigate
    - ``Radius``: Movement radius in world units

    Example: ``vset /agent/BP_Human_01/nav/start 500``

    Returns: ``Started autonomous navigation for 'BP_Human_01' with radius 500.0``

Target Navigation
~~~~~~~~~~~~~~~

vset /agent/[AgentName]/nav/goto [X] [Y] [Z]
    Navigate agent to a specific position.

    Arguments:
    - ``AgentName``: Name of the actor to navigate
    - ``X``: Target X coordinate
    - ``Y``: Target Y coordinate
    - ``Z``: Target Z coordinate

    Example: ``vset /agent/BP_Human_01/nav/goto 100 200 0``

    Returns: ``Navigating 'BP_Human_01' to location (100.00, 200.00, 0.00)``

Navigation Control
~~~~~~~~~~~~~~~~

vset /agent/[AgentName]/nav/stop
    Stop current navigation for an agent.

    Arguments:
    - ``AgentName``: Name of the actor

    Example: ``vset /agent/BP_Human_01/nav/stop``

    Returns: ``Stopped navigation for 'BP_Human_01'``

Status Query
~~~~~~~~~~

vget /agent/[AgentName]/nav/status
    Get agent navigation status.

    Arguments:
    - ``AgentName``: Name of the actor

    Returns: Navigation status in format ``mode:MODE,goal:(X,Y,Z)``

    Mode values:
    - ``autonomous`` - Random autonomous movement
    - ``totarget`` - Navigating to specific target
    - ``paused`` - Navigation paused or not started

    Example: ``vget /agent/BP_Human_01/nav/status``

    Returns: ``mode:autonomous,goal:(150.00,200.00,0.00)``

Blueprint API
------------

NavigationBPLib provides Blueprint-accessible functions.

.. function:: StartAutonomousNavigation(World, Agent, Radius) -> NavAgentController

   Start autonomous random navigation for an agent.

   :param World: World context object
   :param Agent: Actor to navigate
   :param Radius: Movement radius in units
   :return: NavAgentController or nullptr on failure

.. function:: NavigateAgentToPosition(World, Agent, TargetPos) -> NavAgentController

   Navigate agent to a specific position.

   :param World: World context object
   :param Agent: Actor to navigate
   :param TargetPos: Target location
   :return: NavAgentController or nullptr on failure

.. function:: StopNavigation(World, Agent) -> bool

   Stop agent navigation.

   :param World: World context object
   :param Agent: Actor to stop
   :return: true if stopped successfully

.. function:: GetNavController(World, Agent) -> NavAgentController

   Get the navigation controller for an agent.

   :param World: World context object
   :param Agent: Actor to check
   :return: NavAgentController or nullptr if not navigating

Navigation Status Values
----------------------

The ``vget /agent/[AgentName]/nav/status`` command returns:

``mode:autonomous,goal:(X,Y,Z)``
    Agent is moving randomly within navigation bounds.

``mode:totarget,goal:(X,Y,Z)``
    Agent is navigating to a specific target location.

``mode:paused,goal:(X,Y,Z)``
    Navigation is paused or agent reached destination.

Workflow Example
--------------

**Start autonomous movement**

::

    vset /agent/BP_Human_01/nav/start 300

    # Check status
    vget /agent/BP_Human_01/nav/status

**Navigate to specific location**

::

    # Stop current navigation
    vset /agent/BP_Human_01/nav/stop

    # Navigate to target
    vset /agent/BP_Human_01/nav/goto 500 300 0

    # Monitor progress
    vget /agent/BP_Human_01/nav/status

**Stop navigation**

::

    vset /agent/BP_Human_01/nav/stop

Common Issues
------------

**Navigation fails to start**

- Ensure Navigation Mesh exists in the level
- Check agent has valid NavAgentProperties
- Verify agent is not static or blocked

**Agent doesn't reach target**

- Check for navigation mesh gaps
- Ensure target is on NavMesh surface
- Check for pathfinding obstacles

**Autonomous navigation stays in one area**

- Increase radius parameter
- Check NavMesh bounds
- Ensure multiple NavMesh areas are connected

See Also
--------

- :doc:`../guides/scene-composition` - Scene generation with agents
- :doc:`dataset-automation-commands` - Dataset automation with navigation
- Navigation System documentation in UE5 docs
