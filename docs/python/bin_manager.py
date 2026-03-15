#!/usr/bin/env python3
import sys, datetime, os, re
import subprocess
import socket
import time
import json
import psutil
import random
from pathlib import Path
from multiprocessing import Process, Event, Value
import argparse
from sequence_builder import build_concatenated_matting_sequence, build_concatenated_trajectory_sequence


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
MAP_LOAD_WAIT = 30
STATUS_POLL_INTERVAL = 2.0
CONFIG_SLASH_TOTAL_SCENES = 70
SESSION_TIMEOUT = CONFIG_SLASH_TOTAL_SCENES * 30
STARTUP_TIMEOUT = 120

if EXE_PATH.__str__().endswith("HillsideSampleProject.exe"):
    AVAILABLE_MAPS: list[tuple[str, float]] = [
        ("LV_Exterior", 1.0)
    ]
elif EXE_PATH.__str__().endswith("CitySample.exe"):
    AVAILABLE_MAPS: list[tuple[str, float]] = [
        ("Small_City_LVL", 1.0)
    ]
else:
    AVAILABLE_MAPS: list[tuple[str, float]] = [
        ("Tokyo", 1.0),
        ("Chinese_mountain_town", 1.0),
        ("Demo_Roof", 1.0),
        # ("Urban_RoadsideConstruction_Scene", 1.0),
        # ("Town", 0.2),
        ("L_WillowLake", 0.3),
        # ("Jungle", 1.0),
        # ("TrainStation", 1.5),
        # ("Mountains_Map", 0.8),
        ("Asian_town", 2.0),
        # ("Hutong", 1.0),
        ("Midgardr_Free", 1.5),
        ("Warehouse", 1.0),
        ("Downtown_West", 0.5),
        # ("Downtown_West_Night", 1.7),
        ("Bridge_P", 1.3),
        ("LV_Exterior", 0.3),
        ("LV_Exterior_Night", 0.2),
        ("LV_Exterior_Sunrise", 0.5),
        ("Beach_P", 0.5),
        ("UNIVERSITY_CLASSROOM", 1.5),
    ]

    # AVAILABLE_MAPS = [
    #     ("Demo_Roof", 1.0),
    # ]
print(f"\n{'='*60}")
print(f"Available Maps")
print(f"{'='*60}")
for m, w in AVAILABLE_MAPS:
    print(f"\t{m} (weight: {w})")

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
                child_name = child.name().lower()
                if 'python' in child_name:
                    continue
                if hasattr(child, 'children') and len(child.children()) > 0:
                    kill_process_and_its_children(child)
                else:
                    kill_process(child)
    except Exception as e:
        print(f"[WARN] Kill process and it's children exception: {e}")
    kill_process(p)

def watchdog_worker(game_pid, timeout_seconds, stop_event, last_alive_timestamp):
    """Separate process that monitors and kills game if timeout reached."""
    import time
    import sys

    start_time = time.time()
    last_status_time = start_time

    try:
        while not stop_event.is_set():
            time.sleep(1)

            if stop_event.is_set():
                break

            current_time = time.time()
            last_alive = last_alive_timestamp.value
            elapsed_since_alive = current_time - last_alive

            # Print status every 30 seconds
            if current_time - last_status_time >= 30:
                watchdog_elapsed = current_time - start_time
                try:
                    proc = psutil.Process(game_pid)
                    proc_status = "alive" if proc.is_running() else "terminated"
                    mem_info = proc.memory_info().rss / (1024 * 1024)  # MB
                    status_line = f"[WATCHDOG] Status @ {watchdog_elapsed:.0f}s | Game PID {game_pid} ({proc_status}, {mem_info:.0f}MB) | Last alive: {elapsed_since_alive:.1f}s ago | Timeout: {timeout_seconds}s"
                except psutil.NoSuchProcess:
                    status_line = f"[WATCHDOG] Status @ {watchdog_elapsed:.0f}s | Game PID {game_pid} (not found) | Last alive: {elapsed_since_alive:.1f}s ago | Timeout: {timeout_seconds}s"
                except Exception as e:
                    status_line = f"[WATCHDOG] Status @ {watchdog_elapsed:.0f}s | Game PID {game_pid} (error: {e}) | Last alive: {elapsed_since_alive:.1f}s ago | Timeout: {timeout_seconds}s"

                print(status_line, flush=True)
                last_status_time = current_time

            if elapsed_since_alive > timeout_seconds:
                print(f"[WATCHDOG] Timeout reached ({timeout_seconds}s), killing game process {game_pid}", flush=True)
                try:
                    proc = psutil.Process(game_pid)
                    for child in proc.children(recursive=True):
                        child.kill()
                    proc.kill()
                    print(f"[WATCHDOG] Game process {game_pid} killed", flush=True)
                except psutil.NoSuchProcess:
                    print(f"[WATCHDOG] Game process {game_pid} already terminated", flush=True)
                except Exception as e:
                    print(f"[WATCHDOG] Error killing process: {e}", flush=True)
                break
    except KeyboardInterrupt:
        pass
    print("[WATCHDOG] Watchdog process exiting", flush=True)

def startup_watchdog_worker(game_pid, timeout_seconds, stop_event):
    """Separate process that monitors game startup. Kills game if it hangs during startup."""
    import time

    start_time = time.time()
    print(f"[STARTUP_WATCHDOG] Started monitoring game PID {game_pid}, timeout: {timeout_seconds}s", flush=True)

    try:
        while not stop_event.is_set():
            time.sleep(1)

            if stop_event.is_set():
                print("[STARTUP_WATCHDOG] Stop signal received, exiting", flush=True)
                break

            elapsed = time.time() - start_time

            # Check if game process is still running
            try:
                proc = psutil.Process(game_pid)
                if not proc.is_running():
                    print(f"[STARTUP_WATCHDOG] Game process {game_pid} terminated early", flush=True)
                    break
            except psutil.NoSuchProcess:
                print(f"[STARTUP_WATCHDOG] Game process {game_pid} not found", flush=True)
                break

            # Check timeout
            if elapsed > timeout_seconds:
                print(f"[STARTUP_WATCHDOG] STARTUP TIMEOUT REACHED ({timeout_seconds}s), killing game process {game_pid}", flush=True)
                try:
                    proc = psutil.Process(game_pid)
                    for child in proc.children(recursive=True):
                        child.kill()
                    proc.kill()
                    print(f"[STARTUP_WATCHDOG] Game process {game_pid} killed due to startup timeout", flush=True)
                except psutil.NoSuchProcess:
                    print(f"[STARTUP_WATCHDOG] Game process {game_pid} already terminated", flush=True)
                except Exception as e:
                    print(f"[STARTUP_WATCHDOG] Error killing process: {e}", flush=True)
                break

            # Print status every 10 seconds
            if int(elapsed) % 10 == 0 and int(elapsed) > 0:
                try:
                    proc = psutil.Process(game_pid)
                    mem_info = proc.memory_info().rss / (1024 * 1024)
                    print(f"[STARTUP_WATCHDOG] Monitoring... {elapsed:.0f}s elapsed, game PID {game_pid} ({mem_info:.0f}MB)", flush=True)
                except:
                    print(f"[STARTUP_WATCHDOG] Monitoring... {elapsed:.0f}s elapsed", flush=True)

    except KeyboardInterrupt:
        pass

    print("[STARTUP_WATCHDOG] Startup watchdog process exiting", flush=True)

def weighted_random_choice(maps: list[tuple[str, float]]) -> str:
    """Select a map based on probability weights."""
    if not maps:
        raise ValueError("Empty map list")
    total_weight = sum(weight for _, weight in maps)
    r = random.uniform(0, total_weight)
    cumulative = 0.0
    for map_name, weight in maps:
        cumulative += weight
        if r <= cumulative:
            return map_name
    return maps[-1][0]

def start_game():
    if not EXE_PATH.exists():
        print(f"[ERROR] Executable not found: {EXE_PATH}")
        return None, None

    selected_map = weighted_random_choice(AVAILABLE_MAPS)
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

    # Parse command line arguments
    parser = argparse.ArgumentParser(description="UnrealCV Dataset Automation Binary Manager")
    parser.add_argument("--traj", action="store_true", help="Run Trajectory task instead of Matting")
    args = parser.parse_args()

    is_trajectory_mode = args.traj
    task_name = "Trajectory" if is_trajectory_mode else "Matting"

    print(f"\n{'='*60}")
    print(f"Task Mode: {task_name}")
    print(f"{'='*60}")

    run_count = 0

    try:
        while True:
            print(f"\n{'='*60}")
            print(f"Binary Session #{run_count + 1}")
            print(f"{'='*60}")

            # Initialize startup watchdog variables
            startup_watchdog_stop = None
            startup_watchdog_proc = None

            game_proc, map_name = start_game()
            if game_proc is None:
                return 1

            # Start startup watchdog immediately after game process starts
            startup_watchdog_stop = Event()
            startup_watchdog_proc = Process(
                target=startup_watchdog_worker,
                args=(game_proc.pid, STARTUP_TIMEOUT, startup_watchdog_stop)
            )
            startup_watchdog_proc.start()
            print(f"[STARTUP_WATCHDOG] Started startup watchdog process (PID: {startup_watchdog_proc.pid}, timeout: {STARTUP_TIMEOUT}s)")

            if not wait_for_server(game_proc, CONNECT_TIMEOUT):
                print("[ERROR] Failed to start server, cleaning up...")
                startup_watchdog_stop.set()
                startup_watchdog_proc.join(timeout=1.0)
                if startup_watchdog_proc.is_alive():
                    startup_watchdog_proc.terminate()
                kill_process_and_its_children(game_proc)
                print("[INFO] Retrying in 5s...")
                time.sleep(5)
                continue

            client = unrealcv.Client(("127.0.0.1", PORT))
            if not client.connect(timeout=10):
                print("[ERROR] Failed to connect to UnrealCV")
                startup_watchdog_stop.set()
                startup_watchdog_proc.join(timeout=1.0)
                if startup_watchdog_proc.is_alive():
                    startup_watchdog_proc.terminate()
                kill_process_and_its_children(game_proc)
                print("[INFO] Retrying in 5s...")
                time.sleep(5)
                continue

            print("[OK] Connected to UnrealCV")

            print(f"\n[WAIT] Waiting {MAP_LOAD_WAIT}s for map loading...")
            time.sleep(MAP_LOAD_WAIT)

            version = client.request("vget /unrealcv/version")
            print(f"[VERSION] {version}")

            print(f"\n{'='*60}")
            print(f"[CONFIG] Configuring Video Encoder...")
            print(client.request("vset /captureactor/h264_encoding 0"))
            print(client.request("vset /captureactor/auto_generate_video 1"))

            print(f"\n{'='*60}")
            print(f"[CONFIG] Configuring {task_name} task...")

            client.request(f"vset /datasetautomation/config/total_scenes {CONFIG_SLASH_TOTAL_SCENES}")

            if is_trajectory_mode:
                print(client.request("vset /datasetautomation/config/trajectory_fps 30"))
                print(client.request("vset /datasetautomation/config/num_frames 121"))
                print(client.request("vset /datasetautomation/config/recording_options lit,mask,oneobjlit,depth,metadata"))
                print(client.request("vset /captureactor/paused_tick_interval 0.1"))
            else:
                # Matting task config
                print(client.request("vset /datasetautomation/config/trajectory_fps 30"))
                print(client.request("vset /datasetautomation/config/num_frames 90"))
                print(client.request("vset /datasetautomation/config/recording_options lit,mask,oneobjlit,depth,metadata"))

            timecode = datetime.datetime.now().strftime(r"%y-%m-%d") + f"_{task_name}"
            output_dir = str(PKG_DIR / "DatasetAutomationOutputDirectory" / timecode / map_name)
            client.request(f"vset /datasetautomation/config/output_directory {output_dir}")

            print(f"[CONFIG] Output directory: {output_dir}")
            print(f"[CONFIG] Batch size: {CONFIG_SLASH_TOTAL_SCENES} scenes")

            # Build appropriate sequence based on task type
            if is_trajectory_mode:
                command_sequence, scene_configs = build_concatenated_trajectory_sequence(CONFIG_SLASH_TOTAL_SCENES)
            else:
                command_sequence, scene_configs = build_concatenated_matting_sequence(CONFIG_SLASH_TOTAL_SCENES)

            seq_json = json.dumps(command_sequence, separators=(',', ':'))
            result = client.request(f"vset /datasetautomation/sequence {seq_json}")
            print(f"[SEQUENCE] {result}")
            print(f"[SEQUENCE] Total commands: {len(command_sequence['commands'])}")
            print(f"[SEQUENCE] Scenes: {CONFIG_SLASH_TOTAL_SCENES}")
            print()
            print("[SEQUENCE] Scene configurations:")
            for cfg in scene_configs:
                if is_trajectory_mode:
                    print(f"  Scene {cfg['scene']:2d}: {cfg['resolution']:12s} | FOV {cfg['fov']:10s} | Sync Frame {cfg.get('sync_frame', 'N/A'):6s} | Trajectory: {cfg['trajectory']}")
                else:
                    print(f"  Scene {cfg['scene']:2d}: {cfg['resolution']:12s} | FOV {cfg['fov']:5s} | {cfg['trajectory']:25s} | Mode {cfg.get('animation_mode', 'Unknown')}")


            print(f"\n{'='*60}")
            print(f"[CONFIG] Scalability...")
            # print(client.request("vrun r.ScreenPercentage 67.0"))
            # print(client.request("vrun r.Shadow.Virtual.Enable 0"))
            print(client.request("vrun r.HairStrands.SkyLighting 0"))
            if not is_trajectory_mode:
                print(client.request("vrun r.HairStrands.SwapType 2"))


            # Disable unnecessary warnings and messages
            client.request("vrun DisableAllScreenMessages")
            client.request("vrun r.Streaming.PoolSize.ShowWarnings 0")
            
            result = client.request("vset /datasetautomation/start")
            print(f"[START] {result}")

            # Startup complete, stop and destroy the startup watchdog
            print("[STARTUP_WATCHDOG] Startup complete, stopping startup watchdog...")
            startup_watchdog_stop.set()
            startup_watchdog_proc.join(timeout=2.0)
            if startup_watchdog_proc.is_alive():
                startup_watchdog_proc.terminate()
                startup_watchdog_proc.join(timeout=1.0)
            print("[STARTUP_WATCHDOG] Startup watchdog stopped")

            run_count += 1
            session_start = time.time()

            watchdog_stop = Event()
            last_alive = Value('d', time.time())
            watchdog_proc = Process(
                target=watchdog_worker,
                args=(game_proc.pid, SESSION_TIMEOUT, watchdog_stop, last_alive)
            )
            watchdog_proc.start()
            print(f"[WATCHDOG] Started main watchdog process (PID: {watchdog_proc.pid}, timeout: {SESSION_TIMEOUT}s)")

            try:
                while True:
                    try:
                        status = client.request("vget /datasetautomation/status")
                        last_alive.value = time.time()
                        elapsed = time.time() - session_start
                        print(f"[STATUS] Run #{run_count}, time: {elapsed:.1f}s - {status}")

                        if status is None:
                            print(f"[WARNING] Status is None")
                            break

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
                # Stop main watchdog
                watchdog_stop.set()
                watchdog_proc.join(timeout=2.0)
                if watchdog_proc.is_alive():
                    watchdog_proc.terminate()
                    watchdog_proc.join(timeout=1.0)

                # Stop startup watchdog if still running
                if startup_watchdog_stop is not None:
                    startup_watchdog_stop.set()
                if startup_watchdog_proc is not None and startup_watchdog_proc.is_alive():
                    startup_watchdog_proc.join(timeout=1.0)
                    if startup_watchdog_proc.is_alive():
                        startup_watchdog_proc.terminate()
                        startup_watchdog_proc.join(timeout=1.0)

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
