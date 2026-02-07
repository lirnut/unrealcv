#!/usr/bin/env python3
import sys
sys.path.insert(0, 'G:/HUAWEI_Project_UE56/Plugins/unrealcv/Source/uezoo')
import unrealcv

print('Starting UnrealCV test...')

client = unrealcv.Client(('127.0.0.1', 9000))
if client.connect(timeout=10):
    print('[PASS] Connected to UnrealCV server')

    tests = [
        ('Version', 'vget /unrealcv/version'),
        ('Cameras', 'vget /cameras'),
        ('Camera 0 Location', 'vget /camera/0/location'),
        ('Camera 0 Rotation', 'vget /camera/0/rotation'),
        ('Objects', 'vget /objects'),
    ]

    passed = 0
    for name, cmd in tests:
        try:
            res = client.request(cmd)
            if res and not res.startswith('error'):
                print(f'[PASS] {name}: {res[:50]}...' if len(str(res)) > 50 else f'[PASS] {name}: {res}')
                passed += 1
            else:
                print(f'[FAIL] {name}: {res}')
        except Exception as e:
            print(f'[FAIL] {name}: {e}')

    client.disconnect()
    print(f'\nResult: {passed}/{len(tests)} tests passed')
    sys.exit(0 if passed == len(tests) else 1)
else:
    print('[FAIL] Cannot connect to server')
    sys.exit(1)
