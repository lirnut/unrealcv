#!/usr/bin/env python3
"""
Closed-loop test system for UnrealCV.
Uses RunUnreal from unrealcv.launcher to start game, then runs tests.
"""
import sys
import os

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PLUGIN_ROOT = os.path.dirname(SCRIPT_DIR)
CLIENT_PYTHON_DIR = os.path.join(PLUGIN_ROOT, "Source", "uezoo")

if os.path.exists(CLIENT_PYTHON_DIR):
    sys.path.insert(0, CLIENT_PYTHON_DIR)

from unrealcv import Client
from unrealcv.launcher import RunUnreal
import time


def run_tests():
    print("Running UnrealCV tests...")
    _client = Client(("127.0.0.1", 9000))
    if not _client.connect():
        print("[FAIL] Cannot connect to UnrealCV server")
        return False

    tests = [
        ("Version", "vget /unrealcv/version"),
        ("Cameras", "vget /cameras"),
        ("Camera 0 Location", "vget /camera/0/location"),
        ("Camera 0 Rotation", "vget /camera/0/rotation"),
        ("Camera 0 FOV", "vget /camera/0/fov"),
        ("Objects", "vget /objects"),
    ]

    passed = 0
    for name, cmd in tests:
        try:
            res = _client.request(cmd)
            if res and not str(res).startswith("error"):
                print(f"  [PASS] {name}")
                passed += 1
            else:
                print(f"  [FAIL] {name}: {res}")
        except Exception as e:
            print(f"  [FAIL] {name}: {e}")

    _client.disconnect()
    print(f"\nResult: {passed}/{len(tests)} tests passed")
    return passed == len(tests)


def main():
    import argparse

    parser = argparse.ArgumentParser(description="Closed-loop test for UnrealCV")
    parser.add_argument(
        "--binary",
        type=str,
        default="G:/HUAWEI_Project_UE56/Binaries/Win64/HUAWEI_Project.exe",
        help="Path to UE game binary",
    )
    parser.add_argument(
        "--offscreen",
        action="store_true",
        help="Run in offscreen mode",
    )
    parser.add_argument(
        "--nullrhi",
        action="store_true",
        help="Use null RHI (no graphics)",
    )
    parser.add_argument(
        "--sleep",
        type=int,
        default=30,
        help="Time to wait for game startup",
    )
    parser.add_argument(
        "--test-only",
        action="store_true",
        help="Skip launching game, only run tests",
    )

    args = parser.parse_args()

    launcher = None

    try:
        if not args.test_only:
            print(f"Starting game: {args.binary}")
            launcher = RunUnreal(args.binary)

            if args.offscreen:
                launcher.start(offscreen=True, nullrhi=args.nullrhi, sleep_time=args.sleep)
            else:
                launcher.start(sleep_time=args.sleep)

        success = run_tests()
        return 0 if success else 1

    finally:
        if launcher:
            print("Closing game...")
            launcher.close()


if __name__ == "__main__":
    sys.exit(main())
