"""
使用方法: 
1. 使用参数指定输入目录
    python .\generate_oneobjlit.py --input-dir D:\DATA\HUAWEI_Project_UE56_PKG\DatasetAutomationOutputDirectory\CB-26-03-08\Mountains_Map\scene_0001_1DC712A0\render_rotate_right.tar.gz
    python .\generate_oneobjlit.py --input-dir D:\DATA\HUAWEI_Project_UE56_PKG\DatasetAutomationOutputDirectory\CB-26-03-08\Mountains_Map\scene_0001_1DC712A0\render_rotate_right
2. 直接调用函数
    from generate_oneobjlit import generate_oneobjlit
    generate_oneobjlit("D:\DATA\HUAWEI_Project_UE56_PKG\DatasetAutomationOutputDirectory\CB-26-03-08\Mountains_Map\scene_0001_1DC712A0\render_rotate_right")
"""




import os
import json
import time
import re
import argparse
import cv2
import numpy as np

def process_alpha_only_frames(sorted_files: list) -> tuple:
    for file_path in sorted_files:
        try:
            img = cv2.imread(file_path, cv2.IMREAD_UNCHANGED)
            if img is None:
                continue
            if len(img.shape) == 3 and img.shape[2] == 4:
                img[:, :, :3] = 0
                cv2.imwrite(file_path, img)
        except Exception:
            pass
    return (True, f"已处理 {len(sorted_files)} 张图片")



def try_decode_content(content: bytes):
    """尝试多种编码解码并解析JSON"""
    encodings = [
        'utf-8-sig',   # UTF-8 with BOM
        'utf-16',      # 自动检测 BOM (UTF-16 LE/BE)
        'utf-8',
        'gbk',
    ]

    for encoding in encodings:
        try:
            res = content.decode(encoding)
            return json.loads(res)
        except Exception:
            continue

    return None

def generate_oneobjlit(input_dir: str):
    input_dir = os.path.abspath(input_dir)

    traj_name = os.path.basename(input_dir)
    scene_path = os.path.dirname(input_dir)
    tar_path = None
    if traj_name.endswith(".tar.gz"):
        tar_path = os.path.join(scene_path, traj_name)
        traj_name = traj_name.removesuffix(".tar.gz")
        input_dir = os.path.join(scene_path, traj_name)
    elif os.path.exists(os.path.join(scene_path, traj_name + ".tar.gz")):
        tar_path = os.path.join(scene_path, traj_name + ".tar.gz")

    if os.path.exists(input_dir) and os.path.isdir(input_dir):
        target_dir = os.path.join(input_dir, "oneobjlit")
        if os.path.exists(target_dir) and os.path.isdir(target_dir):
            all_pngs = os.listdir(target_dir)
            pattern_png = re.compile(r'^(\d+)_(.+)\.png$')
            frame_list = []
            for png in all_pngs:
                match = pattern_png.match(png)
                if match:
                    frame_num = int(match.group(1))
                    frame_list.append((frame_num, os.path.join(target_dir, png)))

            frame_list.sort(key=lambda x: x[0])
            sorted_files = [x[1] for x in frame_list]
            if len(sorted_files) >= 90:
                print("already has 90 oneobjlit frames, skip")
                return


    
    if tar_path is not None:
        # extract tar_path to input_dir
        import tarfile
        with tarfile.open(tar_path, "r:gz") as tar:
            tar.extractall(scene_path)


    overview_path = os.path.join(input_dir, "overview.json")
    overview = None
    if os.path.exists(overview_path):
        with open(overview_path, "rb") as f:
            content = f.read()
            overview = try_decode_content(content)
    else:
        print("overview.json not found!!!")
        if args.time_delay > 0:
            time.sleep(2)

    for filename in os.listdir(input_dir):
        if filename == 'oneobjlit' or filename == 'oneobjgroomlit':
            pngs = os.listdir(os.path.join(input_dir, filename))
            pngs = [os.path.join(input_dir, filename, f) for f in pngs]
            process_alpha_only_frames(pngs)

    oneobjgroomlit_files = None
    mask_files = None

    for dirname in os.listdir(input_dir):
        if dirname == 'oneobjgroomlit':
            pngs = os.listdir(os.path.join(input_dir, dirname))
            pattern_png = re.compile(r'^(\d+)_(.+)\.png$')

            frame_list = []
            for png in pngs:
                match = pattern_png.match(png)
                if match:
                    frame_num = int(match.group(1))
                    frame_list.append((frame_num, os.path.join(input_dir, dirname, png)))

            frame_list.sort(key=lambda x: x[0])
            oneobjgroomlit_files = [f[1] for f in frame_list]

    for dirname in os.listdir(input_dir):
        if dirname == 'mask':
            pngs = os.listdir(os.path.join(input_dir, dirname))
            pattern_png = re.compile(r'^(\d+)_(.+)\.png$')

            frame_list = []
            for png in pngs:
                match = pattern_png.match(png)
                if match:
                    frame_num = int(match.group(1))
                    frame_list.append((frame_num, os.path.join(input_dir, dirname, png)))

            frame_list.sort(key=lambda x: x[0])
            mask_files = [f[1] for f in frame_list]

    if oneobjgroomlit_files is None or mask_files is None:
        print("oneobjgroomlit_files is None or mask_files is None")
        if args.time_delay > 0:
            time.sleep(2)
    else:
        if len(oneobjgroomlit_files) != len(mask_files):
            print(f"len(oneobjgroomlit_files) != len(mask_files): {len(oneobjgroomlit_files)} vs {len(mask_files)}")
            if args.time_delay > 0:
                time.sleep(2)
        else:
            if overview is not None:
                foreground_color = overview["ForegroundColor"].split(",")
                assert len(foreground_color) == 3, f"foreground_color = {len(foreground_color)}"
                foreground_color = np.array([int(c) for c in foreground_color])

                output_gen_dir = os.path.join(input_dir, 'oneobjlit')
                if os.path.exists(output_gen_dir):
                    output_gen_dir = os.path.join(input_dir, 'oneobjlit_gen')
                os.makedirs(output_gen_dir, exist_ok=True)

                for i in range(len(oneobjgroomlit_files)):
                    oneobjgroom_img = cv2.imread(oneobjgroomlit_files[i], cv2.IMREAD_UNCHANGED)
                    mask_img = cv2.imread(mask_files[i], cv2.IMREAD_COLOR)

                    if oneobjgroom_img is None or mask_img is None:
                        continue

                    mask_matches = np.all(mask_img == foreground_color[::-1], axis=2).astype(np.uint8) * 255

                    alpha_channel = oneobjgroom_img[:, :, 3] if oneobjgroom_img.shape[2] == 4 else np.ones(oneobjgroom_img.shape[:2], dtype=np.uint8) * 255

                    alpha_channel = 255 - alpha_channel

                    result_alpha = np.where(mask_matches == 0, alpha_channel, mask_matches)

                    result_img = np.ones((oneobjgroom_img.shape[0], oneobjgroom_img.shape[1], 4), dtype=np.uint8) * 255
                    result_img[:, :, 3] = 255 - result_alpha

                    output_path = os.path.join(output_gen_dir, f"{i}_oneobjlit.png")
                    cv2.imwrite(output_path, result_img)

                print(f"已生成 {len(oneobjgroomlit_files)} 张图片到 {output_gen_dir}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('--input-dir')
    args = parser.parse_args()
    try:
        generate_oneobjlit(args.input_dir)
    except Exception as e:
        print(e)