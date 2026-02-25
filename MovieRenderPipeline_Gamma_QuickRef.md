# MovieRenderPipeline Gamma 控制 - 快速参考卡

## 核心公式

### sRGB 编码 (Float → 8-bit)

```
线性值 v ≤ 0.0031308:
  sRGB = 12.92 × v

线性值 v > 0.0031308:
  sRGB = 1.055 × v^(1/2.4) - 0.055

最终 8-bit 值 = floor(sRGB × 255 + 抖动)
```

## 关键类 & 函数

| 类/函数 | 位置 | 用途 |
|--------|-----|-----|
| `UMoviePipelineColorSetting` | ColorSetting.h | 色彩配置容器 |
| `OCIOConfiguration` | ColorSetting.h | OCIO 开关 |
| `bDisableToneCurve` | ColorSetting.h | Tone Curve 开关 |
| `QuantizeImagePixelDataToBitDepth()` | ImageQuantization.h | 标准量化函数 |
| `GenerateSRGBTable()` | ImageQuantization.cpp | 生成 sRGB 查表 |
| `ConvertLinearTosRGB8bpp()` | ImageQuantization.cpp | Float → sRGB |
| `FMoviePipelineSurfaceReader` | SurfaceReader.h | GPU 异步读取 |
| `FMoviePipelineImageSequenceOutput` | ImageSequenceOutput.h | 输出处理 |

## 配置决策树

```
ColorSetting 可用?
├─ NO → 使用默认行为 (Tone Curve 启用)
└─ YES
    ├─ OCIO 启用?
    │  ├─ YES
    │  │  ├─ Tone Curve 强制禁用
    │  │  ├─ 应用 OCIO 变换
    │  │  └─ 输出格式 = EXR (推荐) 或 PNG (OCIO 已处理色彩)
    │  └─ NO
    │     ├─ 检查 bDisableToneCurve
    │     └─ 决定是否应用 Filmic Tone Curve
    │
    └─ 输出格式选择
       ├─ PNG/JPG/BMP (8-bit)
       │  └─ 应用 sRGB 编码 (查表 + 抖动)
       └─ EXR (32-bit)
          └─ 保持线性浮点
```

## 代码片段

### 获取 ColorSetting

```cpp
if (UMoviePipeline* Pipeline = GetMoviePipeline())
{
    UMoviePipelinePrimaryConfig* Config = Pipeline->GetPipelinePrimaryConfig();
    UMoviePipelineColorSetting* ColorSetting =
        Config->FindSetting<UMoviePipelineColorSetting>();
}
```

### 检查 Gamma 策略

```cpp
bool bApplySRGB = true;
if (ColorSetting && ColorSetting->OCIOConfiguration.bIsEnabled)
{
    bApplySRGB = false;  // OCIO 负责
}
```

### 快速量化

```cpp
TUniquePtr<FImagePixelData> Quantized =
    UE::MoviePipeline::QuantizeImagePixelDataToBitDepth(
        PixelData.Get(),
        8,
        nullptr,
        bApplySRGB
    );
```

## 文件格式 vs Gamma

| 格式 | 色彩空间 | 精度 | sRGB? | 用途 |
|------|---------|------|------|-----|
| PNG | 显示色彩 | 8-bit | ✓ | 快速预览 |
| JPG | 显示色彩 | 8-bit | ✓ | 小文件 |
| EXR | 线性 | 32-bit | ✗ | 高精度、后期 |

## 性能指标 (参考值)

| 操作 | 时间 | 备注 |
|------|------|------|
| GPU 读取延迟 | ~1-2 帧 | 异步三缓冲 |
| sRGB 查表生成 | ~1 ms | 一次性 (缓存) |
| Float16 → sRGB 8-bit | ~10 ms | 4K@30fps, 并行处理 |
| 抖动应用 | +5 ms | 可选 |
| PNG 编码 | ~50 ms | ImageWriteQueue (异步) |
| EXR 编码 | ~200 ms | 高精度编码 |

## 常用配置

### 场景 1: 快速数据集生成 (推荐)

```cpp
ColorSetting->OCIOConfiguration.bIsEnabled = false;
ColorSetting->bDisableToneCurve = false;  // 启用 Filmic
OutputFormat = PNG;
// 结果: 自然色彩、快速编码、小文件
```

### 场景 2: 精确色彩空间 (专业)

```cpp
ColorSetting->OCIOConfiguration.bIsEnabled = true;
ColorSetting->OCIOConfiguration.DisplayContext = "sRGB";
OutputFormat = EXR;
// 结果: 准确色彩、保留数据、可后期处理
```

### 场景 3: 线性数据导出 (学术)

```cpp
ColorSetting->OCIOConfiguration.bIsEnabled = false;
ColorSetting->bDisableToneCurve = true;  // 禁用 Tone Curve
OutputFormat = EXR;
// 结果: 纯线性数据、无显示调整、最大灵活性
```

## 调试命令 (Python 客户端)

```python
from unrealcv import Client

c = Client(('127.0.0.1', 9000))
c.connect()

# 查询 Gamma 配置
status = c.request('vget /unrealcv/colorsetting/status')
print(status)

# 设置 Tone Curve
c.request('vset /unrealcv/colorsetting/disable_tone_curve true')

# 设置 OCIO
c.request('vset /unrealcv/colorsetting/ocio_enabled true')

# 输出格式选择 (应该已在配置中)
# PNG: 自动 sRGB
# EXR: 保持线性
```

## 常见错误

| 错误 | 症状 | 解决方案 |
|------|------|--------|
| 双重 Gamma | 图像过度暗化 | 检查 RenderTarget 不应该是 sRGB 格式 |
| 缺失 Gamma | 8-bit 输出太亮 | 检查 ColorSetting 是否正确加载 |
| OCIO + Tone Curve | 不可预测的色彩 | OCIO 启用时自动禁用 Tone Curve |
| 抖动不足 | 明显色带 | 启用抖动减少量化伪迹 |

## 性能优化

1. **缓存 sRGB 查表** (~65KB, 一次生成)
2. **使用异步 ImageWriteQueue** (不阻塞主线程)
3. **并行化量化** (ParallelFor 4K@60fps)
4. **EXR 用 16-bit** (如果不需要 32-bit)

## 源代码速查

```cpp
// 色彩决策
MoviePipelineColorSetting.h:L32-35  // bDisableToneCurve, OCIOConfiguration

// sRGB 编码
MoviePipelineImageQuantization.cpp:L51-76   // GenerateSRGBTable()
MoviePipelineImageQuantization.cpp:L101-138 // GenerateSRGBTableFloat16toFloat()
MoviePipelineImageQuantization.cpp:L168-232 // ConvertLinearTosRGB8bpp()

// 输出处理
MoviePipelineImageSequenceOutput.cpp:L239-259  // 格式决策
MoviePipelineImageSequenceOutput.cpp:L34-47    // FAsyncImageQuantization

// GPU 读取
MoviePipelineSurfaceReader.cpp:L77-161  // ResolveSampleToReadbackTexture_RenderThread()
MoviePipelineSurfaceReader.cpp:L163-251 // CopyReadbackTexture_RenderThread()
```

## 官方文档参考

- UE MovieRenderPipeline 文档
- OpenColorIO 标准 (ocio.org)
- sRGB 规范 (IEC 61966-2-1)

## 关键数字

- **sRGB 分段点**: 0.0031308
- **sRGB 幂值**: 1/2.4 ≈ 0.41667
- **查表大小**: 65536 (Float16 所有值)
- **抖动表大小**: 1M+ (避免模式)
- **缓冲队列**: 3 (GPU异步读取)

---

## 快速决策表

### "我应该用什么？"

| 需求 | 推荐 | 替代 |
|------|-----|------|
| 数据集生成 | PNG + Filmic | EXR + Linear |
| 电影输出 | EXR + OCIO | PNG + Filmic |
| 学术研究 | EXR + Linear | 不用 PNG |
| 实时预览 | PNG + Filmic | 不用 EXR |
| 色彩准确 | EXR + OCIO | PNG + Filmic |

### "我遇到了什么问题？"

| 问题 | 原因 | 修复 |
|------|------|------|
| 太暗 | 双重 Gamma | 检查 RenderTarget 格式 |
| 太亮 | 缺失 Gamma | 确保 bApplySRGB = true |
| 色彩不对 | OCIO vs Filmic | 互斥，检查配置 |
| 条纹 | 无抖动 | 启用 dithering |

---

*最后更新: 2026-02-25 | UE 5.6 | MovieRenderPipeline*
