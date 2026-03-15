import os
import json
import time
import re
import argparse
import cv2
import numpy as np
from collections import defaultdict
from tqdm import tqdm
from concurrent.futures import ThreadPoolExecutor, as_completed
import subprocess
import sys
from pathlib import Path
import random

# ── 唯一新增依赖：pip install imageio imageio-ffmpeg ──
# imageio-ffmpeg 自带静态编译的 ffmpeg 二进制，无需系统安装
import imageio_ffmpeg


parser = argparse.ArgumentParser(description='使用ffmpeg将n_xxx.png格式的图片序列转换为xxx.mp4视频')
parser.add_argument('--input-dir')
parser.add_argument('--fps', type=int, default=25)
parser.add_argument('--time_delay', type=float, default=0.0)
parser.add_argument('--max-workers', type=int, default=4)
args = parser.parse_args()

# ── 视频编码全局配置 ──
THIS_IS_A_CONFIG_CRF = 14            # CRF值，0=无损, 18≈视觉无损, 23=默认, 越小质量越高
# THIS_IS_A_CONFIG_PRESET = 'slower'   # 编码预设: ultrafast~veryslow, 越慢质量越高
# THIS_IS_A_CONFIG_PRESET = 'ultrafast'
# THIS_IS_A_CONFIG_PRESET = 'veryfast'
# THIS_IS_A_CONFIG_PRESET = 'fast'
THIS_IS_A_CONFIG_PRESET = 'medium'


def run_bg_genvid(input_dir, fps):
    cmd = ["python", f"{os.path.dirname(__file__)}/genvid.py"]
    cmd.extend(["--input-dir", input_dir])
    cmd.extend(["--fps", str(fps)])

    process = subprocess.Popen(
        cmd,
        stdout=sys.stdout,
        stderr=sys.stdout,
        shell=False,
        start_new_session=True
    )

    print(f"已启动后台进程，进程ID: {process.pid}")
    return process


# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
#  核心：用 imageio-ffmpeg 管道替代 cv2.VideoWriter
#  imageio_ffmpeg.write_frames() 返回 generator，通过 .send() 写帧
# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

def create_ffmpeg_writer(output_path: str, width: int, height: int, fps: float):
    """
    创建 ffmpeg 管道 writer。
    内部使用 imageio-ffmpeg 自带的 ffmpeg 二进制。
    """
    writer = imageio_ffmpeg.write_frames(
        output_path,
        (width, height),
        fps=fps,
        codec='libx264',
        pix_fmt_in='bgr24',       # cv2 imread 默认 BGR
        pix_fmt_out='yuv420p',     # 播放器兼容性最好
        macro_block_size=1, 
        output_params=[
            '-crf', str(THIS_IS_A_CONFIG_CRF),
            '-preset', THIS_IS_A_CONFIG_PRESET,
            '-movflags', '+faststart',
        ],
    )
    writer.send(None)  # 初始化 generator
    return writer


def write_frame(writer, frame: np.ndarray):
    """向 ffmpeg 管道写入一帧 (BGR numpy array)"""
    writer.send(frame.tobytes())


def close_writer(writer):
    """关闭管道，等待 ffmpeg 完成编码"""
    writer.close()


# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

def combine_3vids(video1_path, video2_path, video3_path, output_path):
    cap1 = cv2.VideoCapture(video1_path)
    cap2 = cv2.VideoCapture(video2_path)
    cap3 = cv2.VideoCapture(video3_path)

    fps = cap1.get(cv2.CAP_PROP_FPS)
    width = int(cap1.get(cv2.CAP_PROP_FRAME_WIDTH))
    height = int(cap1.get(cv2.CAP_PROP_FRAME_HEIGHT))

    writer = create_ffmpeg_writer(output_path, width * 3, height, fps)

    frames_buffer = []
    buffer_size = 30

    while True:
        ret1, frame1 = cap1.read()
        ret2, frame2 = cap2.read()
        ret3, frame3 = cap3.read()

        if not (ret1 and ret2 and ret3):
            break

        frames_buffer.append(cv2.hconcat([frame1, frame2, frame3]))

        if len(frames_buffer) >= buffer_size:
            for combined in frames_buffer:
                write_frame(writer, combined)
            frames_buffer.clear()

    for combined in frames_buffer:
        write_frame(writer, combined)

    close_writer(writer)
    cap1.release()
    cap2.release()
    cap3.release()


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


def run_combine_3vids(target_dir):
    video_combinations = [
        ('HIDE_rgb.mp4', [('rgb.mp4', 'mask_filtered.mp4', 'HIDE_rgb.mp4', 'rgb+mask_filtered+HIDE_rgb.mp4')]),
        ('rgb_no_target.mp4', [
            ('rgb.mp4', 'mask_filtered.mp4', 'rgb_no_target.mp4', 'rgb+mask_filtered+rgb_no_target.mp4'),
            ('rgb.mp4', 'mask.mp4', 'rgb_no_target.mp4', 'rgb+mask+rgb_no_target.mp4')
        ]),
        (None, [('obs.mp4', 'of0.mp4', 'of1.mp4', 'obs+of0+of1.mp4')])
    ]

    for check_file, combinations in video_combinations:
        if check_file and not os.path.exists(os.path.join(target_dir, check_file)):
            continue

        for combo in combinations:
            video1, video2, video3, output = combo
            paths = [os.path.join(target_dir, v) for v in [video1, video2, video3]]

            if all(os.path.exists(p) for p in paths):
                output_path = os.path.join(target_dir, output)
                combine_3vids(*paths, output_path)
                return

        if check_file:
            break


def process_sequence(seq_name: str, frames: list, args, output_dir: str) -> tuple:
    frames.sort(key=lambda x: x[0])
    sorted_files = [f[1] for f in frames]
    is_npy = sorted_files[0].endswith('.npy')

    if not is_npy:
        output_file = os.path.join(output_dir, f"{seq_name}.mp4")

        first_frame = cv2.imread(sorted_files[0])
        if first_frame is None:
            return (seq_name, False, f"无法读取图片: {sorted_files[0]}")

        height, width = first_frame.shape[:2]

        # ── ffmpeg 管道替代 cv2.VideoWriter ──
        writer = create_ffmpeg_writer(output_file, width, height, args.fps)

        need_resize = False
        frames_buffer = []
        buffer_size = 30

        for file_path in sorted_files:
            frame = cv2.imread(file_path)
            if frame is None:
                continue

            if frame.shape[:2] != (height, width):
                if not need_resize:
                    need_resize = True
                frame = cv2.resize(frame, (width, height))

            frames_buffer.append(frame)

            if len(frames_buffer) >= buffer_size:
                for buffered_frame in frames_buffer:
                    write_frame(writer, buffered_frame)
                frames_buffer.clear()

        for buffered_frame in frames_buffer:
            write_frame(writer, buffered_frame)

        close_writer(writer)

    if is_npy:
        output_file = os.path.join(output_dir, f"{seq_name}.npz")
        np.savez(output_file, *[np.load(f, allow_pickle=True) for f in sorted_files])

    keys_to_save_png = ('mask',)
    if any(key in seq_name.lower() for key in keys_to_save_png):
        rename_to_dir(output_dir, seq_name, sorted_files)
    else:
        delete_all(sorted_files)

    return (seq_name, True, f"成功生成视频: {output_file}")


def rename_to_dir(output_dir, seq_name, sorted_files):
    seq_name_dir = os.path.join(output_dir, seq_name)
    os.makedirs(seq_name_dir, exist_ok=True)

    for file_path in sorted_files:
        try:
            os.rename(file_path, os.path.join(seq_name_dir, os.path.basename(file_path)))
        except Exception:
            print(f"失败: {file_path}")
            pass


def delete_all(sorted_files):
    for file_path in sorted_files:
        try:
            os.remove(file_path)
        except Exception:
            print(f"删除失败: {file_path}")
            pass


def main():
    if args.time_delay > 0:
        print(f"Warning: time_delay is set to {args.time_delay} seconds")
        time.sleep(args.time_delay)

    output_dir = args.input_dir
    os.makedirs(output_dir, exist_ok=True)

    # 打印 ffmpeg 路径，确认使用 imageio-ffmpeg 自带的二进制
    print(f"FFmpeg 路径: {imageio_ffmpeg.get_ffmpeg_exe()}")
    print(f"编码参数: libx264 | CRF={THIS_IS_A_CONFIG_CRF} | preset={THIS_IS_A_CONFIG_PRESET}")

    pattern_png = re.compile(r'^(\d+)_(.+)\.png$')
    pattern_bmp = re.compile(r'^(\d+)_(.+)\.bmp$')
    pattern_npy = re.compile(r'^(\d+)_(.+)\.npy$')

    sequences = defaultdict(list)

    for filename in os.listdir(args.input_dir):
        lower_filename = filename.lower()
        if lower_filename.endswith('.png'):
            suffix = 'png'
            match = pattern_png.match(filename)
        elif lower_filename.endswith('.bmp'):
            suffix = 'bmp'
            match = pattern_bmp.match(filename)
        elif lower_filename.endswith('.npy'):
            suffix = 'npy'
            match = pattern_npy.match(filename)
        else:
            continue

        if match:
            frame_num = int(match.group(1))
            seq_name = match.group(2)
            if suffix == 'npy': seq_name += '_npy'
            file_path = os.path.join(args.input_dir, filename)
            sequences[seq_name].append((frame_num, file_path))

    if os.path.exists(os.path.join(args.input_dir, "rgb")):
        for filename in os.listdir(os.path.join(args.input_dir, "rgb")):
            lower_filename = filename.lower()
            if lower_filename.endswith('.png'):
                suffix = 'png'
                match = pattern_png.match(filename)
            else:
                continue

            if match:
                frame_num = int(match.group(1))
                seq_name = match.group(2)
                file_path = os.path.join(args.input_dir, "rgb", filename)
                sequences[seq_name].append((frame_num, file_path))
                print(file_path)

    if os.path.exists(os.path.join(args.input_dir, "depth")):
        for filename in os.listdir(os.path.join(args.input_dir, "depth")):
            lower_filename = filename.lower()
            if lower_filename.endswith('.png'):
                suffix = 'png'
                match = pattern_png.match(filename)
            else:
                continue

            if match:
                frame_num = int(match.group(1))
                seq_name = match.group(2)
                file_path = os.path.join(args.input_dir, "depth", filename)
                sequences[seq_name].append((frame_num, file_path))
                print(file_path)

    if not sequences:
        print("未找到符合格式的图片序列（格式应为：n_xxx.png）")
        return

    print(f"找到 {len(sequences)} 个序列，使用 {args.max_workers} 个工作线程并行处理...")

    with ThreadPoolExecutor(max_workers=args.max_workers) as executor:
        futures = {
            executor.submit(process_sequence, seq_name, frames, args, output_dir): seq_name
            for seq_name, frames in sequences.items()
        }

        with tqdm(total=len(futures), desc="处理序列") as pbar:
            for future in as_completed(futures):
                seq_name, success, message = future.result()
                if success:
                    tqdm.write(message)
                else:
                    tqdm.write(f"错误: {message}")
                pbar.update(1)

    print("所有序列处理完成！")

    try:
        run_combine_3vids(output_dir)
    except Exception:
        print("Combine failed")


def apply_alpha_to_rgb(rgb_video, oneobjlit_dir, output_dir):
    output_dir.mkdir(exist_ok=True)

    frame_idx = 0

    while True:
        ret, rgb_frame = rgb_video.read()
        if not ret:
            break

        alpha_png_path = oneobjlit_dir / f'{frame_idx}_oneobjlit.png'

        if alpha_png_path.exists():
            if random.random() < 0.03:
                alpha_img = cv2.imread(str(alpha_png_path), cv2.IMREAD_UNCHANGED)

                if alpha_img.shape[2] == 4:
                    alpha_channel = alpha_img[:, :, 3]
                else:
                    alpha_channel = np.mean(alpha_img[:, :, :3], axis=2).astype(np.uint8)

                alpha_normalized = alpha_channel.astype(np.float32) / 255.0
                alpha_reversed = 1.0 - alpha_normalized
                alpha_enhanced = np.power(alpha_reversed, 0.5)

                rgb_float = rgb_frame.astype(np.float32)
                alpha_3ch = np.stack([alpha_enhanced] * 3, axis=2)

                result = rgb_float * alpha_3ch
                result = np.clip(result, 0, 255).astype(np.uint8)

                result_with_alpha = np.dstack([result, (alpha_enhanced * 255).astype(np.uint8)])

                output_path = output_dir / f'{frame_idx}.png'
                cv2.imwrite(str(output_path), result_with_alpha)
                break


        frame_idx += 1

    rgb_video.release()
    print(f'Done! Processed {frame_idx} frames to {output_dir}')


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

def extra():

    overview_path = os.path.join(args.input_dir, "overview.json")
    overview = None
    if os.path.exists(overview_path):
        with open(overview_path, "rb") as f:
            content = f.read()
            overview = try_decode_content(content)
    else:
        print("overview.json not found!!!")
        if args.time_delay > 0:
            time.sleep(2)

    for filename in os.listdir(args.input_dir):
        if filename == 'oneobjlit' or filename == 'oneobjgroomlit':
            pngs = os.listdir(os.path.join(args.input_dir, filename))
            pngs = [os.path.join(args.input_dir, filename, f) for f in pngs]
            process_alpha_only_frames(pngs)

    oneobjgroomlit_files = None
    mask_files = None

    for dirname in os.listdir(args.input_dir):
        if dirname == 'oneobjgroomlit':
            pngs = os.listdir(os.path.join(args.input_dir, dirname))
            pattern_png = re.compile(r'^(\d+)_(.+)\.png$')

            frame_list = []
            for png in pngs:
                match = pattern_png.match(png)
                if match:
                    frame_num = int(match.group(1))
                    frame_list.append((frame_num, os.path.join(args.input_dir, dirname, png)))

            frame_list.sort(key=lambda x: x[0])
            oneobjgroomlit_files = [f[1] for f in frame_list]

    for dirname in os.listdir(args.input_dir):
        if dirname == 'mask':
            pngs = os.listdir(os.path.join(args.input_dir, dirname))
            pattern_png = re.compile(r'^(\d+)_(.+)\.png$')

            frame_list = []
            for png in pngs:
                match = pattern_png.match(png)
                if match:
                    frame_num = int(match.group(1))
                    frame_list.append((frame_num, os.path.join(args.input_dir, dirname, png)))

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

                output_gen_dir = os.path.join(args.input_dir, 'oneobjlit')
                if os.path.exists(output_gen_dir):
                    output_gen_dir = os.path.join(args.input_dir, 'oneobjlit_gen')
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
    
    apply_alpha_to_rgb(
        rgb_video=cv2.VideoCapture(os.path.join(args.input_dir, 'rgb.mp4')),
        oneobjlit_dir=Path(os.path.join(args.input_dir, 'oneobjlit')),
        output_dir=Path(os.path.join(args.input_dir, 'rgb-alpha-rev'))
    )


if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        print(e)
        if args.time_delay > 0:
            time.sleep(2)
    try:
        extra()
    except Exception as e:
        print(e)
        if args.time_delay > 0:
            time.sleep(2)

    print("genvid returned")
    if args.time_delay > 0:
        time.sleep(2)