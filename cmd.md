DebugGame 控制台中支持的完整命令列表

  UnrealCV 支持以下主要命令类别（通过 TCP 服务器或在 DebugGame 控制台中访问）：

  

  游戏控制命令 (/action/*)

  - vset /action/game/pause - 暂停游戏
  - vset /action/game/resume - 恢复游戏
  - vget /action/game/is_paused - 查询是否暂停
  - vset /action/game/level [str] - 切换地图 ⭐
  - vset /action/input/enable - 启用输入
  - vset /action/input/disable - 禁用输入
  - vset /action/eyes_distance [float] - 设置立体摄像机距离
  - vset /action/keyboard [str] [float] - 发送键盘输入

  摄像机命令 (/camera/*)

  - vget /cameras     - 列出所有摄像机（新格式稳定ID，推荐长期使用）
  - vget /cameras_CID - 列出所有摄像机（新格式稳定ID，推荐长期使用）
     ```
     >>> vget /cameras <<<
     CID-UnrealcvPawn_0-fd CID-BP_BaseBike_C_1-49 CID-BP_Drone01_C_1-1d
     ```
  - vget /cameras_legacy - 列出所有摄像机, 旧格式 （类名...）
     ```
     >>> vget /cameras <<<
     PawnSensor FusionCamSensor FusionCamSensor FusionCamSensor FusionCamSensor FusionCamSensor
     ```
     说明: 相机 [id] 既支持旧格式（整数索引: 0, 1, 2...）也支持新格式稳定CID（CID-[所有者object_name]-[两位uuid]）
          - 旧ID: 0, 1, 2... （基于创建顺序，Sensor销毁会导致ID变化）
          - 新ID: CID-[所有者object_name]-[两位uuid] （和Sensor绑定，不会出现Sensor销毁导致ID错位的问题）

  - vset /cameras/spawn - 生成新摄像机
  - vget /camera/[id]/location - 获取摄像机位置
  - vset /camera/[id]/location [x] [y] [z] - 设置摄像机位置
  - vget /camera/[id]/rotation - 获取摄像机旋转
  - vset /camera/[id]/rotation [pitch] [yaw] [roll] - 设置摄像机旋转
  - vset /camera/[id]/moveto [x] [y] [z] - 移动摄像机（物理碰撞，有阻挡检测）
  - vget /camera/[id]/lit [filename] - 获取 RGB 图像
  - vget /camera/[id]/depth [filename] - 获取深度数据
  - vget /camera/[id]/normal [filename] - 获取法线图
  - vget /camera/[id]/optical_flow [filename] - 获取光流
  - vget /camera/[id]/object_mask [filename] - 获取物体分割掩码
  - vget /camera/[id]/oneobjmask [filename] [object_name] - 单个物体分割掩码
  - vget /camera/[id]/seg [filename] - 获取分割（同上）
  - vget /camera/[id]/fov - 获取 FOV
  - vset /camera/[id]/fov [float] - 设置 FOV
  - vget /camera/[id]/use_fast_capture - 获取快速捕获模式状态 (0 or 1)
  - vset /camera/[id]/use_fast_capture [uint] - 设置快速捕获模式 (0=disabled, 1=enabled)
  - vget /camera/[id]/size - 获取分辨率
  - vset /camera/[id]/size [width] [height] - 设置分辨率
  - vset /camera/[id]/projection_type [str] - 设置投影类型 (perspective/orthographic)
  - vset /camera/[id]/ortho_width [float] - 设置正交宽度
  - vset /camera/[id]/lit_source [str] - 设置光照源 (ftc_hdr/fc_hdr/sc_hdr/scna_hdr/ldr/base/color_depth/scene_depth/device_depth/normal)
  - vset /camera/[id]/reflection [str] - 设置反射方法（None/Lumen/ScreenSpace）
  - vset /camera/[id]/illumination [str] - 设置全局光照方法（None/Lumen/ScreenSpace/Plugin）
  - vset /camera/[id]/exposure_method [str] - 设置曝光方法（Histogram/Basic/Manual）
  - vset /camera/[id]/exposure_bias [float] - 设置曝光偏差
  - vset /camera/[id]/auto_speed [float] [float] - 设置自动曝光速度（下降，上升）
  - vset /camera/[id]/auto_brightness [float] [float] - 设置自动曝光亮度范围（最小，最大）
  - vset /camera/[id]/physical_exposure [uint] - 设置物理相机曝光开关
  - vset /camera/[id]/motion_blur [amount] [max] [per_object] [fps] - 设置运动模糊
  - vset /camera/[id]/focal [distance] [range] - 设置焦点参数
  - vget /screenshot [filename] - 获取截图

  物体命令 (/object/*)

  - vget /objects - 获取所有物体列表
  - vget /objects [search_spec] - 搜索物体，不分大小写，返回列表
     ```
     >>> vget /objects bp_character <<<
     BP_Character_C_1
     ```
  - vget /objects/scan_assets - 扫描 /Game/ 路径下所有可生成资源 ⭐
  - vget /objects/scan_assets [str] - 扫描指定路径下所有可生成资源（StaticMesh/SkeletalMesh/Blueprint）⭐
     ```
     >>> vget /objects/scan_assets /Game/MetaHumans/ <<<
     Found 125 spawnable assets in '/Game/MetaHumans/':
     BP_human_2_dress0_f-fat | Blueprint | /Game/MetaHumans/human_2_dress0_f-fat/BP_human_2_dress0_f-fat.BP_human_2_dress0_f-fat
     Chair_01 | StaticMesh | /Game/Props/Furniture/Chair_01.Chair_01
     ...
     ```
  - vset /objects/spawn_cube - 生成测试立方体
  - vset /objects/spawn_cube [name] - 生成命名测试立方体
  - vset /objects/spawn [classname] - 生成物体（使用UClass名称）
  - vset /objects/spawn [classname] [name] - 生成命名物体（使用UClass名称）
  - vset /objects/spawn_from_path [str] - 从资产路径生成物体（Cook友好）⭐
  - vset /objects/spawn_from_path [str] [str] - 从资产路径生成命名物体 ⭐
     ```
     >>> vset /objects/spawn_from_path /Game/MetaHumans/human_2_dress0_f-fat/BP_human_2_dress0_f-fat.BP_human_2_dress0_f-fat <<<
     BP_human_2_dress0_f-fat_C_2147467374

     >>> vset /objects/spawn_from_path /Game/Props/Chair.Chair MyChair <<<
     MyChair
     ```
  - vget /object/[name]/location - 获取物体位置
  - vset /object/[name]/location [x] [y] [z] - 设置物体位置
  - vget /object/[name]/rotation - 获取物体旋转
  - vset /object/[name]/rotation [pitch] [yaw] [roll] - 设置物体旋转
  - vget /object/[name]/scale - 获取物体缩放
  - vset /object/[name]/scale [x] [y] [z] - 设置物体缩放
  - vget /object/[name]/color - 获取物体标注颜色
  - vset /object/[name]/color [r] [g] [b] - 设置物体标注颜色
  - vget /object/[name]/vertex_location - 获取物体顶点位置
  - vget /object/[name]/mobility - 获取物体移动性 (Static/Movable/Stationary)
  - vget /object/[name]/uclass_name - 获取UClass名称
  - vset /object/[name]/name [newname] - 重命名物体
  - vget /object/[name]/label - [编辑器]获取Actor标签
  - vset /object/[name]/label [label] - [编辑器]设置Actor标签
  - vset /object/[name]/show - 显示物体
  - vset /object/[name]/hide - 隐藏物体
  - vset /object/[name]/destroy - 销毁物体
  - vget /object/[name]/bounds - 获取物体边界

  记录命令 (/captureactor/*)
  - vset /captureactor/spawn_free_cam - 生成自由摄像机
  - vset /captureactor/time_dilation [float] - 设置录制时间膨胀 (0.1-10.0)
  - vget /captureactor/asset_pool - 查询资产池中的资产
  - vset /captureactor/[id]/record [output_folder] [fps] [duration_seconds] [{lit|rgb},{object_mask|seg},normal,depth,optical_flow] - 开始录像（不干涉相机移动）
      ```
      >>> vset /capturreactor/CID-BP_Hatchback_child_base_C_4-00/record G:\Project_UE56\tmp 24 100 lit <<<
      Recording started: Camera CID-BP_Hatchback_child_base_C_4-00, File: G:\Project_UE56\tmp, FPS: 24, Frames: 2400, Types: (lit)
      ```
  - vget /captureactor/[id]/is_recording - 查询是否正在录制
  - vset /captureactor/[id]/stop_record - 停止录制指定摄像机

  插件命令 (/unrealcv/*)

  - vget /unrealcv/status - 获取插件状态
  - vget /unrealcv/help - 获取所有命令帮助
  - vget /unrealcv/version - 获取插件版本
  - vget /scene/name - 获取场景名称

  导航命令 (/agent/*)

  - vset /agent/[name]/nav/start [speed] - 开始导航
  - vset /agent/[name]/nav/goto [x] [y] [z] - 导航到位置
  - vset /agent/[name]/nav/stop - 停止导航
  - vget /agent/[name]/nav/status - 获取导航状态

  视图模式

  - vset /viewmode [mode] - 设置视图模式（lit/normal/depth/object_mask）
  - vget /viewmode - 获取当前视图模式

  Pawn命令 (/pawn/*)

  - vget /pawn/location - 获取玩家Pawn位置
  - vset /pawn/location [x] [y] [z] - 设置玩家Pawn位置
  - vget /pawn/rotation - 获取玩家Pawn旋转
  - vset /pawn/rotation [pitch] [yaw] [roll] - 设置玩家Pawn旋转

  光照命令 (/light/*)

  - vget /light/directional/intensity - 获取方向光强度
  - vset /light/directional/intensity [float] - 设置方向光强度
  - vget /light/skylight/intensity - 获取天光强度
  - vset /light/skylight/intensity [float] - 设置天光强度
  - vget /light/directional/castdeepshadow - 获取方向光深阴影状态
  - vset /light/directional/castdeepshadow [bool] - 设置方向光深阴影

  PAK文件命令 (/pak/*)

  - vset /pak/mount [path] [order] - 挂载PAK文件
  - vset /pak/unmount [path] - 卸载PAK文件
  - vget /pak/mounted - 获取所有已挂载的PAK文件列表
  - vget /pak/ismounted [path] - 检查PAK文件是否已挂载
  - vset /pak/scan [mountpoint] [force_rescan] - 扫描已挂载PAK中的资源
  - vget /pak/load [assetpath] - 从PAK加载资源
  - vget /pak/assets [packagepath] - 获取路径下所有资源
  - vset /pak/register [path] [category] - 注册PAK资源到AssetPool

  数据集自动化命令 (/datasetautomation/*)

  Task:
  - vget /datasetautomation/task_name - 获取当前任务名称
  - vset /datasetautomation/task_name [name] - 设置任务名称 (Trajectory/Omnimatte)

  CurrentScene:
  - vset /datasetautomation/currentscene/foreground_actor [name] - 设置前景物体名称
  - vset /datasetautomation/currentscene/primary_camera [id] - 设置主摄像机ID
  - vset /datasetautomation/currentscene/scene_category [category] - 设置场景类别
  - vset /datasetautomation/currentscene/foreground_subcategory [subcategory] - 设置前景子类别
  - vset /datasetautomation/currentscene/occluder_category [category] - 设置遮挡物类别

  Config:
  - vset /datasetautomation/config/total_scenes [N] - 设置总场景数
  - vset /datasetautomation/config/output_directory [path] - 设置输出目录
  - vset /datasetautomation/config/trajectory_fps [fps] - 设置录制FPS
  - vset /datasetautomation/config/trajectory_degrees_per_second [deg] - 设置角速度
  - vset /datasetautomation/config/num_frames [N] - 设置轨迹录制帧数
  - vset /datasetautomation/config/b_load_scene_params_from_json [true/false] - 是否从JSON加载参数
  - vset /datasetautomation/config/foreground_move_speed [float] - 设置前景移动速度(cm/s)
  - vset /datasetautomation/config/foreground_move_angle_offset [float] - 设置前景移动角度偏移(度, 0=前进, 90=右, -90=左, 180=后退)

  Control:
  - vset /datasetautomation/start - 启动自动化（使用当前Config）
  - vset /datasetautomation/stop - 停止自动化
  - vget /datasetautomation/status - 获取自动化状态
  - vget /datasetautomation/history - 获取命令执行历史记录

  动态命令队列自动化命令 (/datasetautomation/*)

  - vset /datasetautomation/sequence [str] - 设置命令序列 (JSON格式)
  - vget /datasetautomation/sequence - 获取当前命令序列
  - vset /datasetautomation/start - 启动自动化（使用 Config 中的参数）
  - vset /datasetautomation/stop - 停止自动化
  - vget /datasetautomation/status - 获取自动化状态

  ================================================================================
  数据集自动化命令序列 (JSON格式)
  ================================================================================

  ------------------------------------------------------------------------------
  支持的命令列表 (cmd)
  ------------------------------------------------------------------------------

  | 命令 | params | 说明 |
  |------|--------|------|
  | random_fov | [min] [max] 或 [value] | 随机/固定 FOV |
  | random_resolution | [WxH] [WxH] ... | 随机选择分辨率 |
  | set_animation_bp | [ABP_Path] | 设置 Animation Blueprint |
  | set_animation_seq | [Seq_Path] | 设置 Animation Sequence (循环播放) |
  | create_scene | - | 创建随机场景 |
  | clear_scene | - | 清除当前场景 |
  | sync_pawn_to_primary_camera | - | 同步 Pawn 到主摄像机位置 |
  | prepare_record | - | 准备录制 |
  | prepare_groom | - | 准备毛发渲染 (Matting 任务) |
  | increment_counter | - | 场景计数器 +1 |
  | check_completion | - | 检查是否完成所有场景 |
  | sync_secondary_cameras | - | 等待副摄像机录制完成 |
  | sync_all_cameras | - | 等待所有摄像机录制完成 |
  | save_videos | - | 保存所有视频 |
  | annotate_world | - | 标注世界物体 |
  | delay | 秒数 | 等待指定秒数 |
  | set_pause | true/false | 暂停/恢复游戏 |
  | set_time_dilation | 数值 | 设置时间膨胀 (0.1-10.0) |
  | load_level | 关卡名 | 加载指定关卡 |
  | load_random_level_every_n_scenes | N | 每 N 个场景随机换图 |
  | special_wait | 轨迹索引 | 等待录制达到指定轨迹索引 |
  | set_animation_bp | AnimBP路径 | 设置前景物体动画蓝图 |
  | record_nav_track | - | 录制导航轨迹 |
  | vrun | 控制台命令 | 执行 UE 控制台命令 |
  | random_fov | 最小 最大 | 随机设置 FOV (范围) |
  | random_fov | 数值 | 设置固定 FOV |
  | random_resolution | WxH WxH... | 随机选择分辨率 |
  | record_trajectory | 轨迹类型 | 开始轨迹录制 |

  ------------------------------------------------------------------------------
  轨迹类型 (record_trajectory params)
  ------------------------------------------------------------------------------

  | 轨迹类型 | 说明 |
  |---------|------|
  | rotate_left_30 | 左转 30 度 |
  | rotate_left_45 | 左转 45 度 |
  | rotate_right_30 | 右转 30 度 |
  | rotate_right_45 | 右转 45 度 |
  | rotate_up_30 | 上转 30 度 |
  | rotate_up_45 | 上转 45 度 |
  | rotate_360 | 360 度旋转 |
  | zoom_in | 推进拍摄 |
  | zoom_out | 拉远拍摄 |
  | random_1 ~ random_4 | 随机轨迹 1-4 |
  | render_only | 仅录制 (不移动相机) |
  | render_only_5s | 仅录制 5 秒 |

  ------------------------------------------------------------------------------
  完整示例：Trajectory 任务
  ------------------------------------------------------------------------------

      ```json
      {
        "task": "Trajectory",
        "commands": [
          {"cmd": "vrun", "params": "vset /captureactor/spawn_free_cam"},
          {"cmd": "vrun", "params": "r.ForceLOD 0"},
          {"cmd": "vrun", "params": "r.SkeletalMeshLODBias -10"},
          {"cmd": "create_scene"},
          {"cmd": "delay", "params": "5.0"},
          {"cmd": "prepare_record"},
          {"cmd": "sync_pawn_to_primary_camera"},
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
      ```

  全景相机命令 (/panoramic/*)

  - vget /panoramic/spawn [x] [y] [z] - 在指定位置生成全景相机
  - vget /panoramic/spawn [x] [y] [z] [resolution] - 生成全景相机并设置立方体贴图分辨率
  - vget /panoramic/capture [filename] - 捕获全景等距柱状图图像到文件
  - vget /panoramic/capture [filename] [width] [height] - 自定义分辨率捕获全景图像

  别名命令

  - vrun [cmd] - 运行UE内置控制台命令
  - vrun [cmd] [arg1] ... - 带参数运行控制台命令
  - vexec [actor_id] [funcname] - 调用Actor的BP函数
  - vexec [actor_id] [funcname] [param1] ... - 带参数调用BP函数
  - vbp [actor_id] [funcname] - 调用BP函数并获取输出参数
  - vbp [actor_id] [funcname] [param1] ... - 带参数调用并获取输出
  - vget /persistent_level/id - 获取持久关卡ID
  - vget /persistent_level/level_script_actor/id - 获取关卡脚本Actor ID

  插件命令 (/unrealcv/* /level/*)

  - vget /unrealcv/status - 获取插件状态
  - vget /unrealcv/help - 获取所有命令帮助
  - vget /unrealcv/version - 获取插件版本
  - vget /unrealcv/echo [str] - [调试]回显消息
  - vget /scene/name - 获取场景名称
  - vget /level/name - 获取当前关卡名称