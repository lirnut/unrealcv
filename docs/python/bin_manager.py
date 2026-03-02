#!/usr/bin/env python3
import sys
import subprocess
import socket
import time
import json
import psutil
import random
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
PKG_DIR = Path("I:/HUAWEI_Project_UE56_PKG/Windows")
EXE_PATH = PKG_DIR / "HUAWEI_Project.exe"

PORT = 9000
CONNECT_TIMEOUT = 180
MAP_LOAD_WAIT = 45
STATUS_POLL_INTERVAL = 2.0
CONFIG_SLASH_TOTAL_SCENES = 10

AVAILABLE_MAPS = [
    "Tokyo",
    "Chinese_mountain_town",
    "Demo_Roof",
    "Urban_RoadsideConstruction_Scene",
    "Town",
    # "L_WillowLake",
    # "Jungle",
    # "TrainStation",
    # "Mountains_Map",
    "Asian_town",
    "Hutong",
    "Midgardr_Free",
    "Warehouse",
    "Downtown_West",
    "Downtown_West_Night",
    "Bridge_P",
]

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

    selected_map = random.choice(AVAILABLE_MAPS)
    print(f"[INFO] Selected map {selected_map}...")
    cmd = [
        str(EXE_PATH),
        selected_map,
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
            print(f"[ERROR] Game exited with code {poll}")
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

def try_connect(port, timeout=2.0):
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(timeout)
        result = sock.connect_ex(("127.0.0.1", port))
        sock.close()
        return result == 0
    except Exception:
        return False

def build_matting_command_sequence():
    """
    Matting task command sequence (equivalent to DatasetAutomationBPLib.cpp lines 146-200)

    This is the Python-side definition that replaces the hardcoded C++ sequence.
    Matches the exact logic:
    - 50% portrait (1080x1920, FOV 50-60) OR 50% landscape (1920x1080, FOV 80-90)
    - Random trajectory: render_only, render_left_rotate, render_right_rotate, etc.
    - ForegroundMoveSpeed: 70.0 cm/s @ 90° offset (perpendicular motion)
    - Time dilation: 0.65x (slow-motion for hair dynamics)
    - 90 frames @ 30fps = 3 seconds real-time, ~4.6 seconds in-game
    """
    if random.random() < 0.5:
        resolution = "1080x1920"
        fov_range = "50 60"
    else:
        resolution = "1920x1080"
        fov_range = "80 90"

    matting_trajectory_options = [
        "render_only",
        "render_left_rotate",
        "render_right_rotate",
        "render_rotate_left",
        "render_rotate_right"
    ]
    chosen_trajectory = random.choice(matting_trajectory_options)

    command_sequence = {
        "task": "Matting",
        "commands": [
            {"cmd": "vrun", "params": "vset /captureactor/time_dilation 0.65"},
            {"cmd": "load_scene_param_json"},
            {"cmd": "random_scene_param_camera_height", "params": "120 155"},
            {"cmd": "random_scene_param_camera_angle_offset", "params": "-60 60"},
            {"cmd": "random_scene_param_camera_distance", "params": "75 100"},
            {"cmd": "create_scene"},
            {"cmd": "set_animation_bp", "params": "/Game/MetaHumans/ABP_Run.ABP_Run_C"},
            {"cmd": "prepare_groom"},
            {"cmd": "sync_pawn_to_primary_camera"},
            {"cmd": "delay", "params": "5.0"},
            {"cmd": "random_resolution", "params": resolution},
            {"cmd": "random_fov", "params": fov_range},
            {"cmd": "aim_camera_at_foreground", "params": "125 175"},
            {"cmd": "add_camera_rotation_noise", "params": "4.0 0.5 2.0"},
            {"cmd": "prepare_record"},
            {"cmd": "delay", "params": "10.0"},
            {"cmd": "record_trajectory", "params": chosen_trajectory},
            {"cmd": "sync_all_cameras"},
            {"cmd": "delay", "params": "1.0"},
            {"cmd": "clear_scene"},
            {"cmd": "delay", "params": "0.5"},
            {"cmd": "increment_counter"},
            {"cmd": "check_completion"}
        ]
    }

    return command_sequence

def main():
    import unrealcv
    run_count = 0

    try:
        while True:
            print(f"\n{'='*60}")
            print(f"Binary Session #{run_count + 1}")
            print(f"{'='*60}")

            game_proc = start_game()
            if game_proc is None:
                return 1

            if not wait_for_server(game_proc, CONNECT_TIMEOUT):
                print("[ERROR] Failed to start server, cleaning up...")
                kill_process_and_its_children(game_proc)
                print("[INFO] Retrying in 5s...")
                time.sleep(5)
                continue

            client = unrealcv.Client(("127.0.0.1", PORT))
            if not client.connect(timeout=10):
                print("[ERROR] Failed to connect to UnrealCV")
                kill_process_and_its_children(game_proc)
                print("[INFO] Retrying in 5s...")
                time.sleep(5)
                continue

            print("[OK] Connected to UnrealCV")

            version = client.request("vget /unrealcv/version")
            print(f"[VERSION] {version}")

            print(f"\n{'='*60}")
            print(f"[CONFIG] Configuring Matting task...")
            print(f"{'='*60}")

            client.request(f"vset /datasetautomation/config/total_scenes {CONFIG_SLASH_TOTAL_SCENES}")
            client.request("vset /datasetautomation/config/trajectory_fps 30")
            client.request("vset /datasetautomation/config/num_frames 90")
            client.request("vset /datasetautomation/config/foreground_move_speed 70.0")
            client.request("vset /datasetautomation/config/foreground_move_angle_offset 90.0")
            client.request("vset /datasetautomation/config/recording_options lit,oneobjlit,metadata")

            output_dir = str(PKG_DIR / "HUAWEI_Project" / "Saved" / "DatasetAutomationOutputDirectory")
            client.request(f"vset /datasetautomation/config/output_directory {output_dir}")

            print(f"[CONFIG] Output directory: {output_dir}")
            print(f"[CONFIG] Batch size: {CONFIG_SLASH_TOTAL_SCENES} scenes")
            print(f"[CONFIG] Recording: 90 frames @ 30fps (3s real-time)")
            print(f"[CONFIG] Foreground motion: 70 cm/s @ 90° offset")

            command_sequence = build_matting_command_sequence()
            seq_json = json.dumps(command_sequence)
            result = client.request(f"vset /datasetautomation/sequence {seq_json}")
            print(f"[SEQUENCE] {result}")
            print(f"[SEQUENCE] Uploaded {len(command_sequence['commands'])} commands")

            print(f"\n[WAIT] Waiting {MAP_LOAD_WAIT}s for map loading...")
            time.sleep(MAP_LOAD_WAIT)

            result = client.request("vset /datasetautomation/start")
            print(f"[START] {result}")

            run_count += 1
            session_start = time.time()

            try:
                while True:

                    try:
                        status = client.request("vget /datasetautomation/status")
                        elapsed = time.time() - session_start
                        print(f"[STATUS] Run #{run_count}, time: {elapsed:.1f}s - {status}")

                        if "Completed" in status:
                            print(f"[SUCCESS] Session completed")
                            break
                        elif "Error" in status:
                            print(f"[ERROR] Session failed: {status}")
                            break

                    except Exception as e:
                        print(f"[ERROR] Status check failed: {e}")
                        break

                    time.sleep(STATUS_POLL_INTERVAL)

            except KeyboardInterrupt:
                print("\n[INFO] Interrupted by user (Ctrl+C)")
                raise

            finally:
                try:
                    client.disconnect()
                except:
                    pass

                print("[INFO] Stopping game and all child processes...")
                kill_process_and_its_children(game_proc)

            print(f"[INFO] Binary session #{run_count} ended")
            print("[INFO] Restarting in 5s...")
            time.sleep(5)

    except KeyboardInterrupt:
        print("\n[INFO] Shutdown requested, exiting...")
        return 0

    return 0

if __name__ == "__main__":
    import os
    os._exit(main())
