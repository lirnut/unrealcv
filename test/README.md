The development of UnrealCV is supported by tests to ensure correctness.

## How to run tests

After setting up UnrealCV (game binary or editor), run basic tests to verify functionality.

**Install dependencies:**

.. code-block:: bash

   pip install -r test/requirements.txt

**Run all tests:**

.. code-block:: bash

   pytest test/ -x

**Run specific test file:**

.. code-block:: bash

   pytest test/server/camera_test.py

**Run specific test function:**

.. code-block:: bash

   pytest test/server/camera_test.py::test_camera_control

**Pytest options:**

- ``-x`` - Stop on first failure
- ``-v`` - Verbose output
- ``-s`` - Show print statements

**Requirements:**

- Game binary running, OR
- UE Editor with UnrealCV plugin in Play mode

## Test File Structure

Test scripts end with '\_test.py' suffix.

**Server tests** (``test/server/``):

+---------------------------+----------------------------------------+
| File                      | Description                            |
+---------------------------+----------------------------------------+
| ``camera_test.py``        | Camera control, sensors, recording     |
| ``object_test.py``        | Object manipulation, visibility        |
| ``connection_test.py``    | TCP connection, throughput             |
| ``stereo_test.py``        | Stereo camera tests                   |
| ``rr_test.py``            | RealisticRendering demo tests         |
| ``test_api.py``           | General API tests                     |
| ``conftest.py``           | Pytest configuration                  |
+---------------------------+----------------------------------------+

**Client tests** (``test/client/``):

+---------------------------+----------------------------------------+
| File                      | Description                            |
+---------------------------+----------------------------------------+
| ``test_client.py``        | Python client library tests            |
| ``test_dev_server.py``    | Development server tests               |
+---------------------------+----------------------------------------+

**Root test files:**

+---------------------------+----------------------------------------+
| File                      | Description                            |
+---------------------------+----------------------------------------+
| ``quick_test.py``         | Quick sanity checks                   |
| ``test_connect.py``       | Connection verification               |
| ``run_test.py``           | Test runner                           |
| ``closed_loop_test.py``   | Closed-loop automation tests           |
| ``conftest.py``           | Pytest configuration                  |
| ``requirements.txt``      | Test dependencies                     |
+---------------------------+----------------------------------------+

## Best Practices

1. Run ``quick_test.py`` first for fast verification
2. Use ``connection_test.py`` to verify TCP connectivity
3. Run camera tests before recording tests
4. Check ``-s`` output for debug information

## Docker Support

Docker can run tests automatically without manual game launch. See [unrealcv-docker-images](https://github.com/qiuwch/unrealcv-docker-images).

.. code-block:: bash

   # Verify docker setup
   python docker_util.py

   # Run with docker (requires nvidia-docker on Linux)
   pytest --docker
