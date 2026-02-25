# MovieRenderPipeline vs SceneCaptureComponent2D - 技术对比详解

## 核心架构差异

### MovieRenderPipeline 架构

```
应用层 (Blueprint/Python)
    ↓
[UMoviePipelinePrimaryConfig] (配置管理)
    ├─ UMoviePipelineColorSetting (Gamma/OCIO)
    ├─ UMoviePipelineOutputSetting (输出目录/格式)
    └─ UMoviePipelineHighResSetting (分辨率/分块)
    ↓
[FUnrealcvServer → CommandDispatcher] (TCP通信)
    ↓
[UMoviePipelineDeferredPass*] (6+种渲染Pass)
    ├─ RenderSample_GameThreadImpl()
    ├─ SetupImpl() (视图状态管理)
    └─ TeardownImpl()
    ↓
[Deferred Rendering] (线性工作流, Float16/32)
    ├─ SceneColorHDR 输出
    ├─ 可选PostProcess Materials
    └─ 可选OCIO变换
    ↓
[FMoviePipelineSurfaceReader] (GPU异步读取)
    ├─ ResolveSampleToReadbackTexture_RenderThread()
    ├─ FRHIGPUTextureReadback (GPU缓冲)
    └─ 三缓冲机制 (无GPU/CPU同步)
    ↓
[UMoviePipelineImageSequenceOutput] (格式转换)
    ├─ ColorSetting 检查 (Gamma决策)
    ├─ FAsyncImageQuantization (sRGB编码)
    └─ PixelPreProcessors (裁剪/合成)
    ↓
[ImageWriteQueue] (异步磁盘I/O)
    └─ PNG/JPG/BMP/EXR 输出

性能特征: 高吞吐量, 可扩展性强, 多核利用率高
```

### SceneCaptureComponent2D 架构

```
应用层 (Blueprint)
    ↓
[ASceneCapture2D / USceneCaptureComponent2D]
    ├─ CaptureSource (ESceneCaptureSource 选择)
    ├─ TextureTarget (用户指定的RenderTarget)
    ├─ PostProcessSettings
    └─ ShowFlags (渲染控制)
    ↓
[FSceneCaptureFXSystem] (每帧更新)
    ├─ 场景遍历
    └─ 视图状态检索
    ↓
[指定 CaptureSource 的渲染流程]
    ├─ SCS_SceneColorHDR → Deferred Rendering
    ├─ SCS_FinalColorLDR → Tonemapped Output
    ├─ SCS_SceneDepth → Depth Pass
    └─ SCS_BaseColor → Material Pass (Deferred Only)
    ↓
[Direct RenderTarget Write]
    ├─ RenderTarget格式决定色彩空间
    │  ├─ RTF_RGBA16F → 线性浮点 (无Gamma)
    │  └─ RTF_RGBA8 → sRGB编码 (已Gamma)
    └─ PostProcessBlend 应用
    ↓
[CPU访问] (同步)
    ├─ ReadSurfaceData() (GPU 阻塞)
    └─ 直接返回像素数据

性能特征: 低延迟, 简单集成, 实时交互
```

---

## 详细对比表

### 1. 色彩空间管理

| 方面 | MovieRenderPipeline | SceneCaptureComponent2D |
|-----|-------------------|----------------------|
| **内部色彩空间** | 强制线性 (Float32 FLinearColor) | 取决于RenderTarget格式 |
| **Gamma编码时机** | 输出时 (ImageSequenceOutput) | RenderTarget创建时 |
| **Gamma控制方式** | ColorSetting.bDisableToneCurve | PostProcessSettings.bOverride_FilmSlope |
| **Tone Curve** | Filmic (UE标准) 或 OCIO | PostProcessSettings.FilmSlope 等参数 |
| **OCIO支持** | 完整集成 (FOpenColorIODisplayExtension) | 无原生支持 |
| **sRGB RenderTarget** | 禁用 (保持线性) | 用户可选 |

### 2. 渲染Pass系统

| 方面 | MovieRenderPipeline | SceneCaptureComponent2D |
|-----|-------------------|----------------------|
| **预设Pass** | 6个 (Lit, Unlit, Lighting, Reflections, PathTracer, 自定义) | 单一 (基于CaptureSource) |
| **CaptureSource选择** | 固定 SCS_SceneColorHDR | 10种可选 (HDR/LDR, 数据通道) |
| **多Pass输出** | 支持 (6+视图同时渲染) | 不支持 (单一输出) |
| **后处理Pass** | UMoviePipelinePostProcessPass 数组 | PostProcessSettings (单一集合) |
| **Show Flags** | MoviePipelineRenderShowFlagOverride() | ShowFlags 直接设置 |
| **后处理时机** | Deferred + 可选 PostProcess Materials | Deferred → Tonemapping → PostProcess |

### 3. 数据读取与精度

| 方面 | MovieRenderPipeline | SceneCaptureComponent2D |
|-----|-------------------|----------------------|
| **读取机制** | 异步 GPU→CPU (FRHIGPUTextureReadback) | 同步 (ReadSurfaceData) |
| **缓冲策略** | 三缓冲 (GPU/CPU无阻塞) | 单缓冲 (GPU 等待 → 阻塞) |
| **延迟** | ~1-2帧 (可配置) | 0帧 (同步返回) |
| **像素精度** | Float32 中间 → 8/16/32bit 输出 | 取决于 RenderTarget 格式 |
| **Alpha处理** | RGB: sRGB编码, A: 线性 | 取决于 RenderTarget 和 PostProcess |
| **采样方式** | Point (无插值) | 依赖 CaptureSource |

### 4. 输出格式支持

| 格式 | MovieRenderPipeline | SceneCaptureComponent2D |
|-----|-------------------|----------------------|
| **PNG (8bit)** | 支持 (自动 sRGB 编码) | 需手动编码 |
| **JPG (8bit)** | 支持 (自动 sRGB 编码) | 需手动编码 |
| **BMP (8bit)** | 支持 (自动 sRGB 编码) | 需手动编码 |
| **EXR (32bit)** | 支持 (保持线性) | 需手动导出 |
| **DPX** | 不支持 | 不支持 |
| **TGA** | 不支持 | 需手动编码 |

### 5. 性能特征

| 方面 | MovieRenderPipeline | SceneCaptureComponent2D |
|-----|-------------------|----------------------|
| **CPU消耗** | 低 (异步I/O, 并行处理) | 高 (同步读回导致GPU 阻塞) |
| **GPU消耗** | 中等 (多Pass积累) | 低 (单Pass) |
| **内存占用** | 高 (缓冲池 + 中间数据) | 低 (单RenderTarget) |
| **吞吐量** | 高 (40-400K fps 用于数据集) | 低 (实时 ~60 fps) |
| **扩展性** | 优秀 (线程池, 批处理) | 差 (frame-by-frame) |
| **多核利用** | 充分 (异步I/O + 量化) | 部分 (GPU主导) |

### 6. 多摄像机支持

| 方面 | MovieRenderPipeline | SceneCaptureComponent2D |
|-----|-------------------|----------------------|
| **并发** | 支持 (多Camera独立视图状态) | 支持 (实例化) |
| **协调机制** | CommandDispatcher 集中管理 | 各自独立渲染 |
| **数据关联** | ShotIndex + CameraIndex 跟踪 | Actor->Component 关系 |
| **视图历史** | 每Camera每Tile独立 (用于TAA) | 全局 (潜在冲突) |

### 7. 工作流与易用性

| 方面 | MovieRenderPipeline | SceneCaptureComponent2D |
|-----|-------------------|----------------------|
| **集成难度** | 高 (需理解完整管道) | 低 (拖拽配置) |
| **配置复杂度** | 高 (多层级设置) | 低 (单一组件) |
| **学习曲线** | 陡峭 (文档有限) | 平缓 (常见用例) |
| **Debug能力** | 中等 (统计/日志) | 高 (实时预览) |
| **自定义性** | 极高 (源代码改) | 中等 (Blueprint参数) |
| **渲染预览** | 编辑器中可视化 | 实时编辑器显示 |

### 8. 色彩准确度与科学性

| 方面 | MovieRenderPipeline | SceneCaptureComponent2D |
|-----|-------------------|----------------------|
| **线性工作流** | 严格实现 | 依赖用户配置 |
| **Gamma编码** | 精确 (查表+抖动) | 近似 (硬件) |
| **色彩管理** | OCIO (工业标准) | 无 |
| **HDR支持** | 完整 (Float32 EXR) | 部分 (HDR RenderTarget) |
| **色彩空间转换** | 显式可控 | 隐式 (RenderTarget格式) |
| **量化精度** | 可配置 (8/16/32bit) | 固定 (RenderTarget) |

---

## 关键代码路径对比

### Gamma编码流程

**MovieRenderPipeline** (显式):
```
ImageSequenceOutput.OnReceiveImageDataImpl()
  → ColorSetting.OCIOConfiguration.bIsEnabled 检查
  → FAsyncImageQuantization 处理器
    → QuantizeImagePixelDataToBitDepth(..., bConvertToSRGB=true)
      → GenerateSRGBTable() / ConvertLinearTosRGB8bpp()
        → sRGB 查表转换 (Pow(1/2.4) 计算)
        → 随机抖动 (防止banding)
```

**SceneCaptureComponent2D** (隐式):
```
RenderTarget格式选择
  → 创建时决定 (RTF_RGBA8 = sRGB, RTF_RGBA16F = 线性)
  → PostProcessSettings.bOverride_* 参数
    → PostProcess Pass 应用
      → 最终写入RenderTarget (硬件自动Gamma编码)
  → ReadSurfaceData() 返回已编码数据
```

### 性能关键差异

**MovieRenderPipeline** (吞吐量优化):
```cpp
// 三缓冲异步读取
FMoviePipelineSurfaceQueue Surfaces[3];
CurrentFrame → Surface[0]
             ↓
Render Thread: EnqueueCopy() → GPU异步读回
             ↓
Game Thread: 前一帧已准备 → Lock() 无等待
             ↓
Async Writer: 并行编码 + 磁盘I/O
```

**SceneCaptureComponent2D** (低延迟):
```cpp
// 同步读取
TextureRenderTarget2D::ReadPixels()
  → RHI ReadSurface (GPU Flush 等待)
  → CPU 直接返回数据
  ↓ [GPU被阻塞]
```

---

## 用途场景选择矩阵

### 使用 MovieRenderPipeline 的情况

✓ 生成大规模数据集 (>10K 图像)
✓ 电影/高质量渲染输出
✓ 多层次分解渲染 (RGB + 法线 + 深度 + ID)
✓ HDR 输出需求
✓ 精确色彩管理 (OCIO 工作流)
✓ 需要异步处理 (不阻塞主线程)
✓ 批量自动化渲染
✓ 离线渲染农场集成

### 使用 SceneCaptureComponent2D 的情况

✓ 实时交互应用
✓ 游戏内渲染目标 (UI, 镜子效果)
✓ 低延迟反馈 (VR, 动作捕捉)
✓ 简单集成需求
✓ 内存受限设备
✓ 单一或少量输出
✓ 快速原型化
✓ 编辑器预览

---

## 针对UnrealCV 的应用建议

### MQRC (MovieQualityRenderComponent) 最佳实践

1. **保持线性管道**
   - 使用 Float32 内部纹理
   - 延迟 Gamma 编码到输出阶段
   - 代码位置: `MovieQualityRenderComponent.cpp`

2. **Gamma 控制策略**
   ```cpp
   // 在 MQRC 中检查 MRP ColorSetting
   UMoviePipelineColorSetting* ColorSetting =
       GetMoviePipeline()->GetPipelinePrimaryConfig()->FindSetting<UMoviePipelineColorSetting>();

   if (ColorSetting && ColorSetting->OCIOConfiguration.bIsEnabled) {
       // OCIO 路径 - 颜色管理由 OCIO 负责
       render_with_OCIO();
   } else {
       // 线性路径 - Gamma 在输出时应用
       render_linear();
   }
   ```

3. **输出格式决策**
   - PNG: 快速预览 (小文件, 广泛兼容)
   - EXR: 最终输出 (无损, 线性保存, 后期处理)
   - 组合: 同时生成两种 (预览 + 存档)

4. **性能优化**
   - 利用异步 GPU 读取 (不等待同步)
   - 批量处理多帧 (提高 CPU 缓存命中率)
   - 并行编码处理 (FImageWriteQueue)

---

## 总结表

| 功能 | MRP | SCC2D | 适用场景 |
|-----|-----|-------|--------|
| 线性工作流 | 完整 | 手动 | 数据集 |
| Gamma控制 | 显式精确 | 隐式近似 | 色彩准确度高 |
| 多Pass | 支持 | 不支持 | 分层渲染 |
| 异步处理 | 完整 | 无 | 高吞吐量 |
| 易用性 | 低 | 高 | 快速集成 |
| 性能 | 吞吐量优 | 延迟优 | 批量vs实时 |
| OCIO | 支持 | 无 | 工业色彩 |
| 推荐用途 | 离线数据 | 实时应用 | - |
