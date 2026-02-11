"""
Dataset Automation Python 指南
================================

本指南演示如何使用 Python 控制 UnrealCV 的数据集自动化系统生成标注视频数据集。

包含两种方式：
1. C++ create_scene 方式：使用 UE 内置场景生成
2. 手动方式：Python 完全控制（生成、位置、录制）
"""

import json
import time
import math
import random
from unrealcv import api


# =============================================================================
# 方式 1: C++ create_scene 方式
# =============================================================================
# create_scene 命令触发 UE 内置的场景生成 (USceneCompositionBPLib::GenerateRandomScene)
# 自动执行：
#   - 从配置类别中随机生成前景 actor
#   - 随机生成遮挡物
#   - 定位相机以观看前景
#   - 设置灯光

def example_create_scene_approach():
    """
    示例：使用 C++ create_scene 进行自动化场景生成

    工作流程：
    1. 配置自动化参数（total_scenes=1）
    2. 设置任务类型
    3. 定义命令序列
    4. 启动并等待完成
    5. 重新初始化下一个场景
    """
    client = api.UnrealCv_API(port=9000, ip='127.0.0.1', resolution=(1920, 1080))

    # ========== 步骤 1: 配置参数 ==========
    # total_scenes=1，每次只生成一个场景，完成后重新初始化
    client.client.request('vset /datasetautomation/config/total_scenes 1')
    client.client.request('vset /datasetautomation/config/output_directory G:/Dataset')
    client.client.request('vset /datasetautomation/config/trajectory_fps 30')
    client.client.request('vset /datasetautomation/config/num_frames 121')


    # 设置任务类型
    client.client.request('vset /datasetautomation/task_name Trajectory')

    # ========== 步骤 2: 定义命令序列 ==========
    # 参考 C++ 内置 Trajectory 序列
    command_sequence = {
        "task": "Trajectory",
        "commands": [
            {"cmd": "random_fov", "params": "40 55"},
            {"cmd": "random_resolution", "params": "1920x1080"},

            {"cmd": "create_scene"},

            # 动画配置
            {
                "cmd": "set_animation_bp",
                "params": "/Game/MetaHumans/ABP_RandomHeadMovement.ABP_RandomHeadMovement_C"
            },

            {"cmd": "sync_pawn_to_primary_camera"},
            {"cmd": "delay", "params": "5.0"},

            {"cmd": "prepare_record"},
            {"cmd": "delay", "params": "10.0"},

            {"cmd": "record_trajectory", "params": "render_only"},
            {"cmd": "special_wait", "params": "50"},
            {"cmd": "set_pause", "params": "true"},

            {"cmd": "record_trajectory", "params": "rotate_left_30"},
            {"cmd": "delay", "params": "5.0"},
            {"cmd": "record_trajectory", "params": "rotate_right_30"},
            {"cmd": "delay", "params": "5.0"},
            {"cmd": "record_trajectory", "params": "rotate_up_30"},
            {"cmd": "delay", "params": "5.0"},
            {"cmd": "record_trajectory", "params": "rotate_360"},
            {"cmd": "delay", "params": "5.0"},
            {"cmd": "record_trajectory", "params": "zoom_in"},

            {"cmd": "sync_secondary_cameras"},
            {"cmd": "set_time_dilation", "params": "1.0"},
            {"cmd": "delay", "params": "5.0"},
            {"cmd": "record_trajectory", "params": "zoom_out"},
            {"cmd": "delay", "params": "5.0"},
            {"cmd": "record_trajectory", "params": "random_1"},
            {"cmd": "delay", "params": "5.0"},
            {"cmd": "record_trajectory", "params": "random_2"},
            {"cmd": "delay", "params": "5.0"},
            {"cmd": "record_trajectory", "params": "random_3"},
            {"cmd": "delay", "params": "5.0"},
            {"cmd": "record_trajectory", "params": "random_4"},

            {"cmd": "sync_secondary_cameras"},
            {"cmd": "set_pause", "params": "false"},
            {"cmd": "set_time_dilation", "params": "1.0"},
            {"cmd": "sync_all_cameras"},
            {"cmd": "delay", "params": "1.0"},
            {"cmd": "clear_scene"},
            {"cmd": "delay", "params": "0.5"},
            {"cmd": "increment_counter"},
		    {"cmd": "load_random_level_every_n_scenes", "params": "1"},
            {"cmd": "check_completion"}
        ]
    }

    client.client.request(
        'vset /datasetautomation/sequence {}'.format(json.dumps(command_sequence))
    )

    # ========== 步骤 3: 循环生成多个场景 ==========
    for scene_index in range(10):  # 生成 10 个场景
        print(f"\n=== 开始场景 {scene_index + 1}/10 ===")

        # 重新初始化场景（清空并准备下一个）
        client.client.request('vset /datasetautomation/stop')  # 确保停止
        client.client.request('vset /datasetautomation/task_name Trajectory')  # 重新设置任务名

        # 重新上传命令序列
        client.client.request(
            'vset /datasetautomation/sequence {}'.format(json.dumps(command_sequence))
        )

        # 启动
        client.client.request('vset /datasetautomation/start')

        # 等待完成
        while True:
            status = client.client.request('vget /datasetautomation/status')
            print(f"状态: {status}")

            if "Completed" in status:
                print(f"场景 {scene_index + 1} 完成")
                break
            elif "Error" in status:
                print(f"场景 {scene_index + 1} 错误，停止")
                return

            time.sleep(2.0)

    print("\n所有场景生成完成！")


# =============================================================================
# 方式 2: 手动 Python 控制
# =============================================================================
# Python 直接控制：生成、定位、录制
# 使用 vset 命令配置数据集自动化系统

def example_manual_approach():
    """
    示例：手动 Python 控制所有环节

    工作流程：
    1. 手动生成对象
    2. 手动定位：前景在原点，相机在前景周围随机位置，面朝前景
    3. 配置数据集自动化并录制
    4. 完成后清理并重新初始化下一个场景
    """
    client = api.UnrealCv_API(port=9000, ip='127.0.0.1', resolution=(1920, 1080))

    # 录制配置
    output_dir = "G:/Dataset"
    trajectory_fps = 30
    num_frames = 121

    # ========== 循环生成多个场景 ==========
    for scene_index in range(10):
        print(f"\n=== 场景 {scene_index + 1}/10 ===")

        # ========== 步骤 1: 手动生成对象 ==========
        # 前景人物（骨骼网格体，用于动画）
        fg_actor = client.client.request('vset /objects/spawn BP_MetaHuman_C foreground_actor')
        print(f"生成前景人物: {fg_actor}")

        # ========== 步骤 2: 手动定位 ==========
        # 2.1 前景人物在原点
        client.client.request('vset /object/foreground_actor/location 0 0 0')
        client.client.request('vset /object/foreground_actor/rotation 0 0 0')

        fg_location = client.get_obj_location('foreground_actor')
        print(f"前景人物位置: {fg_location}")

        # # 2.2 遮挡物
        # occluder_count = 3
        # for i in range(occluder_count):
        #     angle = i * (360 / occluder_count)
        #     distance = random.uniform(200, 400)
        #     x = fg_location[0] + distance * math.cos(math.radians(angle))
        #     y = fg_location[1] + distance * math.sin(math.radians(angle))
        #     z = random.uniform(30, 100)

        #     client.client.request(f'vset /objects/spawn BP_Tree_C occluder_{i}')
        #     client.client.request(f'vset /object/occluder_{i}/location {x} {y} {z}')
        #     print(f"遮挡物 occluder_{i} -> ({x:.0f}, {y:.0f}, {z:.0f})")

        # 2.3 相机在前景周围随机位置，面朝前景
        camera_height = random.uniform(150, 200)
        camera_distance = random.uniform(300, 500)
        camera_angle = random.uniform(0, 360)

        cam_x = fg_location[0] + camera_distance * math.cos(math.radians(camera_angle))
        cam_y = fg_location[1] + camera_distance * math.sin(math.radians(camera_angle))
        cam_z = fg_location[2] + camera_height

        cam_id = client.spawn_free_camera()
        client.set_cam_location(cam_id, [cam_x, cam_y, cam_z])

        # 计算旋转：面朝前景
        dy = fg_location[1] - cam_y
        dx = fg_location[0] - cam_x
        dz = fg_location[2] - cam_z
        yaw = math.atan2(dy, dx) * 180 / math.pi
        pitch = math.atan2(dz, math.sqrt(dx**2 + dy**2)) * 180 / math.pi
        client.set_cam_rotation(cam_id, [pitch, yaw, 0])

        print(f"相机 {cam_id} -> 位置: ({cam_x:.0f}, {cam_y:.0f}, {cam_z:.0f}), 朝向: yaw={yaw:.1f}, pitch={pitch:.1f}")

        # ========== 步骤 3: 配置数据集自动化 ==========
        client.client.request('vset /datasetautomation/config/total_scenes 1')
        client.client.request(f'vset /datasetautomation/config/output_directory {output_dir}')
        client.client.request(f'vset /datasetautomation/config/trajectory_fps {trajectory_fps}')
        client.client.request(f'vset /datasetautomation/config/num_frames {num_frames}')

        client.client.request('vset /datasetautomation/currentscene/foreground_actor foreground_actor')
        client.client.request(f'vset /datasetautomation/currentscene/primary_camera {cam_id}')
        client.client.request('vset /datasetautomation/currentscene/scene_category outdoor')
        client.client.request('vset /datasetautomation/currentscene/foreground_subcategory human')
        client.client.request('vset /datasetautomation/currentscene/occluder_category vegetation')
        client.client.request('vset /datasetautomation/task_name Trajectory')

        # ========== 步骤 4: 命令序列 ==========
        anim_bp_path = "/Game/MetaHumans/ABP_RandomHeadMovement.ABP_RandomHeadMovement_C"

        command_sequence = {
            "task": "Trajectory",
            "commands": [
                # 动画
                {"cmd": "set_animation_bp", "params": anim_bp_path},

                {"cmd": "sync_pawn_to_primary_camera"},
                {"cmd": "delay", "params": "5.0"},

                {"cmd": "prepare_record"},
                {"cmd": "delay", "params": "10.0"},

                {"cmd": "record_trajectory", "params": "render_only"},
                {"cmd": "special_wait", "params": "50"},
                {"cmd": "set_pause", "params": "true"},

                {"cmd": "record_trajectory", "params": "rotate_left_30"},
                {"cmd": "delay", "params": "5.0"},
                {"cmd": "record_trajectory", "params": "rotate_right_30"},
                {"cmd": "delay", "params": "5.0"},
                {"cmd": "record_trajectory", "params": "rotate_up_30"},
                {"cmd": "delay", "params": "5.0"},
                {"cmd": "record_trajectory", "params": "rotate_360"},
                {"cmd": "delay", "params": "5.0"},
                {"cmd": "record_trajectory", "params": "zoom_in"},

                {"cmd": "sync_secondary_cameras"},
                {"cmd": "set_time_dilation", "params": "1.0"},
                {"cmd": "delay", "params": "5.0"},
                {"cmd": "record_trajectory", "params": "zoom_out"},
                {"cmd": "delay", "params": "5.0"},
                {"cmd": "record_trajectory", "params": "random_1"},
                {"cmd": "delay", "params": "5.0"},
                {"cmd": "record_trajectory", "params": "random_2"},
                {"cmd": "delay", "params": "5.0"},
                {"cmd": "record_trajectory", "params": "random_3"},
                {"cmd": "delay", "params": "5.0"},
                {"cmd": "record_trajectory", "params": "random_4"},

                {"cmd": "sync_secondary_cameras"},
                {"cmd": "set_pause", "params": "false"},
                {"cmd": "set_time_dilation", "params": "1.0"},
                {"cmd": "sync_all_cameras"},
                {"cmd": "delay", "params": "1.0"},
                {"cmd": "clear_scene"},
                {"cmd": "delay", "params": "0.5"},
                {"cmd": "increment_counter"},
                {"cmd": "check_completion"}
            ]
        }

        client.client.request(
            'vset /datasetautomation/sequence {}'.format(json.dumps(command_sequence))
        )

        # ========== 步骤 5: 启动并等待 ==========
        client.client.request('vset /datasetautomation/start')

        while True:
            status = client.client.request('vget /datasetautomation/status')
            print(f"状态: {status}")

            if "Completed" in status:
                break
            elif "Error" in status:
                print(f"场景 {scene_index + 1} 错误，停止")
                return

            time.sleep(2.0)

        # 短暂延迟后继续下一个场景
        time.sleep(1.0)

    print("\n所有场景生成完成！")


# =============================================================================
# 轨迹类型参考
# =============================================================================
"""
record_trajectory 命令可用的轨迹类型：

| 轨迹类型       | 描述                          |
|---------------|------------------------------|
| render_only   | 静态相机，按原样录制           |
| rotate_left_30| 相机左转 30 度               |
| rotate_right_30| 相机右转 30 度              |
| rotate_up_30  | 向上看 30 度                 |
| rotate_360    | 围绕目标 360 度旋转           |
| zoom_in      | 相机向目标靠近                |
| zoom_out     | 相机远离目标                  |
| random_1~4   | 随机轨迹模式 1-4             |
"""

# =============================================================================
# datasetautomation 主要配置命令
# =============================================================================
"""
配置命令：
  vset /datasetautomation/config/total_scenes [N]           - 总场景数
  vset /datasetautomation/config/output_directory [path]    - 输出目录
  vset /datasetautomation/config/trajectory_fps [fps]       - 录制 FPS
  vset /datasetautomation/config/num_frames [N]             - 每轨迹帧数

场景设置命令：
  vset /datasetautomation/currentscene/foreground_actor [name]  - 前景 actor 名称
  vset /datasetautomation/currentscene/primary_camera [id]      - 相机 ID
  vset /datasetautomation/currentscene/scene_category [cat]     - 场景类别

控制命令：
  vset /datasetautomation/sequence [json]  - 设置命令序列
  vset /datasetautomation/start              - 启动自动化
  vset /datasetautomation/stop              - 停止自动化
  vget /datasetautomation/status             - 获取当前状态

"""


#   ------------------------------------------------------------------------------
#   sequence序列支持的命令列表 (cmd)
#   ------------------------------------------------------------------------------
"""
🎥 摄像机控制
    random_fov             随机/固定FOV
    random_resolution      随机选择分辨率
    sync_pawn_to_primary_camera    同步Pawn到主摄像机
    sync_secondary_cameras 等待副摄像机录制完成
    sync_all_cameras       等待所有摄像机录制完成

🎬 动画设置
    set_animation_bp       设置动画蓝图
    set_animation_seq      设置动画序列
    record_trajectory      开始轨迹录制

🌍 场景管理
    create_scene           创建随机场景
    clear_scene            清除当前场景
    load_level             加载指定关卡
    load_random_level_every_n_scenes    每N场景随机换图
    annotate_world         标注世界物体

⏯️ 录制控制
    prepare_record         准备录制
    prepare_groom          准备毛发渲染(Matting)
    record_nav_track       录制导航轨迹
    save_videos            保存所有视频
    special_wait           等待录制到指定帧

🔄 流程控制
    increment_counter      场景计数器+1
    check_completion       检查场景完成度
    delay                  等待指定秒数
    set_pause              暂停/恢复游戏
    set_time_dilation      设置时间膨胀
    vrun                   执行UE控制台命令

"""


# =============================================================================
# create_scene C++ 自动生成场景：SceneComposition.json 配置文件说明
# =============================================================================
"""
create_scene 命令使用 G:/HUAWEI_Project_UE56/Saved/SceneComposition.json 配置文件定义场景生成规则。

一、Mesh 生成方式
-----------------

1. 类别随机生成（推荐）
   使用 ForegroundCategory / OccluderCategory 从 AssetPool 中随机选择资产：

   "ForegroundCategory": "Foreground_Human",      // 前景类别
   "OccluderCategory": "Occluder_All",            // 遮挡物类别

   优点：
   - 每个场景生成的资产类型不可预测
   - 适合多样性数据集

2. 直接指定路径
   使用 ForegroundPathSpec / OccluderPathSpec 指定具体资产路径：

   "ForegroundPathSpec": "/Game/MetaHumans/human_2_dress3_f-thin/BP_human_2_dress3_f-thin.BP_human_2_dress3_f-thin",
   "OccluderPathSpec": "/Game/Props/Trees/BP_OakTree.BP_OakTree",

   优先级：PathSpec 非空时优先使用，忽略 Category

二、生成位置方式
-----------------

1. 安全点位（SafePoints）- 80% 概率使用
   预定义的演员安全位置列表，优先在这些位置生成前景：

   "SafePoints": [
       {
           "Position": {
               "X": 1333.99,
               "Y": 25.43,
               "Z": 105.91
           }
       },
       {
           "Position": {
               "X": -487.72,
               "Y": -8.60,
               "Z": 95.17
           }
       }
   ]

   作用：
   - 确保前景物体不会生成在不安全位置（如水下、地下）
   - 确保有足够空间做遮挡关系

2. 随机区域（SpawnArea）- 20% 概率使用
   当安全点位不存在或随机选择未命中时使用：

   "XMin": -3657,    // 区域 X 最小值
   "XMax": 3790,     // 区域 X 最大值
   "YMin": -2299,    // 区域 Y 最小值
   "YMax": 1400,     // 区域 Y 最大值

   在矩形区域内随机生成位置。

三、配置示例
-------------

{
    "SimpleTester": {
        "Enabled": true,
        "XMax": 1000,
        "YMax": 1000,
        "XMin": -1000,
        "YMin": -1000,
        "GroundHeight": 50,
        "ForegroundPathSpec": "",                    // 空 = 使用类别随机
        "ForegroundCategory": "Foreground_Human",    // 前景从该类别随机
        "OccluderPathSpec": "",                     // 空 = 使用类别随机
        "OccluderCategory": "Occluder_All",         // 遮挡物从该类别随机
        "OccluderCount": 3,                          // 遮挡物数量
        "bAutoPositionCamera": true,
        "ForegroundYaw": -1,                         // -1 = 随机朝向
        "SafePoints": [                              // 安全点位（80%概率使用）
            {"Position": {"X": 699.50, "Y": -696.98, "Z": 123.09}},
            {"Position": {"X": -426.35, "Y": -86.95, "Z": 94.79}}
        ]
    }
}


"""

