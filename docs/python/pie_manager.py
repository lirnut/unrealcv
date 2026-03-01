#!/usr/bin/env python3
import socket
import time
from pathlib import Path

EDITOR_PORT = 9000
PIE_PORT = 9001
PIE_START_TIMEOUT = 30
PIE_START_WAIT = 15


def try_connect(port, timeout=2.0):
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(timeout)
        result = sock.connect_ex(("127.0.0.1", port))
        # sock.close()
        sock._real_close()
        return result == 0
    except Exception:
        return False


def main():
    import unrealcv

    pie_run_count = 0
    pie_session_start = None

    print(f"[INFO] Connecting to editor on port {EDITOR_PORT}...")
    editor_client = unrealcv.Client(("127.0.0.1", EDITOR_PORT))
    if not editor_client.connect(timeout=10):
        print("[ERROR] Failed to connect to editor")
        return 1

    print("[OK] Connected to editor")
    version = editor_client.request("vget /unrealcv/version")
    print(f"[VERSION] {version}")

    while True:
        print(f"\n{'='*60}")
        print(f"[CHECK] Checking PIE status on port {PIE_PORT}...")
        print(f"{'='*60}")

        if not try_connect(PIE_PORT):
            print("[INFO] PIE not running, starting PIE...")

            result = editor_client.request("vset /editor/start_standalone_pie")
            print(f"[PIE START] {result}")

            print(f"[WAIT] Waiting {PIE_START_WAIT}s for PIE to start...")
            time.sleep(PIE_START_WAIT)

            for attempt in range(PIE_START_TIMEOUT // 5):
                if try_connect(PIE_PORT):
                    pie_run_count += 1
                    pie_session_start = time.time()
                    print(f"[OK] PIE started on port {PIE_PORT}")
                    print(f"[STATS] Run #{pie_run_count}")
                    break
                print(f"[RETRY] Attempt {attempt + 1}, waiting 5s...")
                time.sleep(5)
            else:
                print("[ERROR] PIE failed to start")
                continue
        else:
            print(f"[INFO] PIE already running on port {PIE_PORT}")

        # pie_client = unrealcv.Client(("127.0.0.1", PIE_PORT))
        # if not pie_client.connect(timeout=10):
        #     print("[ERROR] Failed to connect to PIE")
        #     continue

        # print("[OK] Connected to PIE")

        try:
            while True:
                # pie_client = unrealcv.Client(("127.0.0.1", PIE_PORT))
                # if not pie_client.connect(timeout=10):
                if not try_connect(PIE_PORT):
                    elapsed = time.time() - pie_session_start if pie_session_start else 0
                    print(f"[INFO] PIE stopped (run #{pie_run_count}, runtime: {elapsed:.1f}s)")
                    pie_session_start = None
                    break

                if pie_session_start:
                    elapsed = time.time() - pie_session_start
                    print(f"[OK] PIE running - Run #{pie_run_count}, time: {elapsed:.1f}s")

                time.sleep(1.0)
        except KeyboardInterrupt:
            break

        print("[INFO] PIE session ended, returning to check loop...")

    editor_client.disconnect()
    return 0


if __name__ == "__main__":
    import os
    os._exit(main())
