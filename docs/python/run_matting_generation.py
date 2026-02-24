#!/usr/bin/env python3
import sys
import subprocess
import socket
import time
import json
import os
import psutil
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
PKG_DIR = SCRIPT_DIR / "Windows"
EXE_PATH = PKG_DIR / "HUAWEI_Project.exe"

PORT = 9000
TIMEOUT = 180

def kill_process(p):
    try:
        p.terminate()
        _, alive = psutil.wait_procs([p,], timeout=0.01)
        if len(alive):
            _, alive = psutil.wait_procs(alive, timeout=0.10)
            if len(alive):
                for p in alive: p.kill()
    except Exception as e:
        print(f"[WARN] Kill process exception: {e}")

def kill_process_and_its_children(p):
    p = psutil.Process(p.pid)
    if len(p.children()) > 0:
        for child in p.children():
            if hasattr(child, 'children') and len(child.children()) > 0:
                kill_process_and_its_children(child)
            else:
                kill_process(child)
    kill_process(p)

def start_game():
    if not EXE_PATH.exists():
        print(f"[ERROR] Executable not found: {EXE_PATH}")
        return None

    cmd = [
        str(EXE_PATH),
        "-Log",
        "-FullStdOutLogOutput",
    ]

    print(f"[INFO] Starting game on port {PORT}...")
    print(f"[CMD] {' '.join(cmd)}")

    proc = subprocess.Popen(
        cmd,
        cwd=str(EXE_PATH.parent),
        stdout=None,
        stderr=None,
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
            print(f"[ERROR] Game exited with code {poll}")
            print(f"Last output:\n{output[-2000:]}")
            return False

        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(1)
            result = sock.connect_ex(("127.0.0.1", PORT))
            sock.close()
            if result == 0:
                print(f"[OK] Server ready on port {PORT}")
                return True
        except Exception:
            pass

        time.sleep(2)

        elapsed = int(time.time() - start_time)
        if elapsed % 10 == 0 and elapsed > 0:
            print(f"[WAIT] Waiting for server... ({elapsed}s)")

    print(f"[ERROR] Timeout waiting for server")
    return False

def generate_matting_dataset(client, num_scenes=1):
    output_dir = str(PKG_DIR / "HUAWEI_Project" / "Saved" / "DatasetAutomationOutputDirectory")

    print(f"\n{'='*60}")
    print(f"Matting Dataset Generation")
    print(f"{'='*60}")
    print(f"Scenes: {num_scenes}")
    print(f"Output: {output_dir}")
    print(f"{'='*60}\n")

    print("[CONFIG] Setting up automation...")
    client.request('vset /datasetautomation/config/total_scenes 1')
    client.request(f'vset /datasetautomation/config/output_directory {output_dir}')
    client.request('vset /datasetautomation/config/trajectory_fps 30')
    client.request('vset /datasetautomation/config/num_frames 90')
    client.request('vset /datasetautomation/config/recording_options lit,oneobjlit,metadata')
    client.request('vset /datasetautomation/task_name Matting')

    command_sequence = {
        "task": "Matting",
        "commands": [
            {"cmd": "vrun", "params": "vset /captureactor/time_dilation 0.3"},
            {"cmd": "load_scene_param_json"},
            {"cmd": "random_scene_param_camera_height", "params": "120 155"},
            {"cmd": "random_scene_param_camera_angle_offset", "params": "-60 60"},
            {"cmd": "random_scene_param_camera_distance", "params": "75 100"},
            {"cmd": "create_scene"},
            {"cmd": "set_animation_bp", "params": "/Game/MetaHumans/ABP_RandomHeadMovement.ABP_RandomHeadMovement_C"},
            {"cmd": "set_animation_bp", "params": "/Game/MetaHumans/ABP_Run.ABP_Run_C"},
            {"cmd": "prepare_groom"},
            {"cmd": "sync_pawn_to_primary_camera"},
            {"cmd": "delay", "params": "5.0"},
            {"cmd": "random_resolution", "params": "1080x1920 1920x1080"},
            {"cmd": "random_fov", "params": "40 55"},
            {"cmd": "aim_camera_at_foreground", "params": "125 155"},
            {"cmd": "add_camera_rotation_noise", "params": "4.0 0.5 2.0"},
            {"cmd": "prepare_record"},
            {"cmd": "delay", "params": "10.0"},
            {"cmd": "record_trajectory", "params": "render_only"},
            {"cmd": "sync_all_cameras"},
            {"cmd": "delay", "params": "1.0"},
            {"cmd": "clear_scene"},
            {"cmd": "delay", "params": "0.5"},
            {"cmd": "increment_counter"},
            {"cmd": "check_completion"}
        ]
    }

    for scene_index in range(num_scenes):
        print(f"\n{'='*60}")
        print(f"Scene {scene_index + 1}/{num_scenes}")
        print(f"{'='*60}")

        client.request('vset /datasetautomation/stop')
        time.sleep(0.5)
        client.request('vset /datasetautomation/task_name Matting')

        seq_json = json.dumps(command_sequence)
        result = client.request(f'vset /datasetautomation/sequence {seq_json}')
        print(f"[SEQUENCE] {result}")

        result = client.request('vset /datasetautomation/start LEGACY_ARG')
        print(f"[START] {result}")

        while True:
            try:
                status = client.request('vget /datasetautomation/status')
            except AssertionError as e:
                print(f"[ERROR] TCP disconnected during status poll: {e}")
                return False

            print(f"[STATUS] {status}")

            if "Completed" in status:
                print(f"[SUCCESS] Scene {scene_index + 1} completed")
                break
            elif "Error" in status:
                print(f"[ERROR] Scene {scene_index + 1} failed: {status}")
                return False
            elif "Idle" in status:
                print(f"[ERROR] Scene {scene_index + 1} failed to start: {status}")
                return False
            else:
                print(f"State {status}")

            time.sleep(2.0)

        time.sleep(1.0)

    print(f"\n{'='*60}")
    print(f"[SUCCESS] All {num_scenes} scenes generated!")
    print(f"[OUTPUT] {output_dir}")
    print(f"{'='*60}")
    return True

def main():
    import argparse
    parser = argparse.ArgumentParser(description="Generate Matting dataset")
    parser.add_argument("--scenes", type=int, default=1, help="Number of scenes to generate")
    parser.add_argument("--skip-start", action="store_true", help="Skip starting game (connect to existing instance)")
    args = parser.parse_args()

    game_proc = None

    try:
        if not args.skip_start:
            game_proc = start_game()
            if not wait_for_server(game_proc, TIMEOUT):
                return 1
        else:
            print(f"[INFO] Connecting to existing instance on port {PORT}...")

        import unrealcv

        client = unrealcv.Client(("127.0.0.1", PORT))
        if not client.connect(timeout=10):
            print("[ERROR] Failed to connect to UnrealCV")
            return 1

        print("[OK] Connected to UnrealCV")

        version = client.request("vget /unrealcv/version")
        print(f"[VERSION] {version}")

        if not generate_matting_dataset(client, args.scenes):
            return 1

        client.disconnect()
        return 0

    except KeyboardInterrupt:
        print("\n[INFO] Interrupted by user (Ctrl+C)")
        return 1

    finally:
        if game_proc:
            print("\n[INFO] Stopping game and all child processes...")
            try:
                kill_process_and_its_children(game_proc)
                print("[OK] Game and all child processes stopped")
            except Exception as e:
                print(f"[ERROR] Failed to stop game: {e}")
                try:
                    subprocess.run(['taskkill', '/F', '/IM', 'HUAWEI_Project.exe'],
                                 capture_output=True)
                    subprocess.run(['taskkill', '/F', '/IM', 'HUAWEI_Project-Win64-Shipping.exe'],
                                 capture_output=True)
                    print("[OK] Force killed with taskkill")
                except Exception as e2:
                    print(f"[ERROR] Taskkill also failed: {e2}")

if __name__ == "__main__":
    os._exit(main())
