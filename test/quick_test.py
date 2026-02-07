#!/usr/bin/env python3
import sys
sys.path.insert(0, 'G:/HUAWEI_Project_UE56/Plugins/unrealcv/Source/uezoo')
import unrealcv

print('Starting UnrealCV quick test...')

client = unrealcv.Client(('127.0.0.1', 9000))
if client.connect(timeout=10):
    print('[PASS] Connected to UnrealCV server')

    res = client.request('vget /unrealcv/version')
    print(f'[TEST] Version: {res}')

    res = client.request('vget /cameras')
    print(f'[TEST] Cameras: {res}')

    res = client.request('vget /camera/0/location')
    print(f'[TEST] Camera 0 location: {res}')

    res = client.request('vget /objects')
    print(f'[TEST] Objects count: {len(res.split()) if res else 0}')

    client.disconnect()
    print('[DONE] All tests passed!')
else:
    print('[FAIL] Cannot connect to server')
    sys.exit(1)
