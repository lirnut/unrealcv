import os
import time
from pathlib import Path
from multiprocessing import Pool, cpu_count, Process, Queue, Manager
from typing import List, Dict, Tuple, Optional
from collections import defaultdict
import json
import argparse
import tarfile
import io
import queue  # for Queue.Empty exception

DATASET_ROOT = Path("./DatasetAutomationOutputDirectory")
EXPECTED_ONEOBJLIT_FILES = 90
EXPECTED_RGB_PNG_FILES = 90

EXPECTED_ONEOBJLIT_FILES_2 = 121
EXPECTED_RGB_PNG_FILES_2 = 121

DATEDIRT_TO_EXCLUDE = [
]


def check_tar_from_buffer(tar_path: str, tar_bytes: bytes) -> Dict[str, any]:
    """
    Check a tar.gz archive from an in-memory buffer.
    This function is called by worker processes.
    """
    issues = []
    oneobjlit_count = 0
    rgb_png_count = 0
    should_delete = False
    need_genvid = False
    tar_size = len(tar_bytes)

    try:
        # Use BytesIO to treat the bytes as a file object
        tar_buffer = io.BytesIO(tar_bytes)

        # Open tar from memory
        with tarfile.open(fileobj=tar_buffer, mode="r:gz") as tar:
            # Get all member names
            members = tar.getmembers()

            # Check for rgb.mp4
            rgb_mp4_exists = any(m.name.endswith('rgb.mp4') for m in members)
            if not rgb_mp4_exists:
                issues.append("Missing rgb.mp4")

            # Check for overview.json or overview.json.gz
            overview_exists = any(m.name.endswith('overview.json') for m in members)
            overview_gz_exists = any(m.name.endswith('overview.json.gz') for m in members)

            if not overview_exists and not overview_gz_exists:
                issues.append("Missing overview.json (or overview.json.gz)")
                should_delete = True

            # Check for metadata directory
            metadata_exists = any('/metadata/' in m.name for m in members)
            if not metadata_exists:
                issues.append("Missing metadata directory")
                should_delete = True

            # Check for oneobjlit directory and count files
            oneobjlit_files = [m for m in members if '/oneobjlit/' in m.name and m.isfile()]
            oneobjlit_count = len(oneobjlit_files)

            if oneobjlit_count == 0:
                need_genvid = True
                issues.append("Missing oneobjlit directory")
            elif oneobjlit_count != EXPECTED_ONEOBJLIT_FILES and oneobjlit_count != EXPECTED_ONEOBJLIT_FILES_2:
                issues.append(f"oneobjlit has {oneobjlit_count} files (expected {EXPECTED_ONEOBJLIT_FILES})")
                if oneobjlit_count < EXPECTED_ONEOBJLIT_FILES or EXPECTED_ONEOBJLIT_FILES < oneobjlit_count < EXPECTED_ONEOBJLIT_FILES_2:
                    should_delete = True

            # Check for rgb directory and count PNG files
            rgb_png_files = [m for m in members if '/rgb/' in m.name and m.name.endswith('.png')]
            rgb_png_count = len(rgb_png_files)

            if rgb_png_count > 0:
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
            else:
                if not rgb_mp4_exists:
                    issues.append("0 rgb png while no rgb.mp4")
                    should_delete = True
                else:
                    # rgb.mp4 exists but we need to check its size
                    # Extract rgb.mp4 info from tar metadata
                    rgb_mp4_members = [m for m in members if m.name.endswith('rgb.mp4')]
                    if rgb_mp4_members:
                        mp4_size = rgb_mp4_members[0].size
                        if mp4_size < 20 * 1024:  # 20 KB
                            issues.append(f"0 rgb png while rgb.mp4 size invalid {mp4_size/1024:.2f}KB")
                            should_delete = True

            # Try to load and validate overview.json if it exists
            overview_members = [m for m in members if m.name.endswith('overview.json')]
            if overview_members:
                try:
                    file_obj = tar.extractfile(overview_members[0])
                    if file_obj:
                        content = file_obj.read()
                        overview_data = json.loads(content.decode('utf-8'))
                        # Basic validation - could add more checks here
                        if not isinstance(overview_data, dict):
                            issues.append("overview.json is not a valid JSON object")
                except Exception as e:
                    issues.append(f"Failed to parse overview.json: {str(e)}")

    except tarfile.TarError as e:
        issues.append(f"Corrupted tar archive: {str(e)}")
        should_delete = True
    except Exception as e:
        issues.append(f"Error reading archive: {str(e)}")
        should_delete = True

    return {
        "path": tar_path,
        "issues": issues,
        "valid": len(issues) == 0,
        "oneobjlit_count": oneobjlit_count,
        "rgb_png_count": rgb_png_count,
        "should_delete": should_delete,
        "need_genvid": need_genvid,
        "tar_size_mb": tar_size / (1024 * 1024)
    }


def worker_process(task_queue: Queue, result_queue: Queue):
    """
    Worker process that consumes tasks from queue and puts results back.
    """
    while True:
        try:
            # Get task from queue (timeout to allow checking for sentinel)
            task = task_queue.get(timeout=1)
            if task is None:  # Sentinel value to indicate shutdown
                break

            tar_path, tar_bytes = task
            result = check_tar_from_buffer(tar_path, tar_bytes)
            result_queue.put(result)

        except queue.Empty:
            continue
        except Exception as e:
            # Put error result
            result_queue.put({
                "path": "unknown",
                "issues": [f"Worker error: {str(e)}"],
                "valid": False,
                "error": True
            })


def producer_process(tar_paths: List[Path], task_queue: Queue, max_queue_size: int = 10):
    """
    Producer process that reads tar files sequentially and puts them in queue.
    This ensures sequential HDD access.
    """
    for tar_path in tar_paths:
        try:
            # Read file into memory
            with open(tar_path, 'rb') as f:
                tar_bytes = f.read()

            # Wait if queue is full (backpressure)
            while task_queue.qsize() >= max_queue_size:
                time.sleep(0.01)

            task_queue.put((str(tar_path), tar_bytes))

        except Exception as e:
            # Put error task
            while task_queue.qsize() >= max_queue_size:
                time.sleep(0.01)
            task_queue.put((str(tar_path), None))

    # Signal completion by putting None for each worker
    # This will be handled by main process


def get_all_tar_paths() -> List[Path]:
    """Find all .tar.gz archives in the dataset structure."""
    tar_paths = []

    if not DATASET_ROOT.exists():
        return tar_paths

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

                for render_item in scene_instance_dir.iterdir():
                    # Check if it's a .tar.gz file
                    if render_item.is_file() and render_item.suffix == '.gz' and render_item.stem.endswith('.tar'):
                        tar_paths.append(render_item)

    return tar_paths


def list_tar_contents(tar_path: Path) -> List[str]:
    """List contents of a tar.gz file without extracting."""
    try:
        with tarfile.open(tar_path, "r:gz") as tar:
            return tar.getnames()
    except Exception as e:
        return [f"Error: {str(e)}"]


def extract_single_file(tar_path: Path, file_path_in_tar: str, output_path: Path) -> bool:
    """
    Extract a single file from tar.gz to disk.
    Useful for inspecting specific files without full extraction.
    """
    try:
        with tarfile.open(tar_path, "r:gz") as tar:
            member = tar.getmember(file_path_in_tar)
            file_obj = tar.extractfile(member)
            if file_obj:
                output_path.parent.mkdir(parents=True, exist_ok=True)
                with open(output_path, 'wb') as f:
                    f.write(file_obj.read())
                return True
        return False
    except Exception as e:
        print(f"Error extracting {file_path_in_tar}: {e}")
        return False


def main():
    parser = argparse.ArgumentParser(
        description="Dataset consistency checker for packaged tar.gz archives (in-memory, producer-consumer)"
    )
    parser.add_argument("--path", type=str, default="./DatasetAutomationOutputDirectory",
                        help="Path to dataset root directory (default: ./DatasetAutomationOutputDirectory)")
    parser.add_argument("--delete", action="store_true",
                        help=f"Delete tar archives with: missing overview.json, oneobjlit==0, or 1<=oneobjlit<{EXPECTED_ONEOBJLIT_FILES}")
    parser.add_argument("--delete-all", action="store_true",
                        help="Delete ALL inconsistent tar archives (any with issues)")
    parser.add_argument("--list", type=str, metavar="TAR_PATH",
                        help="List contents of a specific tar.gz file")
    parser.add_argument("--extract-file", type=str, metavar="TAR_PATH",
                        help="Extract a specific file from tar (use with --file and --output)")
    parser.add_argument("--file", type=str,
                        help="File path inside tar to extract (use with --extract-file)")
    parser.add_argument("--output", type=str,
                        help="Output path for extracted file (use with --extract-file)")
    parser.add_argument("--workers", type=int, default=None,
                        help=f"Number of worker processes (default: {cpu_count()})")
    parser.add_argument("--save-report", type=str, metavar="JSON_PATH",
                        help="Save detailed check report to JSON file")
    parser.add_argument("--queue-size", type=int, default=10,
                        help="Max task queue size (controls memory usage, default: 10)")

    args = parser.parse_args()

    global DATASET_ROOT
    DATASET_ROOT = Path(args.path)

    # Security warning header
    print(f"{'='*80}")
    print(f"Packaged Dataset Consistency Checker (In-Memory, Producer-Consumer)")
    if args.delete or args.delete_all:
        print(f"⚠️  WARNING: DELETE MODE ACTIVE")
        if args.delete:
            print(f"   Will DELETE archives matching: no overview.json / oneobjlit==0 / 1<=oneobjlit<{EXPECTED_ONEOBJLIT_FILES}")
        if args.delete_all:
            print(f"   Will DELETE ALL inconsistent archives")
        print(f"   Press Ctrl+C within 3 seconds to cancel...")
        time.sleep(3)
    else:
        print(f"✓ READ-ONLY MODE - No files will be modified or deleted")
    print(f"{'='*80}\n")

    # Handle single tar file operations (bypass producer-consumer)
    if args.list:
        tar_path = Path(args.list)
        if not tar_path.exists():
            print(f"ERROR: Tar file not found: {tar_path}")
            return

        print(f"Contents of {tar_path}:")
        print(f"{'='*80}")
        contents = list_tar_contents(tar_path)
        for item in contents:
            print(f"  {item}")
        return

    if args.extract_file:
        if not args.file or not args.output:
            print("ERROR: --extract-file requires both --file and --output arguments")
            return

        tar_path = Path(args.extract_file)
        if not tar_path.exists():
            print(f"ERROR: Tar file not found: {tar_path}")
            return

        success = extract_single_file(tar_path, args.file, Path(args.output))
        if success:
            print(f"✓ Successfully extracted: {args.file} -> {args.output}")
        else:
            print(f"✗ Failed to extract: {args.file}")
        return

    # Dataset checking mode
    print(f"Dataset root: {DATASET_ROOT.resolve()}")

    if not DATASET_ROOT.exists():
        print(f"ERROR: Dataset root does not exist: {DATASET_ROOT.resolve()}")
        return

    print(f"Collecting all tar.gz archives...")

    tar_paths = get_all_tar_paths()
    total_archives = len(tar_paths)

    if total_archives == 0:
        print(f"No tar.gz archives found. Please check the dataset path.")
        return

    print(f"Total archives found: {total_archives}")

    # Calculate total size
    total_size = sum(p.stat().st_size for p in tar_paths)
    print(f"Total size: {total_size / (1024*1024*1024):.2f} GB")
    print(f"Avg size: {total_size / total_archives / (1024*1024):.2f} MB per archive")

    date_stats = defaultdict(lambda: defaultdict(int))
    for tar_path in tar_paths:
        parts = tar_path.relative_to(DATASET_ROOT).parts
        if len(parts) >= 4:
            date_dir = parts[0]
            scene_dir = parts[1]
            date_stats[date_dir][scene_dir] += 1

    print(f"\n{'='*80}")
    print(f"Dataset Statistics by Date:")
    print(f"{'='*80}")
    for date_dir in sorted(date_stats.keys()):
        print(f"\n{date_dir}:")
        for scene_dir, count in sorted(date_stats[date_dir].items()):
            print(f"  {scene_dir}: {count} archives")
        print(f"  Total: {sum(date_stats[date_dir].values())} archives")

    num_workers = args.workers if args.workers else cpu_count()
    queue_size = args.queue_size

    print(f"\n{'='*80}")
    print(f"Producer-Consumer Mode (HDD-Optimized)")
    print(f"{'='*80}")
    print(f"Workers: {num_workers}")
    print(f"Queue size: {queue_size}")
    print(f"Reading: Sequential (HDD-friendly)")
    print(f"Processing: Parallel (CPU-intensive)")
    print(f"\nStarting check...")
    print(f"{'='*80}\n")

    start_time = time.time()

    # Create queues
    # Note: maxsize controls backpressure - producer will block when queue is full
    task_queue = Queue(maxsize=queue_size)
    result_queue = Queue()

    # Start worker processes
    workers = []
    for _ in range(num_workers):
        p = Process(target=worker_process, args=(task_queue, result_queue))
        p.start()
        workers.append(p)

    # Producer: Main thread reads files sequentially (optimal for HDD)
    print(f"[Producer] Reading {total_archives} archives sequentially...")
    processed_count = 0

    try:
        for tar_path in tar_paths:
            try:
                # Read file into memory
                with open(tar_path, 'rb') as f:
                    tar_bytes = f.read()

                # Put in queue (blocks if queue is full - this provides backpressure)
                task_queue.put((str(tar_path), tar_bytes))
                processed_count += 1

                if processed_count % 100 == 0:
                    print(f"  Queued: {processed_count}/{total_archives} ({processed_count/total_archives*100:.1f}%)")

            except Exception as e:
                print(f"  Error reading {tar_path}: {e}")
                # Put error result directly
                result_queue.put({
                    "path": str(tar_path),
                    "issues": [f"Read error: {str(e)}"],
                    "valid": False,
                    "should_delete": True,
                    "need_genvid": False,
                    "tar_size_mb": 0
                })

    except KeyboardInterrupt:
        print("\n\nInterrupted by user. Shutting down...")
        # Send sentinel to workers
        for _ in workers:
            task_queue.put(None)
        # Wait for workers to finish
        for w in workers:
            w.join(timeout=5)
        return

    # Signal workers to finish
    print(f"[Producer] All files queued. Signaling workers to finish...")
    for _ in workers:
        task_queue.put(None)

    # Collect results
    print(f"[Consumer] Collecting results...")
    results = []
    while len(results) < total_archives:
        try:
            result = result_queue.get(timeout=1)
            results.append(result)
            if len(results) % 100 == 0:
                print(f"  Collected: {len(results)}/{total_archives}")
        except queue.Empty:
            # Check if all workers are done
            if all(not w.is_alive() for w in workers):
                # Try to drain remaining results
                while True:
                    try:
                        result = result_queue.get_nowait()
                        results.append(result)
                    except queue.Empty:
                        break
                break

    # Wait for all workers to complete
    for w in workers:
        w.join()

    elapsed_time = time.time() - start_time

    # Process results
    valid_count = sum(1 for r in results if r.get("valid", False))
    invalid_count = total_archives - valid_count

    print(f"\n{'='*80}")
    print(f"Check Results:")
    print(f"{'='*80}")
    print(f"Total archives: {total_archives}")
    print(f"Valid archives: {valid_count} ({valid_count/total_archives*100:.2f}%)")
    print(f"Invalid archives: {invalid_count} ({invalid_count/total_archives*100:.2f}%)")
    print(f"Time elapsed: {elapsed_time:.2f} seconds")
    print(f"Speed: {total_archives/elapsed_time:.2f} archives/second")
    print(f"Throughput: {total_size/elapsed_time/(1024*1024):.2f} MB/s")

    # Build scene statistics
    scene_render_status = defaultdict(lambda: defaultdict(list))
    archives_to_delete = set()
    archives_to_delete_all = set()
    archives_need_genvid = []

    for result in results:
        tar_path = Path(result["path"])
        parts = tar_path.relative_to(DATASET_ROOT).parts
        if len(parts) >= 4:
            scene_folder = tar_path.parent
            scene_folder_str = str(scene_folder)
            date_scene_key = f"{parts[0]}\\{parts[1]}"

            scene_render_status[date_scene_key][scene_folder_str].append(result.get("valid", False))

            if result.get("should_delete", False):
                archives_to_delete.add(str(tar_path))

            if not result.get("valid", False):
                archives_to_delete_all.add(str(tar_path))

            if result.get("need_genvid", False):
                archives_need_genvid.append(result["path"])

    scene_validity = {}
    for date_scene_key, scenes in scene_render_status.items():
        complete_scenes = 0
        total_scenes = len(scenes)

        for scene_folder, archive_validities in scenes.items():
            if all(archive_validities):
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
    if total_scenes > 0:
        print(f"Total complete scenes: {total_complete_scenes}/{total_scenes} ({total_complete_scenes/total_scenes*100:.2f}%)")
    else:
        print(f"Total complete scenes: {total_complete_scenes}/{total_scenes}")
    print(f"Total locations: {len(scene_validity)}")

    if len(archives_need_genvid) > 0:
        print(f"\n{'='*80}")
        print(f"Archives Ready for genvid ({EXPECTED_RGB_PNG_FILES} pngs in rgb/, missing rgb.mp4):")
        print(f"{'='*80}")
        print(f"Total: {len(archives_need_genvid)} archives")
        for archive_path in archives_need_genvid[:10]:  # Show first 10
            print(f"  {archive_path}")
        if len(archives_need_genvid) > 10:
            print(f"  ... and {len(archives_need_genvid) - 10} more")

    # Delete mode
    if (args.delete and len(archives_to_delete) > 0) or (args.delete_all and len(archives_to_delete_all) > 0):
        deletion_target = archives_to_delete_all if args.delete_all else archives_to_delete
        deletion_mode = "ALL INCONSISTENT" if args.delete_all else f"SPECIFIC (no overview.json / oneobjlit==0 / 1<=oneobjlit<{EXPECTED_ONEOBJLIT_FILES})"

        print(f"\n{'='*80}")
        print(f"⚠️  Archives to Delete ({deletion_mode}):")
        print(f"{'='*80}")
        print(f"Total archives to delete: {len(deletion_target)}")

        for archive_path in sorted(deletion_target)[:20]:  # Show first 20
            print(f"  {archive_path}")
        if len(deletion_target) > 20:
            print(f"  ... and {len(deletion_target) - 20} more")

        # Final confirmation
        print(f"\n⚠️  Confirm deletion of {len(deletion_target)} archives?")
        confirm = input("Type 'yes' to proceed: ")
        if confirm.lower() != 'yes':
            print("Deletion cancelled.")
            args.delete = False
            args.delete_all = False

        if args.delete or args.delete_all:
            print(f"\n{'='*80}")
            print(f"Deleting archives...")
            print(f"{'='*80}")

            deleted_count = 0
            failed_count = 0
            freed_space = 0

            for archive_path in deletion_target:
                try:
                    path = Path(archive_path)
                    size = path.stat().st_size
                    path.unlink()
                    freed_space += size
                    deleted_count += 1
                    if deleted_count % 10 == 0:
                        print(f"  Deleted: {deleted_count}/{len(deletion_target)}")
                except Exception as e:
                    print(f"✗ Failed to delete {archive_path}: {e}")
                    failed_count += 1

            print(f"\n{'='*80}")
            print(f"Deletion Summary:")
            print(f"{'='*80}")
            print(f"Successfully deleted: {deleted_count} archives")
            print(f"Freed space: {freed_space / (1024*1024*1024):.2f} GB")
            if failed_count > 0:
                print(f"Failed to delete: {failed_count} archives")

    # Report inconsistent archives
    if invalid_count > 0:
        print(f"\n{'='*80}")
        print(f"Inconsistent Archives:")
        print(f"{'='*80}")

        issue_types = defaultdict(list)

        for result in results:
            if not result.get("valid", False):
                print(f"\n{result['path']} ({result.get('tar_size_mb', 0):.2f} MB)")
                for issue in result.get("issues", []):
                    print(f"  - {issue}")
                    issue_types[issue].append(result['path'])

        print(f"\n{'='*80}")
        print(f"Issue Summary:")
        print(f"{'='*80}")
        for issue_type, paths in sorted(issue_types.items(), key=lambda x: len(x[1]), reverse=True):
            print(f"\n{issue_type}: {len(paths)} occurrences")

        if len(archives_to_delete) > 0 and not args.delete:
            print(f"\n{'='*80}")
            print(f"Archives matching --delete criteria: {len(archives_to_delete)}")
            print(f"Use --delete flag to remove these archives")

        if len(archives_to_delete_all) > 0 and not args.delete_all:
            print(f"\n{'='*80}")
            print(f"All inconsistent archives: {len(archives_to_delete_all)}")
            print(f"Use --delete-all flag to remove ALL inconsistent archives")

        # Save report
        report_path = DATASET_ROOT / "inconsistent_archives.json"
        if args.save_report:
            report_path = Path(args.save_report)

        print(f"\n{'='*80}")
        print(f"Saving report to '{report_path}'...")

        inconsistent_data = {
            "total_inconsistent": invalid_count,
            "issue_types": {k: len(v) for k, v in issue_types.items()},
            "details": [r for r in results if not r.get("valid", False)],
            "archives_matching_delete_criteria": sorted(list(archives_to_delete)),
            "archives_all_inconsistent": sorted(list(archives_to_delete_all)),
            "archives_need_genvid": archives_need_genvid
        }

        with open(report_path, "w", encoding="utf-8") as f:
            json.dump(inconsistent_data, f, indent=2, ensure_ascii=False)

        print(f"Saved to: {report_path}")
    else:
        print(f"\n✓ All archives are consistent!")

    print(f"\n{'='*80}")
    print(f"Check complete!")
    print(f"{'='*80}")


if __name__ == "__main__":
    main()
