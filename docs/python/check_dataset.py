import os
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

DATASET_ROOT = Path("./DatasetAutomationOutputDirectory")
EXPECTED_ONEOBJLIT_FILES = 90
EXPECTED_RGB_PNG_FILES = 90

DATEDIRT_TO_EXCLUDE = [
]


def check_render_directory(render_path: Path) -> Dict[str, any]:
    issues = []
    oneobjlit_count = 0
    rgb_png_count = 0
    should_delete = False
    need_genvid = False

    rgb_mp4 = render_path / "rgb.mp4"
    if not rgb_mp4.exists():
        issues.append("Missing rgb.mp4")

    overview_json = render_path / "overview.json"
    overview_json_gz = render_path / "overview.json.gz"

    if not overview_json.exists() and not overview_json_gz.exists():
        issues.append("Missing overview.json (or overview.json.gz)")
        should_delete = True


    metadata_dir = render_path / "metadata"
    if not metadata_dir.exists():
        issues.append("Missing metadata directory")
        should_delete = True

    oneobjlit_dir = render_path / "oneobjlit"
    if not oneobjlit_dir.exists():
        issues.append("Missing oneobjlit directory")
    else:
        file_count = len(list(oneobjlit_dir.iterdir()))
        oneobjlit_count = file_count
        if file_count != EXPECTED_ONEOBJLIT_FILES:
            issues.append(f"oneobjlit has {file_count} files (expected {EXPECTED_ONEOBJLIT_FILES})")
            if  file_count < EXPECTED_ONEOBJLIT_FILES:
                should_delete = True

    rgb_dir = render_path / "rgb"
    if rgb_dir.exists() and rgb_dir.is_dir():
        png_files = list(rgb_dir.glob("*.png"))
        rgb_png_count = len(png_files)
        if rgb_png_count == EXPECTED_RGB_PNG_FILES:
            need_genvid = True
        elif 1 <= rgb_png_count < EXPECTED_RGB_PNG_FILES:
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


def main():
    parser = argparse.ArgumentParser(description="Dataset consistency checker")
    parser.add_argument("--path", type=str, default="./DatasetAutomationOutputDirectory",
                        help="Path to dataset root directory (default: ./DatasetAutomationOutputDirectory)")
    parser.add_argument("--delete", action="store_true",
                        help="Delete scene folders with: missing overview.json, oneobjlit==0, or 1<=oneobjlit<90")
    parser.add_argument("--delete-all", action="store_true",
                        help="Delete ALL inconsistent scene folders (any scene with issues)")
    parser.add_argument("--genvid", action="store_true",
                        help="Run genvid.py for renders with exactly 90 png files in rgb folder but missing rgb.mp4")
    parser.add_argument("--fps", type=int, default=30,
                        help="FPS for genvid.py (default: 30)")
    parser.add_argument("--delete-mode-dir", type=str, metavar="DIRNAME",
                        help="Delete specific subdirectory (e.g., 'rgb') from all render directories recursively")
    parser.add_argument("--compress", action="store_true",
                        help="Compress overview.json files to overview.json.gz (gzip)")
    parser.add_argument("--decompress", action="store_true",
                        help="Decompress overview.json.gz files back to overview.json")
    parser.add_argument("--compress-remove-original", action="store_true",
                        help="Remove original overview.json after compression")
    args = parser.parse_args()

    global DATASET_ROOT
    DATASET_ROOT = Path(args.path)

    print(f"{'='*80}")
    print(f"Dataset Consistency Checker")
    if args.delete:
        print(f"MODE: DELETE SPECIFIC SCENES (no overview.json / oneobjlit==0 / 1<=oneobjlit<90)")
    if args.delete_all:
        print(f"MODE: DELETE ALL INCONSISTENT SCENES")
    if args.genvid:
        print(f"MODE: AUTO GENVID (FPS={args.fps})")
    if args.delete_mode_dir:
        print(f"MODE: DELETE MODE DIR '{args.delete_mode_dir}' from all render directories")
    if args.compress:
        print(f"MODE: COMPRESS overview.json -> overview.json.gz")
    if args.decompress:
        print(f"MODE: DECOMPRESS overview.json.gz -> overview.json")
    print(f"{'='*80}\n")

    print(f"Dataset root: {DATASET_ROOT.resolve()}")

    if not DATASET_ROOT.exists():
        print(f"ERROR: Dataset root does not exist: {DATASET_ROOT.resolve()}")
        return

    print(f"Collecting all render directories...")

    render_paths = get_all_render_paths()
    total_renders = len(render_paths)

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

                    if args.compress_remove_original:
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

    print(f"\n{'='*80}")
    print(f"Check Results:")
    print(f"{'='*80}")
    print(f"Total renders: {total_renders}")
    print(f"Valid renders: {valid_count} ({valid_count/total_renders*100:.2f}%)")
    print(f"Invalid renders: {invalid_count} ({invalid_count/total_renders*100:.2f}%)")

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
        print(f"Renders Ready for genvid (90 pngs in rgb/, missing rgb.mp4):")
        print(f"{'='*80}")
        print(f"Total: {len(renders_need_genvid)} renders")

        if args.genvid:
            print(f"\n{'='*80}")
            print(f"Starting genvid processes...")
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
        deletion_mode = "ALL INCONSISTENT" if args.delete_all else "SPECIFIC (no overview.json / oneobjlit==0 / 1<=oneobjlit<90)"

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
