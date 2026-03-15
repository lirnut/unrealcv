import os
import time
from pathlib import Path
from typing import List
import argparse

DATASET_ROOT = Path("./DatasetAutomationOutputDirectory")

DATEDIRT_TO_EXCLUDE = [
]


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


def main():
    parser = argparse.ArgumentParser(
        description="Delete tar.gz files if a directory with the same name (without .tar.gz) exists"
    )
    parser.add_argument("--path", type=str, default="./DatasetAutomationOutputDirectory",
                        help="Path to dataset root directory (default: ./DatasetAutomationOutputDirectory)")
    parser.add_argument("--dry-run", action="store_true",
                        help="Show what would be deleted without actually deleting")
    parser.add_argument("--workers", type=int, default=1,
                        help="Number of worker processes (default: 1)")

    args = parser.parse_args()

    global DATASET_ROOT
    DATASET_ROOT = Path(args.path)

    print(f"{'='*80}")
    print(f"Delete tar.gz if corresponding directory exists")
    print(f"{'='*80}\n")

    if not DATASET_ROOT.exists():
        print(f"ERROR: Dataset root does not exist: {DATASET_ROOT.resolve()}")
        return

    print(f"Dataset root: {DATASET_ROOT.resolve()}")
    print(f"Collecting all tar.gz archives...")

    tar_paths = get_all_tar_paths()
    total_archives = len(tar_paths)

    if total_archives == 0:
        print(f"No tar.gz archives found.")
        return

    print(f"Total archives found: {total_archives}\n")

    # Find tar files to delete
    tars_to_delete = []
    total_size_to_delete = 0

    for tar_path in tar_paths:
        # Get the directory path (same name as tar without .tar.gz)
        # e.g., xxx.tar.gz -> xxx
        tar_name = tar_path.name  # xxx.tar.gz
        base_name = tar_path.stem  # xxx.tar
        dir_name = Path(tar_path).with_suffix('').stem  # xxx
        corresponding_dir = tar_path.parent / dir_name

        if corresponding_dir.exists() and corresponding_dir.is_dir():
            tar_size = tar_path.stat().st_size
            tars_to_delete.append((tar_path, corresponding_dir, tar_size))
            total_size_to_delete += tar_size

    if len(tars_to_delete) == 0:
        print(f"No tar.gz files found with corresponding directories.")
        return

    print(f"{'='*80}")
    print(f"Found {len(tars_to_delete)} tar.gz files to delete:")
    print(f"{'='*80}")

    for tar_path, dir_path, tar_size in tars_to_delete:
        size_str = f"{tar_size / (1024*1024):.2f} MB"
        print(f"  {tar_path.name} ({size_str}) <- dir exists: {dir_path.name}/")

    print(f"\n{'='*80}")
    print(f"Total size to delete: {total_size_to_delete / (1024*1024):.2f} MB")
    print(f"{'='*80}\n")

    if args.dry_run:
        print(f"[DRY RUN] Would delete {len(tars_to_delete)} tar.gz files")
        return

    # Confirmation
    print(f"Confirm deletion of {len(tars_to_delete)} tar.gz files?")
    confirm = input("Type 'yes' to proceed: ")
    if confirm.lower() != 'yes':
        print("Deletion cancelled.")
        return

    print(f"\n{'='*80}")
    print(f"Deleting tar.gz files...")
    print(f"{'='*80}")

    deleted_count = 0
    failed_count = 0
    freed_space = 0

    for tar_path, dir_path, tar_size in tars_to_delete:
        try:
            tar_path.unlink()
            freed_space += tar_size
            deleted_count += 1
            if deleted_count % 10 == 0:
                print(f"  Deleted: {deleted_count}/{len(tars_to_delete)}")
        except Exception as e:
            print(f"  Failed to delete {tar_path.name}: {e}")
            failed_count += 1

    print(f"\n{'='*80}")
    print(f"Deletion Summary:")
    print(f"{'='*80}")
    print(f"Successfully deleted: {deleted_count} tar.gz files")
    print(f"Freed space: {freed_space / (1024*1024*1024):.2f} GB")
    if failed_count > 0:
        print(f"Failed to delete: {failed_count} files")

    print(f"\n{'='*80}")
    print(f"Done!")
    print(f"{'='*80}")


if __name__ == "__main__":
    main()
