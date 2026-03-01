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
        sock.close()
        return result == 0
    except Exception:
        return False


def main():
    import unrealcv

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
                    print(f"[OK] PIE started on port {PIE_PORT}")
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

        while True:
            # pie_client = unrealcv.Client(("127.0.0.1", PIE_PORT))
            # if not pie_client.connect(timeout=10):
            if not try_connect(PIE_PORT):
                print("[INFO] PIE stopped or not accessible")
                break

            print("[OK] Connected to PIE")


            time.sleep(1.0)

        print("[INFO] PIE session ended, returning to check loop...")

    editor_client.disconnect()
    return 0


if __name__ == "__main__":
    import os
    os._exit(main())
