#!/usr/bin/env python3
import sys
import socket

sys.path.insert(0, 'G:/HUAWEI_Project_UE56/Plugins/unrealcv/Source/uezoo')

HOST = '127.0.0.1'
PORT = 9000

def send_command(sock, cmd):
    """Send command and receive response using UnrealCV protocol"""
    # Add message ID
    message = f"0:{cmd}"
    # Send magic number
    import struct
    MAGIC = 0x9E2B83C1
    sock.sendall(struct.pack('I', MAGIC))
    # Send payload size
    sock.sendall(struct.pack('I', len(message)))
    # Send payload
    sock.sendall(message.encode('utf-8'))

    # Receive magic
    raw_magic = sock.recv(4)
    if not raw_magic:
        return None
    magic = struct.unpack('I', raw_magic)[0]
    if magic != MAGIC:
        print(f"Wrong magic: {magic}")
        return None

    # Receive payload size
    raw_size = sock.recv(4)
    if not raw_size:
        return None
    size = struct.unpack('I', raw_size)[0]

    # Receive payload
    payload = b''
    while len(payload) < size:
        chunk = sock.recv(size - len(payload))
        if not chunk:
            return None
        payload += chunk

    # Parse response (format: id:body)
    response = payload.decode('utf-8')
    if ':' in response:
        response = response.split(':', 1)[1]
    return response

def main():
    print(f"Connecting to {HOST}:{PORT}...")

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(10)

    try:
        sock.connect((HOST, PORT))
        print("Connected!")

        # Receive welcome message
        welcome = sock.recv(1024)
        print(f"Welcome: {welcome}")

        # Run tests
        tests = [
            ("Version", "vget /unrealcv/version"),
            ("Cameras", "vget /cameras"),
            ("Camera 0 Location", "vget /camera/0/location"),
            ("Camera 0 Rotation", "vget /camera/0/rotation"),
            ("Objects", "vget /objects"),
        ]

        passed = 0
        for name, cmd in tests:
            try:
                res = send_command(sock, cmd)
                if res and not res.startswith("error"):
                    print(f"[PASS] {name}: {res[:60]}..." if len(res) > 60 else f"[PASS] {name}: {res}")
                    passed += 1
                else:
                    print(f"[FAIL] {name}: {res}")
            except Exception as e:
                print(f"[FAIL] {name}: {e}")

        print(f"\nResult: {passed}/{len(tests)} tests passed")

    except Exception as e:
        print(f"Error: {e}")
        return 1
    finally:
        sock.close()

    return 0 if passed == len(tests) else 1

if __name__ == "__main__":
    sys.exit(main())
