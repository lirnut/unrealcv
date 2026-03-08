import os
import time
from pathlib import Path
from collections import defaultdict
import argparse
import json
import shutil

DATASET_ROOT = Path("./DatasetAutomationOutputDirectory")

DATEDIRT_TO_EXCLUDE = [
]

# 合法的render数量: 1 或 11
VALID_RENDER_COUNTS = {1, 11}


def get_all_scene_paths() -> list[Path]:
    """Get all scene instance directories (scene_XXXX_XXXXXXXX folders)"""
    scene_paths = []

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

                scene_paths.append(scene_instance_dir)

    return scene_paths


def count_renders_in_scene(scene_path: Path) -> tuple[str, int]:
    """Count render directories in a scene folder"""
    render_count = 0

    for item in scene_path.iterdir():
        if item.is_dir():
            render_count += 1

    return str(scene_path), render_count


def main():
    parser = argparse.ArgumentParser(description="统计每个场景中有多少个render，并检验正确性(合法数量: 1或11)")
    parser.add_argument("--path", type=str, default="./DatasetAutomationOutputDirectory",
                        help="Path to dataset root directory (default: ./DatasetAutomationOutputDirectory)")
    parser.add_argument("--delete", action="store_true",
                        help="删除render数量不合法的场景(不是1或11)")

    args = parser.parse_args()

    global DATASET_ROOT
    DATASET_ROOT = Path(args.path)

    print(f"{'='*80}")
    print(f"场景Render数量统计工具")
    print(f"{'='*80}\n")

    print(f"数据集根目录: {DATASET_ROOT.resolve()}")

    if not DATASET_ROOT.exists():
        print(f"错误: 数据集根目录不存在: {DATASET_ROOT.resolve()}")
        return

    print(f"正在收集所有场景目录...")

    scene_paths = get_all_scene_paths()
    total_scenes = len(scene_paths)

    if total_scenes == 0:
        print(f"未找到场景目录，请检查数据集路径。")
        return

    print(f"找到场景目录总数: {total_scenes}\n")

    # 统计每个场景的render数量
    scene_render_counts = []
    date_stats = defaultdict(lambda: defaultdict(list))  # date -> scene_type -> [render_counts]
    global_render_count_dist = defaultdict(int)  # render_count -> number of scenes
    invalid_scenes = []  # 存储不合法的场景 [(path, render_count), ...]

    for scene_path in scene_paths:
        scene_str, render_count = count_renders_in_scene(scene_path)
        is_valid = render_count in VALID_RENDER_COUNTS
        scene_render_counts.append((scene_str, render_count, is_valid))

        if not is_valid:
            invalid_scenes.append((scene_str, render_count))

        # 解析路径结构: date_dir/scene_dir/scene_instance_dir
        path_parts = Path(scene_str).relative_to(DATASET_ROOT).parts
        if len(path_parts) >= 3:
            date_dir = path_parts[0]
            scene_type = path_parts[1]
            scene_instance = path_parts[2]
        else:
            date_dir = "unknown"
            scene_type = "unknown"
            scene_instance = "unknown"

        date_stats[date_dir][scene_type].append({
            "scene": scene_instance,
            "render_count": render_count,
            "full_path": scene_str,
            "is_valid": is_valid
        })
        global_render_count_dist[render_count] += 1

    # 按render数量排序
    scene_render_counts.sort(key=lambda x: x[1], reverse=True)

    # 打印统计结果
    print(f"{'='*80}")
    print(f"按日期和场景类型统计:")
    print(f"{'='*80}")

    for date_dir in sorted(date_stats.keys()):
        print(f"\n【{date_dir}】")
        date_total_scenes = 0
        date_total_renders = 0

        for scene_type in sorted(date_stats[date_dir].keys()):
            scenes = date_stats[date_dir][scene_type]
            scene_count = len(scenes)
            render_count_sum = sum(s["render_count"] for s in scenes)
            date_total_scenes += scene_count
            date_total_renders += render_count_sum

            print(f"  {scene_type}:")
            print(f"    场景数量: {scene_count}")
            print(f"    Render总数: {render_count_sum}")
            print(f"    平均每场景Render: {render_count_sum/scene_count:.2f}")

            # 显示每个场景的render数量，标记不合法的
            for s in sorted(scenes, key=lambda x: x["scene"]):
                valid_mark = "✓" if s["is_valid"] else "✗ 不合法"
                print(f"      {s['scene']}: {s['render_count']} renders [{valid_mark}]")

        print(f"  {date_dir} 总计: {date_total_scenes} 场景, {date_total_renders} renders")

    # 全局render数量分布统计
    print(f"\n{'='*80}")
    print(f"Render数量分布统计:")
    print(f"{'='*80}")

    for render_count in sorted(global_render_count_dist.keys()):
        scene_count = global_render_count_dist[render_count]
        percentage = scene_count / total_scenes * 100
        valid_mark = "✓ 合法" if render_count in VALID_RENDER_COUNTS else "✗ 不合法"
        print(f"  {render_count} render(s) 的场景: {scene_count} 个 ({percentage:.2f}%) [{valid_mark}]")

    # 打印top 10最多render的场景
    print(f"\n{'='*80}")
    print(f"Render数量最多的10个场景:")
    print(f"{'='*80}")

    for scene_str, render_count, is_valid in scene_render_counts[:10]:
        rel_path = Path(scene_str).relative_to(DATASET_ROOT)
        valid_mark = "✓" if is_valid else "✗ 不合法"
        print(f"  {render_count} renders [{valid_mark}]: {rel_path}")

    # 打印只有1个render的场景（简要列表）
    single_render_scenes = [(s, c, v) for s, c, v in scene_render_counts if c == 1]
    if single_render_scenes:
        print(f"\n{'='*80}")
        print(f"只有1个render的场景 ({len(single_render_scenes)} 个) [✓ 合法]:")
        print(f"{'='*80}")

        # 按日期分组显示
        single_by_date = defaultdict(list)
        for scene_str, render_count, is_valid in single_render_scenes:
            path_parts = Path(scene_str).relative_to(DATASET_ROOT).parts
            if len(path_parts) >= 3:
                date_dir = path_parts[0]
                scene_type = path_parts[1]
                scene_instance = path_parts[2]
                single_by_date[f"{date_dir}/{scene_type}"].append(scene_instance)

        for key in sorted(single_by_date.keys()):
            print(f"  {key}:")
            for scene in sorted(single_by_date[key]):
                print(f"    {scene}")

    # 汇总信息
    total_renders = sum(c for _, c, _ in scene_render_counts)
    valid_scenes = [s for s, c, v in scene_render_counts if v]
    invalid_scenes_list = [(s, c) for s, c, v in scene_render_counts if not v]
    avg_renders_per_scene = total_renders / total_scenes if total_scenes > 0 else 0

    print(f"\n{'='*80}")
    print(f"汇总信息:")
    print(f"{'='*80}")
    print(f"总场景数: {total_scenes}")
    print(f"  - 合法场景 (1或11 renders): {len(valid_scenes)} ({len(valid_scenes)/total_scenes*100:.2f}%)")
    print(f"  - 不合法场景 (其他数量): {len(invalid_scenes_list)} ({len(invalid_scenes_list)/total_scenes*100:.2f}%)")
    print(f"总Render数: {total_renders}")
    print(f"平均每场景Render数: {avg_renders_per_scene:.2f}")
    print(f"最多Render的场景: {scene_render_counts[0][1] if scene_render_counts else 0}")
    print(f"最少Render的场景: {scene_render_counts[-1][1] if scene_render_counts else 0}")

    # 显示不合法场景列表
    if invalid_scenes_list:
        print(f"\n{'='*80}")
        print(f"不合法场景列表 (render数量不是1或11):")
        print(f"{'='*80}")

        # 按render数量分组
        invalid_by_count = defaultdict(list)
        for scene_str, render_count in invalid_scenes_list:
            invalid_by_count[render_count].append(scene_str)

        for render_count in sorted(invalid_by_count.keys()):
            scenes = invalid_by_count[render_count]
            print(f"\n  【{render_count} renders】共 {len(scenes)} 个场景:")
            for scene_str in sorted(scenes):
                rel_path = Path(scene_str).relative_to(DATASET_ROOT)
                print(f"    {rel_path}")

        # 删除不合法场景
        if args.delete:
            print(f"\n{'='*80}")
            print(f"删除不合法场景 (--delete):")
            print(f"{'='*80}")

            deleted_count = 0
            failed_count = 0

            for scene_str, render_count in invalid_scenes_list:
                try:
                    shutil.rmtree(scene_str)
                    print(f"✓ 已删除: {Path(scene_str).relative_to(DATASET_ROOT)} ({render_count} renders)")
                    deleted_count += 1
                except Exception as e:
                    print(f"✗ 删除失败 {scene_str}: {e}")
                    failed_count += 1

            print(f"\n删除完成: 成功 {deleted_count}, 失败 {failed_count}")
        else:
            print(f"\n提示: 使用 --delete 参数删除所有不合法场景")
    else:
        print(f"\n✓ 所有场景的render数量都合法 (1或11)!")

    print(f"\n{'='*80}")
    print(f"统计和验证完成!")
    print(f"{'='*80}")


if __name__ == "__main__":
    main()
