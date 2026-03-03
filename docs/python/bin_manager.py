#!/usr/bin/env python3
import sys, datetime, os, re
import subprocess
import socket
import time
import json
import psutil
import random
from pathlib import Path
from sequence_builder import build_concatenated_matting_sequence

SCRIPT_DIR = Path(__file__).resolve().parent
PKG_DIR = Path(SCRIPT_DIR)
# PKG_DIR = Path("I:/HUAWEI_Project_UE56_PKG")
platform = "Windows"

platform_files = os.listdir(PKG_DIR / platform)
pattern_exe = re.compile(r'^.+\.exe$')
pattern_no_suffix = re.compile(r'^.+$')

EXE_PATH = "not found"
for f in platform_files:
    if pattern_exe.match(f) or pattern_no_suffix.match(f):
        fp = PKG_DIR / platform / f
        if os.path.isdir(fp):
            continue
        EXE_PATH = fp
        print("found exe", EXE_PATH)
        break



PORT = 9000
CONNECT_TIMEOUT = 60
MAP_LOAD_WAIT = 15
STATUS_POLL_INTERVAL = 2.0
CONFIG_SLASH_TOTAL_SCENES = 10


if EXE_PATH.endswith("HillsideSampleProject.exe"):
    AVAILABLE_MAPS = [
        "LV_Exterior"
    ]
elif EXE_PATH.endswith("CitySample.exe"):
    AVAILABLE_MAPS = [
        "Small_City_LVL"
    ]
else:
    AVAILABLE_MAPS = [
        "Tokyo",
        "Chinese_mountain_town",
        "Demo_Roof",
        # "Urban_RoadsideConstruction_Scene",
        "Town",
        # "L_WillowLake",
        # "Jungle",
        # "TrainStation",
        # "Mountains_Map",
        "Asian_town",
        # "Hutong",
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
    try:
        p = psutil.Process(p.pid)
        if len(p.children()) > 0:
            for child in p.children():
                if hasattr(child, 'children') and len(child.children()) > 0:
                    kill_process_and_its_children(child)
                else:
                    kill_process(child)
    except Exception as e:
        print(f"[WARN] Kill process and it's children exception: {e}")
    kill_process(p)

def start_game():
    if not EXE_PATH.exists():
        print(f"[ERROR] Executable not found: {EXE_PATH}")
        return None, None

    selected_map = random.choice(AVAILABLE_MAPS)
    print(f"[INFO] Selected map {selected_map}...")
    cmd = [
        str(EXE_PATH),
        selected_map,
        "-Log",
        "-FullStdOutLogOutput",
        "-unattended",
        "-windowed",
	    "-resx=1920",
	    "-resy=1920"
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
    return proc, selected_map

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

def main():
    import unrealcv
    run_count = 0

    try:
        while True:
            print(f"\n{'='*60}")
            print(f"Binary Session #{run_count + 1}")
            print(f"{'='*60}")

            game_proc, map_name = start_game()
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
            print(f"[CONFIG] Configuring Video Encoder...")
            print(f"{'='*60}")
            print(client.request("vset /captureactor/h264_encoding 0"))
            print(client.request("vset /captureactor/auto_generate_video 1"))

            print(f"\n{'='*60}")
            print(f"[CONFIG] Configuring Matting task...")
            print(f"{'='*60}")

            client.request(f"vset /datasetautomation/config/total_scenes {CONFIG_SLASH_TOTAL_SCENES}")
            client.request("vset /datasetautomation/config/trajectory_fps 30")
            client.request("vset /datasetautomation/config/num_frames 90")
            client.request("vset /datasetautomation/config/foreground_move_speed 70.0")
            client.request("vset /datasetautomation/config/foreground_move_angle_offset 90.0")
            client.request("vset /datasetautomation/config/recording_options lit,oneobjlit,metadata")

            timecode = datetime.datetime.now().strftime(r"%y-%m-%d")
            output_dir = str(PKG_DIR / "DatasetAutomationOutputDirectory" / timecode / map_name)
            client.request(f"vset /datasetautomation/config/output_directory {output_dir}")

            print(f"[CONFIG] Output directory: {output_dir}")
            print(f"[CONFIG] Batch size: {CONFIG_SLASH_TOTAL_SCENES} scenes")

            command_sequence, scene_configs = build_concatenated_matting_sequence(CONFIG_SLASH_TOTAL_SCENES)
            seq_json = json.dumps(command_sequence, separators=(',', ':'))
            result = client.request(f"vset /datasetautomation/sequence {seq_json}")
            print(f"[SEQUENCE] {result}")
            print(f"[SEQUENCE] Total commands: {len(command_sequence['commands'])}")
            print(f"[SEQUENCE] Scenes: {CONFIG_SLASH_TOTAL_SCENES} (each with ~22 unique commands)")
            print()
            print("[SEQUENCE] Scene configurations:")
            for cfg in scene_configs:
                print(f"  Scene {cfg['scene']:2d}: {cfg['resolution']:12s} | FOV {cfg['fov']:5s} | {cfg['trajectory']}")

            print(f"\n[WAIT] Waiting {MAP_LOAD_WAIT}s for map loading...")
            time.sleep(MAP_LOAD_WAIT)

            # Disable unnecessary warnings and messages
            client.request("vrun DisableAllScreenMessages")
            client.request("vrun r.Streaming.PoolSize.ShowWarnings 0")
            
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
    except Exception as err:
        print(f"\n[Error] Unknown error {err}")
        return 1
    return 0

if __name__ == "__main__":
    import os
    os._exit(main())
