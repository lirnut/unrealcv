# MovieRenderPipeline Gamma 控制 - UnrealCV 集成指南

## 前言

本文档针对 UnrealCV 的 MQRC (MovieQualityRenderComponent) 如何集成 MovieRenderPipeline 的 Gamma 控制机制进行详细说明。

---

## 一、Gamma 信息获取路径

### 1.1 获取 ColorSetting

```cpp
// 位置: MoviePipelineColorSetting.h
// 存储位置: PipelineConfig → Primary Config → Settings

void FMovieQualityRenderComponent::GetColorSettings()
{
    // 方法1: 通过 Movie Pipeline 配置
    if (UMoviePipeline* MoviePipeline = GetMoviePipeline())
    {
        UMoviePipelinePrimaryConfig* PrimaryConfig = MoviePipeline->GetPipelinePrimaryConfig();
        if (PrimaryConfig)
        {
            UMoviePipelineColorSetting* ColorSetting =
                PrimaryConfig->FindSetting<UMoviePipelineColorSetting>();

            if (ColorSetting)
            {
                // 检查 OCIO 是否启用
                bool bOCIOEnabled = ColorSetting->OCIOConfiguration.bIsEnabled;

                // 检查 Tone Curve 是否禁用
                bool bToneCurveDisabled = ColorSetting->bDisableToneCurve;

                UE_LOG(LogMovieRenderPipeline, Log,
                    TEXT("OCIO Enabled: %s, Tone Curve Disabled: %s"),
                    bOCIOEnabled ? TEXT("true") : TEXT("false"),
                    bToneCurveDisabled ? TEXT("true") : TEXT("false"));
            }
        }
    }
}
```

### 1.2 OCIO 配置结构

```cpp
// 来自: OpenColorIOColorSpace.h

struct FOpenColorIODisplayConfiguration
{
    // 是否启用 OCIO
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsEnabled = false;

    // 配置文件路径
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FOpenColorIOColorSpace ColorConfiguration;

    // 显示设备名称 (如 "sRGB", "Rec.709")
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString DisplayContext;

    // 色彩空间查看模式
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 ViewingMode = 0;
};
```

---

## 二、Gamma 编码实现

### 2.1 sRGB 编码查表

```cpp
// 直接使用 MRP 的编码函数

#include "MoviePipelineImageQuantization.h"

void FMovieQualityRenderComponent::ApplysRGBEncoding(
    FFloat16Color* InLinearData,
    int64 PixelCount,
    bool bIncludeDithering)
{
    if (!InLinearData || PixelCount <= 0)
    {
        return;
    }

    // 生成 sRGB 查表 (内部缓存)
    static TArray<float> sRGBTable = GenerateSRGBTableFloat16toFloat();
    static TArray<float> RandStreamTable = PreGenerateRandomTable();

    // 并行处理像素
    ParallelFor((PixelCount + 65535) / 65536,
        [&](int32 BatchIndex)
        {
            const int64 StartPixel = BatchIndex * 65536;
            const int64 EndPixel = FMath::Min(StartPixel + 65536, PixelCount);

            for (int64 PixelIndex = StartPixel; PixelIndex < EndPixel; ++PixelIndex)
            {
                FFloat16Color& Pixel = InLinearData[PixelIndex];

                // 查表获取 sRGB 值 (0-255 范围)
                float RsRGB = sRGBTable[Pixel.R.Encoded];
                float GsRGB = sRGBTable[Pixel.G.Encoded];
                float BsRGB = sRGBTable[Pixel.B.Encoded];

                // 可选: 添加抖动减少条纹
                if (bIncludeDithering)
                {
                    RsRGB += RandStreamTable[(PixelIndex * 3 + 0) % RandStreamTable.Num()];
                    GsRGB += RandStreamTable[(PixelIndex * 3 + 1) % RandStreamTable.Num()];
                    BsRGB += RandStreamTable[(PixelIndex * 3 + 2) % RandStreamTable.Num()];
                }

                // 转换为 8-bit
                Pixel.R = FFloat16((uint8)FMath::Clamp(RsRGB, 0.f, 255.f));
                Pixel.G = FFloat16((uint8)FMath::Clamp(GsRGB, 0.f, 255.f));
                Pixel.B = FFloat16((uint8)FMath::Clamp(BsRGB, 0.f, 255.f));
                // Alpha 保持线性不变
            }
        });
}

// 辅助函数: 生成 sRGB 编码查表
static TArray<float> GenerateSRGBTableFloat16toFloat()
{
    TArray<float> OutsRGBTable;
    OutsRGBTable.SetNumUninitialized(65536);

    for (int32 TableIndex = 0; TableIndex < 65536; ++TableIndex)
    {
        FFloat16 Value;
        Value.Encoded = TableIndex;
        float ValueAsLinear = (float)Value;

        // sRGB 分段函数
        if (ValueAsLinear <= 0.0031308f)
        {
            ValueAsLinear = ValueAsLinear * 12.92f;
        }
        else
        {
            ValueAsLinear = FMath::Pow(ValueAsLinear, 1.0f / 2.4f) * 1.055f - 0.055f;
        }

        OutsRGBTable[TableIndex] = (ValueAsLinear * 255.f);
    }

    return OutsRGBTable;
}

// 辅助函数: 预生成随机抖动表
static TArray<float> PreGenerateRandomTable()
{
    TArray<float> RandTable;
    FRandomStream RandStream(31337);

    int32 TableSize = (1024 * 1024) + 7;
    RandTable.SetNumUninitialized(TableSize);

    for (int32 Index = 0; Index < TableSize; ++Index)
    {
        float Value = RandStream.GetFraction();
        *(uint32*)&Value &= 0xFFFFFF00U;  // 限制精度
        RandTable[Index] = Value;
    }

    return RandTable;
}
```

### 2.2 直接使用 MRP 的量化函数

```cpp
#include "MoviePipelineImageQuantization.h"

void FMovieQualityRenderComponent::QuantizeToSRGB(
    TUniquePtr<FImagePixelData>& InOutPixelData)
{
    // 使用 MRP 提供的标准量化函数
    TUniquePtr<FImagePixelData> QuantizedData =
        UE::MoviePipeline::QuantizeImagePixelDataToBitDepth(
            InOutPixelData.Get(),
            8,                    // 目标 8-bit
            nullptr,              // Payload
            true                  // bConvertToSrgb = true
        );

    InOutPixelData = MoveTemp(QuantizedData);
}
```

---

## 三、Tone Curve 处理

### 3.1 检查 Tone Curve 状态

```cpp
void FMovieQualityRenderComponent::HandleToneCurve()
{
    UMoviePipelineColorSetting* ColorSetting = GetColorSetting();

    if (!ColorSetting)
    {
        // 默认应用 Tone Curve (旧行为兼容)
        bApplyFilmicToneCurve = true;
        return;
    }

    if (ColorSetting->OCIOConfiguration.bIsEnabled)
    {
        // OCIO 启用 → Tone Curve 强制禁用
        bApplyFilmicToneCurve = false;
        UE_LOG(LogMovieRenderPipeline, Warning,
            TEXT("OCIO enabled, disabling Filmic Tone Curve"));
    }
    else
    {
        // 使用用户设置
        bApplyFilmicToneCurve = !ColorSetting->bDisableToneCurve;
    }
}
```

### 3.2 Tone Curve 应用位置

```cpp
// 在 Deferred Rendering Pass 中

void FMovieQualityRenderComponent::RenderDeferredPass()
{
    FSceneViewFamily ViewFamily = ...;

    // 配置 Post Process 设置
    FPostProcessSettings PostProcessSettings;

    // 检查是否应用 Tone Curve
    if (bApplyFilmicToneCurve)
    {
        // Filmic Tone Curve 通过 ShowFlags 启用
        ViewFamily.EngineShowFlags.SetTonemapper(true);

        // 配置 Tone Curve 参数
        PostProcessSettings.bOverride_FilmSlope = 1;
        PostProcessSettings.FilmSlope = 0.88f;  // UE 标准值

        PostProcessSettings.bOverride_FilmToe = 1;
        PostProcessSettings.FilmToe = 0.55f;

        PostProcessSettings.bOverride_FilmShoulder = 1;
        PostProcessSettings.FilmShoulder = 0.26f;

        PostProcessSettings.bOverride_FilmBlackClip = 1;
        PostProcessSettings.FilmBlackClip = 0.0f;

        PostProcessSettings.bOverride_FilmWhiteClip = 1;
        PostProcessSettings.FilmWhiteClip = 0.04f;
    }
    else
    {
        // Disable Tonemapping 保持线性
        ViewFamily.EngineShowFlags.SetTonemapper(false);
    }

    // 执行渲染...
}
```

---

## 四、OCIO 集成

### 4.1 获取 OCIO 配置

```cpp
void FMovieQualityRenderComponent::SetupOCIO()
{
    UMoviePipelineColorSetting* ColorSetting = GetColorSetting();

    if (!ColorSetting || !ColorSetting->OCIOConfiguration.bIsEnabled)
    {
        // OCIO 未启用
        bUseOCIO = false;
        return;
    }

    bUseOCIO = true;

    // 获取 OCIO 配置
    const FOpenColorIODisplayConfiguration& OCIOConfig =
        ColorSetting->OCIOConfiguration;

    // 加载 OCIO 配置文件 (如需)
    FString ConfigFilePath = OCIOConfig.ColorConfiguration.ConfigurationPath;
    FString WorkingColorSpace = OCIOConfig.ColorConfiguration.WorkingColorSpaceName;
    FString DisplayColorSpace = OCIOConfig.ColorConfiguration.DisplayColorSpaceName;
    FString ViewTransform = OCIOConfig.ColorConfiguration.DisplayViewName;

    UE_LOG(LogMovieRenderPipeline, Log,
        TEXT("OCIO Config: %s, Working: %s, Display: %s, View: %s"),
        *ConfigFilePath, *WorkingColorSpace, *DisplayColorSpace, *ViewTransform);
}
```

### 4.2 应用 OCIO 变换 (通过 SceneViewExtension)

```cpp
#include "OpenColorIODisplayExtension.h"

class FMovieQualityOCIOSceneViewExtension : public FSceneViewExtensionBase
{
public:
    FMovieQualityOCIOSceneViewExtension(
        const FAutoRegister& AutoRegister,
        const FOpenColorIODisplayConfiguration& InOCIOConfig)
        : FSceneViewExtensionBase(AutoRegister)
        , OCIOConfig(InOCIOConfig)
    {
    }

    virtual void SetupViewFamily(FSceneViewFamilyContext& InViewFamily) override
    {
        // OCIO 会在 PostProcess 阶段自动应用
    }

    virtual void BeginRenderingViewFamily(FSceneViewFamilyContext& InViewFamily) override
    {
        // 配置 OCIO 变换
        // (具体实现由 OpenColorIODisplayExtension 处理)
    }

private:
    FOpenColorIODisplayConfiguration OCIOConfig;
};

// 在 MQRC 中使用:
void FMovieQualityRenderComponent::SetupOCIOViewExtension(
    const FSceneViewFamily& InViewFamily)
{
    UMoviePipelineColorSetting* ColorSetting = GetColorSetting();

    if (ColorSetting && ColorSetting->OCIOConfiguration.bIsEnabled)
    {
        // 创建 OCIO 视图扩展
        OCIOSceneViewExtension = FSceneViewExtensions::NewExtension<
            FMovieQualityOCIOSceneViewExtension>(
            ColorSetting->OCIOConfiguration);
    }
}
```

---

## 五、输出格式与 Gamma 的关系

### 5.1 格式决策树

```cpp
void FMovieQualityRenderComponent::DetermineOutputGamma(
    EImageFormat InFormat,
    bool& bOutApplySRGB,
    bool& bOutHighPrecision)
{
    UMoviePipelineColorSetting* ColorSetting = GetColorSetting();

    switch (InFormat)
    {
        case EImageFormat::PNG:
        case EImageFormat::JPEG:
        case EImageFormat::BMP:
        {
            // 8-bit 格式必须应用 sRGB
            bOutHighPrecision = false;

            // 除非 OCIO 启用
            if (ColorSetting && ColorSetting->OCIOConfiguration.bIsEnabled)
            {
                bOutApplySRGB = false;  // OCIO 负责色彩变换
            }
            else
            {
                bOutApplySRGB = true;   // 标准 sRGB 编码
            }
            break;
        }

        case EImageFormat::EXR:
        {
            // EXR 保持线性 + 高精度
            bOutHighPrecision = true;
            bOutApplySRGB = false;      // 不应用 sRGB
            break;
        }

        default:
            bOutApplySRGB = false;
            bOutHighPrecision = false;
            break;
    }

    UE_LOG(LogMovieRenderPipeline, Log,
        TEXT("Format: %d, Apply sRGB: %s, High Precision: %s"),
        (int32)InFormat,
        bOutApplySRGB ? TEXT("true") : TEXT("false"),
        bOutHighPrecision ? TEXT("true") : TEXT("false"));
}
```

### 5.2 RenderTarget 格式选择

```cpp
EPixelFormat FMovieQualityRenderComponent::GetRenderTargetFormat(
    bool bHighPrecision)
{
    // MRP 总是使用线性格式
    // 绝不使用 sRGB RenderTarget (让 Gamma 在输出时应用)

    if (bHighPrecision)
    {
        return EPixelFormat::PF_FloatRGBA;   // 32-bit 浮点 (线性)
    }
    else
    {
        return EPixelFormat::PF_A2B10G10R10F;  // 16-bit 浮点 (线性)
    }
}
```

---

## 六、完整的集成流程

### 6.1 初始化 (SetupImpl)

```cpp
void FMovieQualityRenderComponent::SetupImpl(
    const MoviePipeline::FMoviePipelineRenderPassInitSettings& InPassInitSettings)
{
    Super::SetupImpl(InPassInitSettings);

    // 1. 获取 Color Setting
    UMoviePipelineColorSetting* ColorSetting = GetColorSetting();

    // 2. 确定 Gamma 策略
    if (ColorSetting)
    {
        if (ColorSetting->OCIOConfiguration.bIsEnabled)
        {
            // OCIO 路径
            bUseOCIO = true;
            bApplyFilmicToneCurve = false;
            SetupOCIOViewExtension();
        }
        else
        {
            // 线性路径
            bUseOCIO = false;
            bApplyFilmicToneCurve = !ColorSetting->bDisableToneCurve;
        }
    }
    else
    {
        // 默认行为 (兼容旧版)
        bUseOCIO = false;
        bApplyFilmicToneCurve = true;
    }

    // 3. 创建 RenderTarget (始终线性)
    CreateRenderTarget(EPixelFormat::PF_FloatRGBA, bUseHighPrecision);

    // 4. 创建 Surface 队列
    CreateSurfaceQueue();

    UE_LOG(LogMovieRenderPipeline, Log,
        TEXT("Setup: OCIO=%s, ToneCurve=%s, HighPrecision=%s"),
        bUseOCIO ? TEXT("enabled") : TEXT("disabled"),
        bApplyFilmicToneCurve ? TEXT("enabled") : TEXT("disabled"),
        bUseHighPrecision ? TEXT("yes") : TEXT("no"));
}
```

### 6.2 渲染 (RenderSample_GameThreadImpl)

```cpp
void FMovieQualityRenderComponent::RenderSample_GameThreadImpl(
    const FMoviePipelineRenderPassMetrics& InSampleState)
{
    // 1. 设置 Tone Curve 行为
    if (bApplyFilmicToneCurve)
    {
        // Filmic Tone Curve 将在 Deferred 渲染中应用
        ApplyFilmicToneCurveSettings();
    }

    // 2. 执行 Deferred 渲染 (线性输出)
    FSceneViewFamilyContext ViewFamily = CalculateViewFamily(InSampleState);

    // 3. 异步读取 GPU 数据
    EnqueueGPUReadback();

    Super::RenderSample_GameThreadImpl(InSampleState);
}
```

### 6.3 读取和转换 (OnGPUReadbackComplete)

```cpp
void FMovieQualityRenderComponent::OnGPUReadbackComplete(
    TUniquePtr<FImagePixelData>&& InPixelData)
{
    // 此时 InPixelData 是线性浮点

    // 1. 检查是否需要量化
    EImageFormat OutputFormat = GetOutputFormat();
    bool bApplySRGB = false;
    bool bHighPrecision = false;

    DetermineOutputGamma(OutputFormat, bApplySRGB, bHighPrecision);

    // 2. 应用 Gamma (仅 8-bit 格式)
    if (bApplySRGB)
    {
        if (bUseOCIO)
        {
            // OCIO 变换 (不同的色彩空间映射)
            ApplyOCIOTransform(InPixelData);
        }
        else
        {
            // 标准 sRGB 编码
            QuantizeToSRGB(InPixelData);
        }
    }
    // else: EXR 保持线性

    // 3. 写入文件
    WritePixelDataToFile(MoveTemp(InPixelData), OutputFormat);
}
```

---

## 七、诊断与调试

### 7.1 Gamma 链路验证

```cpp
void FMovieQualityRenderComponent::VerifyGammaChain()
{
    UMoviePipelineColorSetting* ColorSetting = GetColorSetting();

    if (!ColorSetting)
    {
        UE_LOG(LogMovieRenderPipeline, Warning,
            TEXT("ColorSetting not found! Using default Gamma behavior"));
        return;
    }

    // 检查配置一致性
    bool bOCIOEnabled = ColorSetting->OCIOConfiguration.bIsEnabled;
    bool bToneCurveDisabled = ColorSetting->bDisableToneCurve;

    if (bOCIOEnabled && !bToneCurveDisabled)
    {
        UE_LOG(LogMovieRenderPipeline, Warning,
            TEXT("Inconsistent: OCIO enabled but Tone Curve not disabled. "
                 "Tone Curve will be ignored."));
    }

    // 打印最终配置
    UE_LOG(LogMovieRenderPipeline, Log,
        TEXT("Gamma Configuration:"));
    UE_LOG(LogMovieRenderPipeline, Log,
        TEXT("  - OCIO Enabled: %s"), bOCIOEnabled ? TEXT("true") : TEXT("false"));
    UE_LOG(LogMovieRenderPipeline, Log,
        TEXT("  - Tone Curve Disabled: %s"), bToneCurveDisabled ? TEXT("true") : TEXT("false"));
    UE_LOG(LogMovieRenderPipeline, Log,
        TEXT("  - Effective Gamma Strategy: %s"),
        bOCIOEnabled ? TEXT("OCIO") : (bToneCurveDisabled ? TEXT("Linear") : TEXT("Filmic")));
}
```

### 7.2 输出验证

```cpp
void FMovieQualityRenderComponent::ValidateOutputFormat(
    EImageFormat InFormat,
    const FIntPoint& InResolution)
{
    bool bApplySRGB = false;
    bool bHighPrecision = false;

    DetermineOutputGamma(InFormat, bApplySRGB, bHighPrecision);

    UE_LOG(LogMovieRenderPipeline, Log,
        TEXT("Output Configuration:"));
    UE_LOG(LogMovieRenderPipeline, Log,
        TEXT("  - Format: %d (PNG=5, JPG=6, BMP=7, EXR=8)"), (int32)InFormat);
    UE_LOG(LogMovieRenderPipeline, Log,
        TEXT("  - Resolution: %dx%d"), InResolution.X, InResolution.Y);
    UE_LOG(LogMovieRenderPipeline, Log,
        TEXT("  - Apply sRGB: %s"), bApplySRGB ? TEXT("true") : TEXT("false"));
    UE_LOG(LogMovieRenderPipeline, Log,
        TEXT("  - High Precision: %s"), bHighPrecision ? TEXT("true") : TEXT("false"));
}
```

---

## 八、常见问题与解决

### Q1: 输出图像太暗/太亮

**原因**: Gamma 编码方向错误

```cpp
// 诊断
- 检查输出格式 (8-bit 应该自动应用 sRGB)
- 检查 ColorSetting.bDisableToneCurve 值
- 检查 OCIO 是否意外启用
```

### Q2: PNG 和 EXR 色彩不一致

**原因**: 一个应用了 sRGB, 一个保持线性

```cpp
// 预期行为 (正确):
PNG: sRGB 编码 + 显示 (色彩校正)
EXR: 线性保存 (供后期处理)

// 它们看起来"不一样"是正常的
// PNG 经过 Gamma 编码看起来更亮/更鲜艳
```

### Q3: 如何在 Python 客户端中控制 Gamma?

```python
# unrealcv 客户端

# 启用 OCIO
client.request('vset /unrealcv/colorsetting/ocio_enabled true')

# 禁用 Tone Curve
client.request('vset /unrealcv/colorsetting/disable_tone_curve true')

# 查询当前状态
result = client.request('vget /unrealcv/colorsetting/status')
# 返回: "OCIO:true ToneCurve:false"
```

---

## 九、性能优化建议

### 9.1 查表缓存

```cpp
// 全局缓存 sRGB 查表，避免重复生成
static TArray<float> g_sRGBTable;
static bool g_bsRGBTableInitialized = false;

void FMovieQualityRenderComponent::InitializeSRGBTables()
{
    if (g_bsRGBTableInitialized)
        return;

    g_sRGBTable = GenerateSRGBTableFloat16toFloat();
    g_bsRGBTableInitialized = true;

    UE_LOG(LogMovieRenderPipeline, Log,
        TEXT("sRGB table cached (%.2f MB)"),
        (float)g_sRGBTable.Num() * sizeof(float) / (1024.f * 1024.f));
}
```

### 9.2 异步量化

```cpp
// 使用 ImageWriteQueue 的异步处理

void FMovieQualityRenderComponent::WritePixelDataAsync(
    TUniquePtr<FImagePixelData> InPixelData)
{
    // ImageWriteQueue 会在线程池中执行这些操作
    FImageWriteTask* WriteTask = new FImageWriteTask();
    WriteTask->Format = EImageFormat::PNG;
    WriteTask->Filename = OutputFilePath;
    WriteTask->PixelData = MoveTemp(InPixelData);

    // 添加量化处理器 (async)
    WriteTask->PixelPreProcessors.Add(
        FAsyncImageQuantization(WriteTask, true /* bConvertToSRGB */)
    );

    // 排队 (不阻塞主线程)
    ImageWriteQueue->Enqueue(WriteTask);
}
```

---

## 十、参考资源

### 关键源文件

| 功能 | 文件路径 |
|-----|---------|
| Color Setting | `MovieRenderPipelineCore/Public/MoviePipelineColorSetting.h` |
| Quantization | `MovieRenderPipelineCore/Private/MoviePipelineImageQuantization.cpp` |
| Surface Reader | `MovieRenderPipelineCore/Private/MoviePipelineSurfaceReader.cpp` |
| Image Output | `MovieRenderPipelineRenderPasses/Private/MoviePipelineImageSequenceOutput.cpp` |
| Deferred Pass | `MovieRenderPipelineRenderPasses/Public/MoviePipelineDeferredPasses.h` |
| OCIO Helper | `MovieRenderPipelineCore/Private/Graph/MovieGraphOCIOHelper.cpp` |

### 相关类

```cpp
// 色彩管理
UMoviePipelineColorSetting
FOpenColorIODisplayConfiguration
FOpenColorIODisplayExtension

// 数据处理
FMoviePipelineSurfaceReader
FMoviePipelineSurfaceQueue
UE::MoviePipeline::QuantizeImagePixelDataToBitDepth()

// 输出
UMoviePipelineImageSequenceOutputBase
FAsyncImageQuantization

// 渲染
UMoviePipelineDeferredPassBase
FMoviePipelinePostProcessPass
```

---

## 总结

MovieRenderPipeline 的 Gamma 控制包含三个主要层次：

1. **配置层** (ColorSetting): 用户选择是否启用 OCIO 或 Tone Curve
2. **处理层** (Quantization): 根据配置选择合适的色彩变换
3. **输出层** (ImageSequenceOutput): 根据文件格式选择最终编码方式

通过理解这三层的交互，可以在 UnrealCV 的 MQRC 中正确集成 Gamma 控制，实现与标准 MRP 一致的色彩管理行为。
