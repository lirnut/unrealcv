#!/usr/bin/env python3
"""
Simple closed-loop test runner
"""
import sys
import subprocess
import socket
import time
import os
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
PLUGIN_ROOT = SCRIPT_DIR.parent
CLIENT_PYTHON_DIR = PLUGIN_ROOT / "Source" / "uezoo"

if CLIENT_PYTHON_DIR.exists():
    sys.path.insert(0, str(CLIENT_PYTHON_DIR))

import unrealcv

UE_PATH = Path("H:/UE_5.6/Engine")
PROJECT_PATH = Path("G:/HUAWEI_Project_UE56/HUAWEI_Project.uproject")
EXE_PATH = PROJECT_PATH.parent / "Binaries" / "Win64" / "HUAWEI_Project.exe"

PORT = 9000
TIMEOUT = 180

def find_build_tool():
    patterns = [
        "Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll",
        "Build/BatchFiles/RunUAT.bat",
    ]
    for pattern in patterns:
        path = UE_PATH / pattern
        if path.exists():
            return path
    return None

def compile_project():
    build_tool = find_build_tool()
    if not build_tool:
        print("Build tool not found")
        return False

    cmd = [
        str(build_tool),
        "HUAWEI_Project",
        "Win64",
        "Development",
        f"-Project={PROJECT_PATH}",
        "-WaitMutex",
        "-FromMsBuild",
        "-architecture=x64",
    ]

    print(f"Compiling: {' '.join(cmd)}")
    result = subprocess.run(cmd, cwd=str(UE_PATH.parent), capture_output=True, text=True, timeout=600)
    if result.returncode == 0:
        print("Build successful")
        return True
    else:
        print(f"Build failed: {result.stderr[:500]}")
        return False

def start_game():
    if not EXE_PATH.exists():
        print(f"Executable not found: {EXE_PATH}")
        return None

    env = os.environ.copy()
    env["UE-CV-PORT"] = str(PORT)

    cmd = [
        str(EXE_PATH),
        f"-Port={PORT}",
        "-Log",
        "-NoSplash",
        "-NoPause",
        "-FullStdOutLogOutput",
    ]

    print(f"Starting game: {' '.join(cmd)}")
    proc = subprocess.Popen(
        cmd,
        cwd=str(EXE_PATH.parent),
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1,
    )
    return proc

def wait_for_server(proc, timeout):
    start_time = time.time()
    while time.time() - start_time < timeout:
        poll = proc.poll()
        if poll is not None:
            output = proc.stdout.read()
            print(f"Game exited with code {poll}")
            print(f"Last output:\n{output[-2000:]}")
            return False

        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(1)
            result = sock.connect_ex(("127.0.0.1", PORT))
            sock.close()
            if result == 0:
                print(f"Server ready on port {PORT}")
                return True
        except Exception:
            pass

        time.sleep(2)

        if int(time.time() - start_time) % 10 == 0:
            output = proc.stdout.read()
            if output:
                lines = output.strip().split('\n')
                for line in lines[-5:]:
                    if 'UnrealCV' in line or 'error' in line.lower():
                        print(f"  [LOG] {line}")

    print(f"Timeout waiting for server")
    return False

def run_tests():
    client = unrealcv.Client(("127.0.0.1", PORT))
    if not client.connect(timeout=10):
        print("Failed to connect")
        return False

    print("Connected!")

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
            res = client.request(cmd)
            if res and not res.startswith("error"):
                print(f"  [PASS] {name}")
                passed += 1
            else:
                print(f"  [FAIL] {name}: {res}")
        except Exception as e:
            print(f"  [FAIL] {name}: {e}")

    client.disconnect()
    print(f"\nResult: {passed}/{len(tests)} tests passed")
    return passed == len(tests)

def main():
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("--compile", action="store_true")
    parser.add_argument("--skip-compile", action="store_true")
    args = parser.parse_args()

    game_proc = None

    try:
        if args.compile:
            if not compile_project():
                return 1

        if not args.skip_compile:
            game_proc = start_game()
            if not game_proc:
                return 1
            if not wait_for_server(game_proc, TIMEOUT):
                return 1

        if not run_tests():
            return 1

        return 0

    finally:
        if game_proc:
            print("Stopping game...")
            game_proc.terminate()
            try:
                game_proc.wait(timeout=10)
            except subprocess.TimeoutExpired:
                game_proc.kill()
                game_proc.wait()

if __name__ == "__main__":
    sys.exit(main())
