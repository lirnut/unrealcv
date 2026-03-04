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

DATASET_ROOT = Path("./DatasetAutomationOutputDirectory")
EXPECTED_ONEOBJLIT_FILES = 90
EXPECTED_RGB_PNG_FILES = 90

DATEDIRT_TO_EXCLUDE = [
    "26-03-05"
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
    if not overview_json.exists():
        issues.append("Missing overview.json")
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
        if not rgb_mp4.exists():
            issues.append("no rgb folder while no rgb.mp4")
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
            started_count = 0

            for render_path in renders_need_genvid:
                try:
                    cmd = [
                        "python",
                        str(genvid_script),
                        "--input-dir", render_path,
                        "--fps", str(args.fps),
                        "--time_delay", str(abs(min(5 * 60, started_count * 8)))
                    ]

                    process = subprocess.Popen(
                        cmd,
                        stdout=subprocess.DEVNULL,
                        stderr=subprocess.DEVNULL,
                        shell=False,
                        start_new_session=True
                    )

                    print(f"✓ Started genvid (PID {process.pid}): {render_path}")
                    started_count += 1
                    time.sleep(2)
                except Exception as e:
                    print(f"✗ Failed to start genvid for {render_path}: {e}")

            print(f"\n{'='*80}")
            print(f"Genvid Summary:")
            print(f"{'='*80}")
            print(f"Successfully started: {started_count} processes")
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
