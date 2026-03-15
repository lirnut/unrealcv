import os
import re
import numpy as np
import time
from pathlib import Path
from multiprocessing import Pool, cpu_count
from typing import List, Dict, Tuple
from collections import defaultdict
import json
import argparse
import shutil
import subprocess
import gzip
import tarfile

DATASET_ROOT = Path("./DatasetAutomationOutputDirectory")
EXPECTED_ONEOBJLIT_FILES = 90
EXPECTED_RGB_PNG_FILES = 90

EXPECTED_ONEOBJLIT_FILES_2 = 121
EXPECTED_RGB_PNG_FILES_2 = 121

DATEDIRT_TO_EXCLUDE = [
]


GOOD_SAFE_POINTS = [
    [-27375.68, 6687.37, 105.01],
    [-27666.99, 10568.83, 118.72],
    [-19336.28, 21354.54, 118.78],
    [-4949.28, 23774.72, 107.98],
    [-2478.06, 22480.86, 105.45],
    [4754.28, 17816.05, 109.57],
    [17239.71, 10954.90, 97.47],
    [18035.16, 8842.40, 93.03],
    [26767.51, 18510.71, 105.09],
    [36555.64, 11710.85, 93.45],
    [-39749.49, -211.39, 113.89],
    [-41586.18, 2506.41, 116.15],
    [-54020.59, 5368.79, 94.02],
    [-81451.83, 16336.29, 106.19],
    [-21271.80, 48646.66, 106.35],
    [-21001.02, 48859.27, 93.08],
    [-13500.29, 51378.69, 105.00],
    [-10254.83, 48164.48, 118.39],
    [-8694.04, 46520.80, 118.35],
    [-7035.06, 44709.65, 118.33],
    [9931.92, 38757.32, 106.01],
    [15221.91, 38919.36, 105.22],
    [15650.72, 38878.29, 105.02],
    [15809.77, 38863.06, 105.02],
    [16158.94, 38829.62, 105.02],
    [17371.56, 38913.68, 105.02],
    [17977.48, 38765.33, 105.01],
    [18406.21, 38723.58, 105.01],
    [19163.18, 38681.80, 105.01],
    [19886.89, 38643.80, 105.01],
    [20275.87, 38606.55, 105.01],
    [22478.44, 36945.86, 117.18],
    [22478.44, 36945.86, 117.18],
    [22640.63, 36508.87, 108.00],
    [23281.28, 35707.68, 110.19],
    [23543.45, 35192.86, 108.01],
    [40834.14, 22203.84, 111.82],
]


def run_genvid_for_render(args_tuple: Tuple[str, str, int]) -> Dict[str, any]:
    """
    Run genvid.py for a single render directory.
    Used for parallel processing.

    Args:
        args_tuple: (render_path, genvid_script, fps)
    """
    render_path, genvid_script, fps = args_tuple

    try:
        cmd = [
            "python",
            genvid_script,
            "--input-dir", render_path,
            "--fps", str(fps),
            "--time_delay", "0"
        ]

        result = subprocess.run(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            shell=False
        )

        if result.returncode == 0:
            return {"status": "success", "path": render_path}
        else:
            return {"status": "failed", "path": render_path, "error": result.stderr.strip()}
    except Exception as e:
        return {"status": "failed", "path": render_path, "error": str(e)}



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


def check_render_directory(render_path: Path) -> Dict[str, any]:
    issues = []
    oneobjlit_count = 0
    rgb_png_count = 0
    should_delete = False
    need_genvid = False

    rgb_mp4 = render_path / "rgb.mp4"
    if not rgb_mp4.exists():
        issues.append("Missing rgb.mp4")

    overview_json_path = render_path / "overview.json"
    overview_json_gz_path = render_path / "overview.json.gz"
    overview = None
    resolution = "unknown"

    # slow
    if not overview_json_path.exists() and not overview_json_gz_path.exists():
        issues.append("Missing overview.json (or overview.json.gz)")
        should_delete = True
    else:
        if overview_json_path.exists():
            with open(overview_json_path, "rb") as f:
                content = f.read()
                overview = try_decode_content(content)
        elif overview_json_gz_path.exists():
            with gzip.open(overview_json_gz_path, "rb") as f:
                content = f.read()
                overview = try_decode_content(content)

        if overview is None:
            issues.append("Failed to decode overview.json (or overview.json.gz)")
        else:
            resolution = overview.get("Resolution", "unknown")


    metadata_dir = render_path / "metadata"
    if not metadata_dir.exists():
        issues.append("Missing metadata directory")
        should_delete = True
    else:
        step_metadatas = os.listdir(metadata_dir)
        pattern_json = re.compile(r'^(\d+)_(.+)\.json$')
        valid_files = []
        for file in step_metadatas:
            match = pattern_json.match(file)
            if match:
                step_id = int(match.group(1))
                valid_files.append((step_id, file))

        if len(valid_files) != EXPECTED_ONEOBJLIT_FILES and len(valid_files) != EXPECTED_ONEOBJLIT_FILES_2:
            issues.append(f"metadata has {len(valid_files)} files (expected {EXPECTED_ONEOBJLIT_FILES} or {EXPECTED_ONEOBJLIT_FILES_2})")
            should_delete = True
        else:
            # load the first step metadata
            valid_files.sort(key=lambda x: x[0])
            first_step_metadata = valid_files[0][1]
            first_step_metadata_path = metadata_dir / first_step_metadata
            with open(first_step_metadata_path, "rb") as f:
                content = f.read()
                first_step_metadata = try_decode_content(content)
            if first_step_metadata is None:
                issues.append(f"Failed to decode step metadata")
            else:
                get_xyz = lambda x: np.array([x["X"], x["Y"], x["Z"]])
                # get_xy = lambda x: np.array([x["X"], x["Y"]])
                # camera_location = get_xy(first_step_metadata["CameraLocation"])
                # foreground_location = get_xy(first_step_metadata["ForegroundLocation"])
                # hori_dist = np.linalg.norm(camera_location - foreground_location)
                # if hori_dist < 50:
                #     issues.append(f"Camera and foreground location are too close, {hori_dist}")
                #     if hori_dist < 30:
                #         should_delete = True
                good_safe_point_matched = False
                # camera_location = get_xyz(first_step_metadata["CameraLocation"])
                foreground_location = get_xyz(first_step_metadata["ForegroundLocation"])
                for good_safe_point in GOOD_SAFE_POINTS:
                    good_safe_point = np.array(good_safe_point)
                    dist = np.linalg.norm(foreground_location - good_safe_point)
                    if dist < 100:
                        print("good safe point matched")
                        good_safe_point_matched = True
                        break
                
                if not good_safe_point_matched:
                    issues.append(f"Foreground location {foreground_location} is not matched with any good safe point")
                    should_delete = True


    oneobjlit_dir = render_path / "oneobjlit"
    if not oneobjlit_dir.exists():
        need_genvid = True
        issues.append("Missing oneobjlit directory")
    else:
        file_count = len(list(oneobjlit_dir.iterdir()))
        oneobjlit_count = file_count
        if file_count != EXPECTED_ONEOBJLIT_FILES and file_count != EXPECTED_ONEOBJLIT_FILES_2:
            issues.append(f"oneobjlit has {file_count} files (expected {EXPECTED_ONEOBJLIT_FILES})")
            if  file_count < EXPECTED_ONEOBJLIT_FILES or EXPECTED_ONEOBJLIT_FILES < file_count < EXPECTED_ONEOBJLIT_FILES_2:
                should_delete = True

    rgb_dir = render_path / "rgb"
    if rgb_dir.exists() and rgb_dir.is_dir():
        png_files = list(rgb_dir.glob("*.png"))
        rgb_png_count = len(png_files)
        if rgb_png_count == EXPECTED_RGB_PNG_FILES:
            need_genvid = True
        if rgb_png_count == EXPECTED_RGB_PNG_FILES_2:
            need_genvid = True
        elif 1 <= rgb_png_count < EXPECTED_RGB_PNG_FILES:
            issues.append(f"rgb count incorrect {rgb_png_count}")
            should_delete = True
        elif EXPECTED_RGB_PNG_FILES < rgb_png_count < EXPECTED_RGB_PNG_FILES_2:
            issues.append(f"rgb count incorrect {rgb_png_count}")
            should_delete = True
        elif rgb_png_count == 0:
            if not rgb_mp4.exists():
                issues.append("0 rgb png while no rgb.mp4")
                should_delete = True
            else:
                mp4_sz = os.path.getsize(rgb_mp4)
                if mp4_sz < 20 * 1024: # 20 K Byte
                    issues.append(f"0 rgb png while rgb.mp4 size invalid {mp4_sz/1024}KB")
                    should_delete = True

    else:
        if not rgb_mp4.exists():
            issues.append("no rgb folder while no rgb.mp4")
            should_delete = True
        else:
            mp4_sz = os.path.getsize(rgb_mp4)
            if mp4_sz < 20 * 1024: # 20 K Byte
                issues.append(f"no rgb folder while rgb.mp4 size invalid {mp4_sz/1024}KB")
                should_delete = True


    return {
        "path": str(render_path),
        "issues": issues,
        "valid": len(issues) == 0,
        "oneobjlit_count": oneobjlit_count,
        "rgb_png_count": rgb_png_count,
        "resolution": resolution,
        "should_delete": should_delete,
        "need_genvid": need_genvid
    }


def get_all_render_paths() -> List[Path]:
    render_paths = []

    for date_dir in DATASET_ROOT.iterdir():
        if not date_dir.is_dir():
            continue

        if date_dir.name in DATEDIRT_TO_EXCLUDE:
            continue

        for scene_dir in date_dir.iterdir():
            if not scene_dir.is_dir():
                continue

            for scene_instance_dir in scene_dir.iterdir():
                if not scene_instance_dir.is_dir():
                    continue
                if not scene_instance_dir.name.startswith("scene_"):
                    continue

                for render_dir in scene_instance_dir.iterdir():
                    if render_dir.is_dir():
                        render_paths.append(render_dir)

    return render_paths

def get_all_packaged_render_paths() -> List[Path]:
    render_paths = []

    for date_dir in DATASET_ROOT.iterdir():
        if not date_dir.is_dir():
            continue

        if date_dir.name in DATEDIRT_TO_EXCLUDE:
            continue

        for scene_dir in date_dir.iterdir():
            if not scene_dir.is_dir():
                continue

            for scene_instance_dir in scene_dir.iterdir():
                if not scene_instance_dir.is_dir():
                    continue
                if not scene_instance_dir.name.startswith("scene_"):
                    continue

                for render_dir in scene_instance_dir.iterdir():
                    if not render_dir.is_dir() and render_dir.name.endswith(".tar.gz"):
                        render_paths.append(render_dir)

    return render_paths

def pack_render_directory(render_path_str: str) -> Dict[str, any]:
    """Pack a single render directory into .tar.gz (for multiprocessing)"""
    import tarfile

    render_path = Path(render_path_str)
    tar_path = render_path.parent / f"{render_path.name}.tar.gz"

    if tar_path.exists():
        return {"status": "skipped", "path": str(render_path), "reason": "already packed"}

    try:
        # Calculate size before
        size_before = sum(f.stat().st_size for f in render_path.rglob('*') if f.is_file())

        # Create tar.gz archive
        with tarfile.open(tar_path, "w:gz", compresslevel=9) as tar:
            tar.add(render_path, arcname=render_path.name)

        size_after = tar_path.stat().st_size

        # Remove original directory after successful packing
        if os.path.exists(tar_path):
            shutil.rmtree(render_path)
        else:
            print(f"Warning: Tar archive failed to create for {render_path}")
            return {"status": "failed", "path": str(render_path), "error": "Tar archive not created"}

        print(f"Successfully packed {render_path} to {tar_path}")
        return {
            "status": "success",
            "path": str(render_path),
            "size_before": size_before,
            "size_after": size_after
        }
    except Exception as e:
        return {"status": "failed", "path": str(render_path), "error": str(e)}


def unpack_render_directory(render_path_str: str) -> Dict[str, any]:
    """Unpack a single .tar.gz archive (for multiprocessing)"""
    import tarfile

    render_path = Path(render_path_str)
    tar_path = render_path.parent / f"{render_path.name}.tar.gz"

    # Check if tar.gz exists
    if not tar_path.exists():
        # Maybe render_path is the tar.gz file itself
        if render_path.suffix == '.gz' and render_path.stem.endswith('.tar'):
            tar_path = render_path
            unpack_dir = render_path.parent / render_path.stem.replace('.tar', '')
        else:
            print(f"Warning: No tar.gz found for {render_path}")
            return {"status": "skipped", "path": str(render_path), "reason": "no tar.gz found"}
    else:
        unpack_dir = render_path

    if unpack_dir.exists():
        print(f"Warning: Unpack directory {unpack_dir} already exists for {render_path}")
        return {"status": "skipped", "path": str(render_path), "reason": "already unpacked"}

    try:
        size_before = tar_path.stat().st_size

        # Extract tar.gz archive
        with tarfile.open(tar_path, "r:gz") as tar:
            tar.extractall(path=tar_path.parent)

        # Calculate size after
        size_after = sum(f.stat().st_size for f in unpack_dir.rglob('*') if f.is_file())

        if unpack_dir.exists():
            if size_after >= size_before:
                tar_path.unlink()
            else:
                print(f"Warning: Unpacked size {size_after} is smaller than before {size_before} for {render_path}, probably corrupted, won't delete original tar")

        print(f"Successfully unpacked {tar_path} to {unpack_dir}")
        return {
            "status": "success",
            "path": str(render_path),
            "size_before": size_before,
            "size_after": size_after
        }
    except Exception as e:
        print(f"Warning: Failed to unpack {tar_path} to {unpack_dir}")
        return {"status": "failed", "path": str(render_path), "error": str(e)}


def main():
    parser = argparse.ArgumentParser(description="Dataset consistency checker")
    parser.add_argument("--path", type=str, default="./DatasetAutomationOutputDirectory",
                        help="Path to dataset root directory (default: ./DatasetAutomationOutputDirectory)")
    parser.add_argument("--delete", action="store_true",
                        help=f"Delete scene folders with: missing overview.json, oneobjlit==0, or 1<=oneobjlit<{EXPECTED_ONEOBJLIT_FILES}")
    parser.add_argument("--delete-all", action="store_true",
                        help="Delete ALL inconsistent scene folders (any scene with issues)")
    parser.add_argument("--genvid", action="store_true",
                        help=f"Run genvid.py for renders with exactly {EXPECTED_RGB_PNG_FILES} png files in rgb folder but missing rgb.mp4")
    parser.add_argument("--fps", type=int, default=30,
                        help="FPS for genvid.py (default: 30)")
    parser.add_argument("--genvid-parallel", type=int, default=1,
                        help=f"Number of parallel processes for genvid.py (default: 1, sequential)")
    parser.add_argument("--delete-mode-dir", type=str, metavar="DIRNAME",
                        help="Delete specific subdirectory (e.g., 'rgb') from all render directories recursively")
    parser.add_argument("--compress", action="store_true",
                        help="Compress overview.json files to overview.json.gz (gzip)")
    parser.add_argument("--decompress", action="store_true",
                        help="Decompress overview.json.gz files back to overview.json")
    parser.add_argument("--compress-keep-original", action="store_true",
                        help="Keep original overview.json after compression (default: remove original)")
    parser.add_argument("--pack", action="store_true",
                        help="Pack each render directory into a .tar.gz archive to reduce small files")
    parser.add_argument("--unpack", action="store_true",
                        help="Unpack .tar.gz archives back to render directories")
    parser.add_argument("--pack-parallel", type=int, default=1,
                        help="Number of parallel processes for packing (default: 0, sequential)")

    args = parser.parse_args()

    global DATASET_ROOT
    DATASET_ROOT = Path(args.path)

    print(f"{'='*80}")
    print(f"Dataset Consistency Checker")
    if args.delete:
        print(f"MODE: DELETE SPECIFIC SCENES (no overview.json / oneobjlit==0 / 1<=oneobjlit<{EXPECTED_ONEOBJLIT_FILES})")
    if args.delete_all:
        print(f"MODE: DELETE ALL INCONSISTENT SCENES")
    if args.genvid:
        if args.genvid_parallel > 1:
            print(f"MODE: AUTO GENVID (FPS={args.fps}, PARALLEL={args.genvid_parallel})")
        else:
            print(f"MODE: AUTO GENVID (FPS={args.fps})")
    if args.delete_mode_dir:
        print(f"MODE: DELETE MODE DIR '{args.delete_mode_dir}' from all render directories")
    if args.compress:
        print(f"MODE: COMPRESS overview.json -> overview.json.gz")
    if args.decompress:
        print(f"MODE: DECOMPRESS overview.json.gz -> overview.json")
    if args.pack:
        print(f"MODE: PACK render directories -> .tar.gz archives")
    if args.unpack:
        print(f"MODE: UNPACK .tar.gz archives -> render directories")
    print(f"{'='*80}\n")

    print(f"Dataset root: {DATASET_ROOT.resolve()}")

    if not DATASET_ROOT.exists():
        print(f"ERROR: Dataset root does not exist: {DATASET_ROOT.resolve()}")
        return

    print(f"Collecting all render directories...")

    render_paths = get_all_render_paths()
    total_renders = len(render_paths)

    if args.pack or args.unpack:
        print(f"\n{'='*80}")
        if args.pack:
            print(f"Packing render directories ({args.pack_parallel} parallel processes)...")
        else:
            print(f"Unpacking .tar.gz archives ({args.pack_parallel} parallel processes)...")
        print(f"{'='*80}")

        # Convert to strings for multiprocessing
        with Pool(processes=args.pack_parallel) as pool:
            if args.pack:
                render_path_strs = [str(p) for p in render_paths]
                results = pool.map(pack_render_directory, render_path_strs)
            else:
                packaged_render_paths_strs = [str(p) for p in get_all_packaged_render_paths()]
                results = pool.map(unpack_render_directory, packaged_render_paths_strs)

        # Aggregate results
        success_count = sum(1 for r in results if r["status"] == "success")
        skipped_count = sum(1 for r in results if r["status"] == "skipped")
        failed_count = sum(1 for r in results if r["status"] == "failed")
        total_size_before = sum(r.get("size_before", 0) for r in results if r["status"] == "success")
        total_size_after = sum(r.get("size_after", 0) for r in results if r["status"] == "success")

        # Print failed items
        for r in results:
            if r["status"] == "failed":
                print(f"✗ Failed: {r['path']} - {r.get('error', 'unknown error')}")

        print(f"\n{'='*80}")
        if args.pack:
            print(f"Packing Summary:")
            print(f"{'='*80}")
            print(f"Packed: {success_count} directories")
            print(f"Skipped: {skipped_count} (already packed)")
            if failed_count > 0:
                print(f"Failed: {failed_count}")
            if success_count > 0:
                print(f"Total size before: {total_size_before / (1024*1024):.2f} MB")
                print(f"Total size after: {total_size_after / (1024*1024):.2f} MB")
                if total_size_before > 0:
                    ratio = (1 - total_size_after / total_size_before) * 100
                    print(f"Compression ratio: {ratio:.1f}%")
        else:
            print(f"Unpacking Summary:")
            print(f"{'='*80}")
            print(f"Unpacked: {success_count} archives")
            print(f"Skipped: {skipped_count} (already unpacked or not found)")
            if failed_count > 0:
                print(f"Failed: {failed_count}")
        print(f"\nOperation complete!")
        return



    if total_renders == 0:
        print(f"No render directories found. Please check the dataset path.")
        return

    print(f"Total render directories found: {total_renders}")

    if args.delete_mode_dir:
        print(f"\n{'='*80}")
        print(f"Deleting '{args.delete_mode_dir}' directories from all render paths...")
        print(f"{'='*80}")

        deleted_dirs = 0
        failed_dirs = 0

        for render_path in render_paths:
            target_dir = render_path / args.delete_mode_dir
            if target_dir.exists() and target_dir.is_dir():
                try:
                    shutil.rmtree(target_dir)
                    print(f"✓ Deleted: {target_dir}")
                    deleted_dirs += 1
                except Exception as e:
                    print(f"✗ Failed to delete {target_dir}: {e}")
                    failed_dirs += 1

        print(f"\n{'='*80}")
        print(f"Delete Mode Dir Summary:")
        print(f"{'='*80}")
        print(f"Successfully deleted: {deleted_dirs} directories")
        if failed_dirs > 0:
            print(f"Failed to delete: {failed_dirs} directories")
        print(f"\nDelete mode dir operation complete!")
        return

    if args.compress or args.decompress:
        print(f"\n{'='*80}")
        if args.compress:
            print(f"Compressing overview.json files...")
        else:
            print(f"Decompressing overview.json.gz files...")
        print(f"{'='*80}")

        processed_count = 0
        skipped_count = 0
        failed_count = 0
        total_size_before = 0
        total_size_after = 0

        for render_path in render_paths:
            if args.compress:
                json_file = render_path / "overview.json"
                gz_file = render_path / "overview.json.gz"

                if not json_file.exists():
                    skipped_count += 1
                    continue

                try:
                    with open(json_file, 'rb') as f_in:
                        with gzip.open(gz_file, 'wb', compresslevel=9) as f_out:
                            f_out.writelines(f_in)

                    size_before = json_file.stat().st_size
                    size_after = gz_file.stat().st_size
                    total_size_before += size_before
                    total_size_after += size_after

                    if not args.compress_keep_original:
                        json_file.unlink()

                    processed_count += 1
                    if processed_count % 100 == 0:
                        print(f"  Progress: {processed_count} files compressed...")
                except Exception as e:
                    print(f"✗ Failed to compress {json_file}: {e}")
                    failed_count += 1

            else:  # decompress
                gz_file = render_path / "overview.json.gz"
                json_file = render_path / "overview.json"

                if not gz_file.exists():
                    skipped_count += 1
                    continue

                try:
                    with gzip.open(gz_file, 'rb') as f_in:
                        with open(json_file, 'wb') as f_out:
                            f_out.writelines(f_in)

                    size_before = gz_file.stat().st_size
                    size_after = json_file.stat().st_size
                    total_size_before += size_before
                    total_size_after += size_after

                    processed_count += 1
                    if processed_count % 100 == 0:
                        print(f"  Progress: {processed_count} files decompressed...")
                except Exception as e:
                    print(f"✗ Failed to decompress {gz_file}: {e}")
                    failed_count += 1

        print(f"\n{'='*80}")
        if args.compress:
            print(f"Compression Summary:")
        else:
            print(f"Decompression Summary:")
        print(f"{'='*80}")
        print(f"Processed: {processed_count} files")
        print(f"Skipped: {skipped_count} files")
        if failed_count > 0:
            print(f"Failed: {failed_count} files")
        if processed_count > 0:
            print(f"Total size before: {total_size_before / (1024*1024):.2f} MB")
            print(f"Total size after: {total_size_after / (1024*1024):.2f} MB")
            if args.compress:
                ratio = (1 - total_size_after / total_size_before) * 100
                print(f"Compression ratio: {ratio:.1f}%")
        print(f"\nOperation complete!")
        return


    date_stats = defaultdict(lambda: defaultdict(int))
    for render_path in render_paths:
        date_dir = render_path.parent.parent.parent.name
        scene_dir = render_path.parent.parent.name
        date_stats[date_dir][scene_dir] += 1

    print(f"\n{'='*80}")
    print(f"Dataset Statistics by Date:")
    print(f"{'='*80}")
    for date_dir in sorted(date_stats.keys()):
        print(f"\n{date_dir}:")
        for scene_dir, count in sorted(date_stats[date_dir].items()):
            print(f"  {scene_dir}: {count} renders")
        print(f"  Total: {sum(date_stats[date_dir].values())} renders")

    print(f"\n{'='*80}")
    print(f"Checking dataset consistency (using {cpu_count()} processes)...")
    print(f"{'='*80}\n")

    with Pool(processes=cpu_count()) as pool:
        results = pool.map(check_render_directory, render_paths)

    valid_count = sum(1 for r in results if r["valid"])
    invalid_count = total_renders - valid_count

    # 统计每种 resolution 的数量
    resolution_stats = defaultdict(int)
    for result in results:
        resolution = result.get("resolution", "unknown")
        resolution_stats[resolution] += 1

    print(f"\n{'='*80}")
    print(f"Check Results:")
    print(f"{'='*80}")
    print(f"Total renders: {total_renders}")
    print(f"Valid renders: {valid_count} ({valid_count/total_renders*100:.2f}%)")
    print(f"Invalid renders: {invalid_count} ({invalid_count/total_renders*100:.2f}%)")

    # 输出 resolution 统计
    print(f"\n{'='*80}")
    print(f"Resolution Statistics:")
    print(f"{'='*80}")
    for resolution, count in sorted(resolution_stats.items(), key=lambda x: (-x[1], x[0])):
        print(f"  {resolution}: {count} renders ({count/total_renders*100:.2f}%)")

    scene_render_status = defaultdict(lambda: defaultdict(list))
    scenes_to_delete = set()
    scenes_to_delete_all = set()
    renders_need_genvid = []

    for result in results:
        render_path = Path(result["path"])
        scene_folder = render_path.parent
        scene_folder_str = str(scene_folder)
        date_scene_key = f"{scene_folder.parent.parent.name}\\{scene_folder.parent.name}"

        scene_render_status[date_scene_key][scene_folder_str].append(result["valid"])

        if result["should_delete"]:
            scenes_to_delete.add(scene_folder_str)

        if not result["valid"]:
            scenes_to_delete_all.add(scene_folder_str)

        if result["need_genvid"]:
            renders_need_genvid.append(result["path"])

    scene_validity = {}
    for date_scene_key, scenes in scene_render_status.items():
        complete_scenes = 0
        total_scenes = len(scenes)

        for scene_folder, render_validities in scenes.items():
            if all(render_validities):
                complete_scenes += 1

        scene_validity[date_scene_key] = {
            "complete": complete_scenes,
            "total": total_scenes
        }

    print(f"\n{'='*80}")
    print(f"Complete Scenes by Location (date\\scene):")
    print(f"{'='*80}")

    total_complete_scenes = 0
    total_scenes = 0
    for date_scene in sorted(scene_validity.keys()):
        stats = scene_validity[date_scene]
        complete_count = stats["complete"]
        total_count = stats["total"]
        total_complete_scenes += complete_count
        total_scenes += total_count
        print(f"{date_scene}: {complete_count}/{total_count} complete scenes")

    print(f"\n{'='*80}")
    print(f"Overall Complete Scene Summary:")
    print(f"{'='*80}")
    print(f"Total complete scenes: {total_complete_scenes}/{total_scenes} ({total_complete_scenes/total_scenes*100:.2f}%)")
    print(f"Total locations: {len(scene_validity)}")

    if len(renders_need_genvid) > 0:
        print(f"\n{'='*80}")
        print(f"Renders Ready for genvid ({EXPECTED_RGB_PNG_FILES} pngs in rgb/, missing rgb.mp4):")
        print(f"{'='*80}")
        print(f"Total: {len(renders_need_genvid)} renders")

        if args.genvid:
            print(f"\n{'='*80}")
            if args.genvid_parallel > 1:
                print(f"Starting genvid processes (parallel={args.genvid_parallel})...")
            else:
                print(f"Starting genvid processes (sequential)...")
            print(f"{'='*80}")

            genvid_script = Path(__file__).parent / "genvid.py"
            if not genvid_script.exists():
                genvid_script = Path(__file__).parent / "Windows" / "HUAWEI_Project" / "Saved" /"genvid.py"
            if not genvid_script.exists():
                genvid_script = Path(__file__).parent / "Windows" / "HillsideSampleProject" / "Saved" /"genvid.py"
            if not genvid_script.exists():
                genvid_script = Path(__file__).parent / "Windows" / "CitySample" / "Saved" /"genvid.py"

            success_count = 0
            failed_count = 0

            if args.genvid_parallel > 1:
                # Parallel processing using multiprocessing Pool
                genvid_args = [(render_path, str(genvid_script), args.fps) for render_path in renders_need_genvid]

                with Pool(processes=args.genvid_parallel) as pool:
                    results = pool.map(run_genvid_for_render, genvid_args)

                for result in results:
                    if result["status"] == "success":
                        success_count += 1
                    else:
                        print(f"✗ genvid failed: {result['path']}")
                        print(f"   error: {result.get('error', 'unknown error')}")
                        failed_count += 1

            else:
                # Sequential processing
                for render_path in renders_need_genvid:
                    try:
                        cmd = [
                            "python",
                            str(genvid_script),
                            "--input-dir", render_path,
                            "--fps", str(args.fps),
                            "--time_delay", "0"
                        ]

                        result = subprocess.run(
                            cmd,
                            stdout=None,
                            stderr=subprocess.PIPE,
                            text=True,
                            shell=False
                        )

                        if result.returncode == 0:
                            success_count += 1
                        else:
                            print(f"✗ genvid failed: {render_path}")
                            print(f"   stderr: {result.stderr.strip()}")
                            failed_count += 1
                    except Exception as e:
                        print(f"✗ Exception for {render_path}: {e}")
                        failed_count += 1

            print(f"\n{'='*80}")
            print(f"Genvid Summary:")
            print(f"{'='*80}")
            print(f"Success: {success_count}, Failed: {failed_count}")
        else:
            for render_path in renders_need_genvid:
                print(f"  {render_path}")
            print(f"\nUse --genvid flag to auto-run genvid.py for these renders")

    if (args.delete and len(scenes_to_delete) > 0) or (args.delete_all and len(scenes_to_delete_all) > 0):
        deletion_target = scenes_to_delete_all if args.delete_all else scenes_to_delete
        deletion_mode = "ALL INCONSISTENT" if args.delete_all else f"SPECIFIC (no overview.json / oneobjlit==0 / 1<=oneobjlit<{EXPECTED_ONEOBJLIT_FILES})"

        print(f"\n{'='*80}")
        print(f"Scenes to Delete ({deletion_mode}):")
        print(f"{'='*80}")
        print(f"Total scenes to delete: {len(deletion_target)}")

        for scene_path in sorted(deletion_target):
            print(f"  {scene_path}")

        print(f"\n{'='*80}")
        print(f"Deleting scenes...")
        print(f"{'='*80}")

        deleted_count = 0
        failed_count = 0

        for scene_path in deletion_target:
            try:
                shutil.rmtree(scene_path)
                print(f"✓ Deleted: {scene_path}")
                deleted_count += 1
            except Exception as e:
                print(f"✗ Failed to delete {scene_path}: {e}")
                failed_count += 1

        print(f"\n{'='*80}")
        print(f"Deletion Summary:")
        print(f"{'='*80}")
        print(f"Successfully deleted: {deleted_count} scenes")
        if failed_count > 0:
            print(f"Failed to delete: {failed_count} scenes")

    if invalid_count > 0:
        print(f"\n{'='*80}")
        print(f"Inconsistent Renders:")
        print(f"{'='*80}")

        issue_types = defaultdict(list)

        for result in results:
            if not result["valid"]:
                print(f"\n{result['path']}")
                for issue in result["issues"]:
                    print(f"  - {issue}")
                    issue_types[issue].append(result['path'])

        print(f"\n{'='*80}")
        print(f"Issue Summary:")
        print(f"{'='*80}")
        for issue_type, paths in sorted(issue_types.items(), key=lambda x: len(x[1]), reverse=True):
            print(f"\n{issue_type}: {len(paths)} occurrences")

        if len(scenes_to_delete) > 0:
            print(f"\n{'='*80}")
            print(f"Scenes matching --delete criteria: {len(scenes_to_delete)}")
            if not args.delete and not args.delete_all:
                print(f"Use --delete flag to remove these scenes")

        if len(scenes_to_delete_all) > 0:
            print(f"\n{'='*80}")
            print(f"All inconsistent scenes: {len(scenes_to_delete_all)}")
            if not args.delete_all:
                print(f"Use --delete-all flag to remove ALL inconsistent scenes")

        print(f"\n{'='*80}")
        print(f"Saving inconsistent paths to 'inconsistent_renders.json'...")

        inconsistent_data = {
            "total_inconsistent": invalid_count,
            "issue_types": {k: len(v) for k, v in issue_types.items()},
            "details": [r for r in results if not r["valid"]],
            "scenes_matching_delete_criteria": sorted(list(scenes_to_delete)),
            "scenes_all_inconsistent": sorted(list(scenes_to_delete_all)),
            "renders_need_genvid": renders_need_genvid
        }

        with open(DATASET_ROOT / "inconsistent_renders.json", "w", encoding="utf-8") as f:
            json.dump(inconsistent_data, f, indent=2, ensure_ascii=False)

        print(f"Saved to: {DATASET_ROOT / 'inconsistent_renders.json'}")
    else:
        print(f"\n✓ All renders are consistent!")

    print(f"\n{'='*80}")
    print(f"Check complete!")
    print(f"{'='*80}")


if __name__ == "__main__":
    main()
