import os
import tarfile
import io
from pathlib import Path
from collections import defaultdict
import json
import gzip
import random
from typing import List, Tuple
import argparse

DATASET_ROOT = Path("./DatasetAutomationOutputDirectory")

DATEDIRT_TO_EXCLUDE = [
]


def get_all_render_items(dataset_root: Path) -> List[Tuple[Path, str]]:
    """
    Find all render items in the dataset structure.
    Supports both .tar.gz archives and folder formats.
    Returns: [(path, type), ...] where type is 'folder' or 'tar.gz'
    """
    render_items = []

    if not dataset_root.exists():
        return render_items

    for date_dir in dataset_root.iterdir():
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

                for render_item in scene_instance_dir.iterdir():
                    if render_item.is_dir():
                        # Folder format
                        render_items.append((render_item, 'folder'))
                    elif render_item.is_file() and render_item.suffix == '.gz' and render_item.stem.endswith('.tar'):
                        # .tar.gz format
                        render_items.append((render_item, 'tar.gz'))

    return render_items


def get_render_name(item: Tuple[Path, str]) -> str:
    """Extract render name from path."""
    path, item_type = item
    if item_type == 'folder':
        return path.name
    else:  # tar.gz
        return path.stem  # removes .gz, leaving .tar filename


def extract_overview_from_folder(folder_path: Path) -> dict:
    """从文件夹中提取 overview.json"""
    try:
        json_path = folder_path / "overview.json"
        if json_path.exists():
            with open(json_path, 'rb') as f:
                content = f.read()
                return try_decode_content(content)

        gz_path = folder_path / "overview.json.gz"
        if gz_path.exists():
            with gzip.open(gz_path, 'rb') as f:
                content = f.read()
                return try_decode_content(content)
    except Exception as e:
        print(f"Error extracting overview from {folder_path}: {e}")
        return None
    return None


def extract_overview_from_tar(tar_path: Path):
    """从 tar.gz 中提取 overview.json，支持多种编码和.gz 压缩"""
    try:
        with open(tar_path, 'rb') as f:
            tar_bytes = f.read()

        tar_buffer = io.BytesIO(tar_bytes)

        with tarfile.open(fileobj=tar_buffer, mode="r:gz") as tar:
            members = tar.getmembers()

            # 先找 overview.json
            for m in members:
                if m.name.endswith('overview.json') and not m.name.endswith('.gz'):
                    file_obj = tar.extractfile(m)
                    if file_obj:
                        content = file_obj.read()
                        return try_decode_content(content)

            # 再找 overview.json.gz
            for m in members:
                if m.name.endswith('overview.json.gz'):
                    file_obj = tar.extractfile(m)
                    if file_obj:
                        compressed = file_obj.read()
                        content = gzip.decompress(compressed)
                        return try_decode_content(content)
    except Exception as e:
        print(f"Error extracting overview from {tar_path}: {e}")
        return None
    return None


def extract_overview(item: Tuple[Path, str]) -> dict:
    """Extract overview.json from render item (supports both folder and tar.gz)"""
    path, item_type = item
    if item_type == 'folder':
        return extract_overview_from_folder(path)
    else:
        return extract_overview_from_tar(path)


def try_decode_content(content: bytes):
    """尝试多种编码解码并解析 JSON"""
    encodings = [
        'utf-8-sig',  # UTF-8 with BOM
        'utf-16',     # 自动检测 BOM (UTF-16 LE/BE)
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


def get_orientation_from_overview(overview: dict) -> str | None:
    """从 overview.json 中获取横竖屏信息"""
    if overview is None:
        return None

    width = overview.get("Width", 0)
    height = overview.get("Height", 0)

    if width > 0 and height > 0:
        if width > height:
            return 'landscape'
        elif height > width:
            return 'portrait'
        else:
            return 'square'

    return None


def main():
    parser = argparse.ArgumentParser(description="Render Counter & Orientation Statistics")
    parser.add_argument("--sample", type=int, default=100,
                        help="Number of items to sample for orientation statistics (default: 100)")
    parser.add_argument("--dataset-root", type=Path, default=DATASET_ROOT,
                        help="Dataset root directory (default: ./DatasetAutomationOutputDirectory)")
    args = parser.parse_args()

    print(f"{'='*80}")
    print(f"Render Counter & Orientation Statistics")
    print(f"{'='*80}\n")

    dataset_root = args.dataset_root
    sample_size = args.sample

    print(f"Dataset root: {dataset_root.resolve()}")

    if not dataset_root.exists():
        print(f"ERROR: Dataset root does not exist: {dataset_root.resolve()}")
        return

    # Temporarily override global for get_all_render_items
    print(f"Collecting all render items (folder + tar.gz)...")
    render_items = get_all_render_items(dataset_root)
    total_items = len(render_items)

    # Count by type
    folder_count = sum(1 for _, t in render_items if t == 'folder')
    tar_count = sum(1 for _, t in render_items if t == 'tar.gz')

    if total_items == 0:
        print(f"No render items found.")
        return

    print(f"Total render items: {total_items}")
    print(f"  - Folders: {folder_count}")
    print(f"  - Tar.gz archives: {tar_count}\n")

    # Count renders by date and scene
    date_scene_counts = defaultdict(lambda: defaultdict(int))
    render_counts = defaultdict(int)

    for item in render_items:
        path, item_type = item
        render_name = get_render_name(item)
        render_counts[render_name] += 1

        # Get date and scene from path
        parts = path.relative_to(dataset_root).parts
        if len(parts) >= 4:
            date_dir = parts[0]
            scene_dir = parts[1]
            date_scene_counts[date_dir][scene_dir] += 1

    print(f"{'='*80}")
    print(f"Render Statistics by Date:")
    print(f"{'='*80}")

    total_renders = 0
    for date_dir in sorted(date_scene_counts.keys()):
        print(f"\n{date_dir}:")
        date_total = 0
        for scene_dir, count in sorted(date_scene_counts[date_dir].items(), key=lambda x: -x[1]):
            print(f"  {scene_dir}: {count} renders")
            date_total += count
        print(f"  Total: {date_total} renders")
        total_renders += date_total

    print(f"\n{'='*80}")
    print(f"Grand Total: {total_renders} renders")
    print(f"{'='*80}")

    print(f"\n{'='*80}")
    print(f"Render Type Breakdown:")
    print(f"{'='*80}")
    print(f"Total unique render types: {len(render_counts)}")
    print(f"\nRender type breakdown:")
    for render_name, count in sorted(render_counts.items(), key=lambda x: -x[1]):
        print(f"  {render_name}: {count} archives")

    # Sample orientation
    print(f"\n{'='*80}")
    print(f"Orientation Sampling (sampling up to {sample_size} items)...")
    print(f"{'='*80}")

    # Sample render items
    actual_sample_size = min(sample_size, total_items)
    sampled_items = random.sample(render_items, actual_sample_size)

    orientation_counts = defaultdict(int)
    successful_samples = 0
    error_samples = 0

    print(f"Sampling {actual_sample_size} items...")

    for i, item in enumerate(sampled_items):
        overview = extract_overview(item)
        orientation = get_orientation_from_overview(overview)

        if orientation:
            orientation_counts[orientation] += 1
            successful_samples += 1
        else:
            error_samples += 1
            if error_samples <= 5:
                print(f"  [Warning] Failed to get orientation from: {item[0].name}")

        if (i + 1) % 200 == 0:
            print(f"  Processed: {i+1}/{actual_sample_size}")

    if error_samples > 5:
        print(f"  ... and {error_samples - 5} more warnings")

    print(f"\nSuccessful samples: {successful_samples}/{actual_sample_size}")
    if error_samples > 0:
        print(f"Failed to get orientation: {error_samples} (overview.json missing or invalid)")

    if successful_samples > 0:
        landscape_count = orientation_counts.get('landscape', 0)
        portrait_count = orientation_counts.get('portrait', 0)
        square_count = orientation_counts.get('square', 0)

        print(f"\nOrientation breakdown:")
        print(f"  Landscape (横屏): {landscape_count} ({landscape_count/successful_samples*100:.2f}%)")
        print(f"  Portrait (竖屏):  {portrait_count} ({portrait_count/successful_samples*100:.2f}%)")
        if square_count > 0:
            print(f"  Square (方形):     {square_count} ({square_count/successful_samples*100:.2f}%)")

        # Estimate total orientation based on sample
        if sample_size < total_items:
            scale_factor = total_items / sample_size
            print(f"\nEstimated total orientation (extrapolated from sample):")
            print(f"  Landscape (横屏): ~{int(landscape_count * scale_factor)} ({landscape_count/successful_samples*100:.2f}%)")
            print(f"  Portrait (竖屏):  ~{int(portrait_count * scale_factor)} ({portrait_count/successful_samples*100:.2f}%)")
            if square_count > 0:
                print(f"  Square (方形):     ~{int(square_count * scale_factor)} ({square_count/successful_samples*100:.2f}%)")
        else:
            print(f"\n(Sampled all items, no extrapolation needed)")

    print(f"\n{'='*80}")
    print(f"Summary:")
    print(f"{'='*80}")
    print(f"Total render items: {total_items} (Folders: {folder_count}, Tar.gz: {tar_count})")
    print(f"Total unique render types: {len(render_counts)}")
    if successful_samples > 0:
        print(f"Sampled orientation: {landscape_count} landscape / {portrait_count} portrait")


if __name__ == "__main__":
    main()
