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

  - vget /cameras - 列出所有摄像机
  - vset /cameras/spawn - 生成新摄像机
  - vget /camera/[id]/location - 获取摄像机位置
  - vset /camera/[id]/location [x] [y] [z] - 设置摄像机位置
  - vget /camera/[id]/rotation - 获取摄像机旋转
  - vset /camera/[id]/rotation [pitch] [yaw] [roll] - 设置摄像机旋转
  - vset /camera/[id]/moveto [x] [y] [z] - 移动摄像机（物理碰撞）
  - vget /camera/[id]/lit [filename] - 获取 RGB 图像
  - vget /camera/[id]/depth [filename] - 获取深度数据
  - vget /camera/[id]/normal [filename] - 获取法线图
  - vget /camera/[id]/optical_flow [filename] - 获取光流
  - vget /camera/[id]/object_mask [filename] - 获取物体分割掩码
  - vget /camera/[id]/seg [filename] - 获取分割（同上）
  - vget /camera/[id]/fov - 获取 FOV
  - vset /camera/[id]/fov [float] - 设置 FOV
  - vget /camera/[id]/use_fast_capture - 获取快速捕获模式状态 (0 or 1)
  - vset /camera/[id]/use_fast_capture [uint] - 设置快速捕获模式 (0=disabled, 1=enabled)
  - vget /camera/[id]/size - 获取分辨率
  - vset /camera/[id]/size [width] [height] - 设置分辨率
  - vset /camera/[id]/projection_type [str] - 设置投影类型
  - vset /camera/[id]/ortho_width [float] - 设置正交宽度
  - vset /camera/[id]/lit_source [str] - 设置光照源
  - vset /camera/[id]/reflection [str] - 设置反射方法（None/Lumen/ScreenSpace）
  - vset /camera/[id]/illumination [str] - 设置全局光照方法
  - vset /camera/[id]/exposure_method [str] - 设置曝光方法
  - vset /camera/[id]/exposure_bias [float] - 设置曝光偏差
  - vset /camera/[id]/motion_blur [amount] [max] [per_object] [fps] - 设置运动模糊
  - vset /camera/[id]/focal [distance] [range] - 设置焦点参数
  - vget /screenshot [filename] - 获取截图

  物体命令 (/object/*)

  - vget /objects - 获取所有物体列表
  - vset /objects/spawn_cube - 生成测试立方体
  - vset /objects/spawn [classname] - 生成物体
  - vget /object/[name]/location - 获取物体位置
  - vset /object/[name]/location [x] [y] [z] - 设置物体位置
  - vget /object/[name]/rotation - 获取物体旋转
  - vset /object/[name]/rotation [pitch] [yaw] [roll] - 设置物体旋转
  - vget /object/[name]/scale - 获取物体缩放
  - vset /object/[name]/scale [x] [y] [z] - 设置物体缩放
  - vget /object/[name]/color - 获取物体标注颜色
  - vset /object/[name]/color [r] [g] [b] - 设置物体标注颜色
  - vset /object/[name]/show - 显示物体
  - vset /object/[name]/hide - 隐藏物体
  - vset /object/[name]/destroy - 销毁物体
  - vget /object/[name]/bounds - 获取物体边界
  - vget /object/[name]/uclass_name - 获取 UClass 名称

  记录命令 (/captureactor/*)

  - vset /captureactor/spawn_free_cam - 生成自由摄像机
  - vset /captureactor/time_dilation [float] - 设置录制时间膨胀 (0.1-10.0)
  - vget /captureactor/asset_pool - 查询资产池中的资产
  - vset /captureactor/[id]/record [output_folder] [fps] [duration_seconds] - 开始录像（不干涉相机移动）, id为相机id
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