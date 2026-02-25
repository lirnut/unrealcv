# MovieRenderPipeline Gamma控制机制详细分析 (UE 5.6)

## 1. 核心概览

MovieRenderPipeline (MRP) 实现了一个**完整的线性工作流（Linear Workflow）** 系统，从像素读取到最终输出文件都涵盖了Gamma管理。

### 关键特性
- **多阶段Gamma处理**：GPU渲染 → 像素读取 → 8bit量化 → 文件写入
- **OCIO集成**：支持OpenColorIO进行高级色彩管理
- **Tone Curve控制**：通过ColorSetting独立控制
- **线性RenderTarget**：内部使用线性色彩空间进行计算
- **输出格式支持**：PNG/JPG/BMP (8bit) 和 EXR (高精度)

---

## 2. 像素读取机制 (MoviePipelineSurfaceReader)

### 文件位置
`H:\UE_5.6\Engine\Plugins\MovieScene\MovieRenderPipeline\Source\MovieRenderPipelineCore\Private\MoviePipelineSurfaceReader.cpp`

### 核心流程

#### 2.1 GPU到CPU传输
```cpp
void FMoviePipelineSurfaceReader::ResolveSampleToReadbackTexture_RenderThread(
    const FTextureRHIRef& SourceSurfaceSample)
{
    FPooledRenderTargetDesc OutputDesc = FPooledRenderTargetDesc::Create2DDesc(
        TargetSize,
        PixelFormat,
        FClearValueBinding::None,
        TexCreate_None,
        TexCreate_RenderTargetable,
        false);

    TShaderMapRef<FScreenVS> VertexShader(ShaderMap);
    TShaderMapRef<FScreenPS> PixelShader(ShaderMap);

    ReadbackTexture->EnqueueCopy(RHICmdList, ResampleTexturePooledRenderTarget->GetRHI());
}
```

**关键点**：
- **FRHIGPUTextureReadback**：GPU端数据无损复制到CPU可读缓冲区
- **三缓冲机制**：避免GPU/CPU同步阻塞（FrameResolveLatency=1）
- **Point采样**：保证像素精确性（无插值失真）

#### 2.2 支持的像素格式
```cpp
switch (PixelFormat)
{
    case EPixelFormat::PF_FloatRGBA:
        TUniquePtr<TImagePixelData<FFloat16Color>> NewPixelData;
        break;

    case EPixelFormat::PF_B8G8R8A8:
        TUniquePtr<TImagePixelData<FColor>> NewPixelData;
        break;
}
```

**注意**：内部格式为线性色彩空间（未应用Gamma）

---

## 3. Gamma转换机制 (MoviePipelineImageQuantization)

### 文件位置
`H:\UE_5.6\Engine\Plugins\MovieScene\MovieRenderPipeline\Source\MovieRenderPipelineCore\Private\MoviePipelineImageQuantization.cpp`

### 3.1 sRGB编码表生成

```cpp
static TArray<uint8> GenerateSRGBTable(uint32 InPrecision)
{
    TArray<uint8> OutsRGBTable;
    OutsRGBTable.SetNumUninitialized(InPrecision);

    for (int32 TableIndex = 0; TableIndex < OutsRGBTable.Num(); TableIndex++)
    {
        float ValueAsLinear = (float)TableIndex / (OutsRGBTable.Num() - 1);

        if (ValueAsLinear <= 0.0031308f)
        {
            ValueAsLinear = ValueAsLinear * 12.92f;
        }
        else
        {
            ValueAsLinear = FMath::Pow(ValueAsLinear, 1.0f / 2.4f) * 1.055f - 0.055f;
        }

        OutsRGBTable.GetData()[TableIndex] = (uint8)(ValueAsLinear * 255.f + 0.5f);
    }
    return OutsRGBTable;
}
```

**sRGB公式**：
```
线性值 ≤ 0.0031308: sRGB = 12.92 × 线性值
线性值 > 0.0031308: sRGB = 1.055 × 线性值^(1/2.4) - 0.055
```

### 3.2 Float16到8bit的转换

```cpp
static TArray<float> GenerateSRGBTableFloat16toFloat()
{
    TArray<float> OutsRGBTable;
    OutsRGBTable.SetNumUninitialized(65536);

    FFloat16 OnePointZero = 1.0f;
    for (int32 TableIndex = OnePointZero.Encoded; TableIndex < 32768; TableIndex++)
    {
        OutsRGBTableData[TableIndex] = 255;
    }
    for (int32 TableIndex = 32768; TableIndex < 65536; TableIndex++)
    {
        OutsRGBTableData[TableIndex] = 0;
    }
    for (int32 TableIndex = 0; TableIndex < OnePointZero.Encoded; TableIndex++)
    {
        FFloat16 Value;
        Value.Encoded = TableIndex;
        float ValueAsLinear = (float)Value;

        if (ValueAsLinear <= 0.0031308f)
            ValueAsLinear = ValueAsLinear * 12.92f;
        else
            ValueAsLinear = FMath::Pow(ValueAsLinear, 1.0f / 2.4f) * 1.055f - 0.055f;

        OutsRGBTableData[TableIndex] = (ValueAsLinear * 255.f);
    }
    return OutsRGBTable;
}
```

### 3.3 抖动降噪（Dithering）

```cpp
static TArray<FColor> ConvertLinearTosRGB8bppViaLookupTable(FFloat16Color* InColor, const int64 InCount)
{
    TArray<float> sRGBTable = GenerateSRGBTableFloat16toFloat();
    FRandomStream RandStream(31337);

    TArray<float> RandStreamTable;
    int32 TableSize = (1024 * 1024) + 7;
    RandStreamTable.SetNumUninitialized(TableSize);
    for (int32 Index = 0; Index < TableSize; Index++)
    {
        float Value = RandStream.GetFraction();
        *(uint32*)&Value &= 0xFFFFFF00U;
        RandStreamTable[Index] = Value;
    }

    ParallelFor(Loops, [&](int32 LoopIndex)
    {
        for (int64 PixelIndex = Start; PixelIndex < End; PixelIndex++)
        {
            OutColor->R = (uint8)FMath::FloorToInt(
                sRGBTableData[InColor[PixelIndex].R.Encoded] +
                RandStreamTable[(PixelIndex + 0) % TableSize]
            );
            OutColor->G = (uint8)FMath::FloorToInt(
                sRGBTableData[InColor[PixelIndex].G.Encoded] +
                RandStreamTable[(PixelIndex + 1) % TableSize]
            );
            OutColor->B = (uint8)FMath::FloorToInt(
                sRGBTableData[InColor[PixelIndex].B.Encoded] +
                RandStreamTable[(PixelIndex + 2) % TableSize]
            );

            OutColor->A = (uint8)FMath::Clamp(
                FMath::FloorToInt((InColor[PixelIndex].A * 255.f) +
                RandStreamTable[(PixelIndex + 3) % TableSize]),
                0, 255
            );
        }
    });

    return OutsRGBData;
}
```

**抖动原理**：
- 添加[0,1)范围的随机值到sRGB转换后的值
- 防止量化伪迹（banding）
- Alpha通道保持线性（不受sRGB编码影响）

---

## 4. Tone Curve与OCIO控制 (ColorSetting)

### 文件位置
`H:\UE_5.6\Engine\Plugins\MovieScene\MovieRenderPipeline\Source\MovieRenderPipelineCore\Public\MoviePipelineColorSetting.h`

### 4.1 ColorSetting定义

```cpp
UCLASS(Blueprintable)
class MOVIERENDERPIPELINECORE_API UMoviePipelineColorSetting : public UMoviePipelineSetting
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "Misc")
    FOpenColorIODisplayConfiguration OCIOConfiguration;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "Misc")
    bool bDisableToneCurve;
};
```

### 4.2 编辑时逻辑

```cpp
#if WITH_EDITOR
bool UMoviePipelineColorSetting::CanEditChange(const FProperty* InProperty) const
{
    if (InProperty->GetName() == GET_MEMBER_NAME_STRING_CHECKED(
        UMoviePipelineColorSetting, bDisableToneCurve))
    {
        return !OCIOConfiguration.bIsEnabled;
    }
    return Super::CanEditChange(InProperty);
}
#endif
```

**互斥关系**：
```
OCIO启用 → Tone Curve强制禁用
OCIO禁用 → Tone Curve可选启用/禁用
```

---

## 5. 输出处理流程 (ImageSequenceOutput)

### 文件位置
`H:\UE_5.6\Engine\Plugins\MovieScene\MovieRenderPipeline\Source\MovieRenderPipelineRenderPasses\Private\MoviePipelineImageSequenceOutput.cpp`

### 5.1 输出Pass流程

```cpp
void UMoviePipelineImageSequenceOutputBase::OnReceiveImageDataImpl(
    FMoviePipelineMergerOutputFrame* InMergedOutputFrame)
{
    UMoviePipelineColorSetting* ColorSetting =
        GetPipeline()->GetPipelinePrimaryConfig()->FindSetting<UMoviePipelineColorSetting>();

    for (TPair<FMoviePipelinePassIdentifier, TUniquePtr<FImagePixelData>>& RenderPassData :
         InMergedOutputFrame->ImageOutputData)
    {
        switch (PreferredOutputFormat)
        {
            case EImageFormat::PNG:
            case EImageFormat::JPEG:
            case EImageFormat::BMP:
            {
                const bool bApplysRGB =
                    !(ColorSetting && ColorSetting->OCIOConfiguration.bIsEnabled);

                TileImageTask->PixelPreProcessors.Add(
                    UE::MoviePipeline::FAsyncImageQuantization(TileImageTask.Get(), bApplysRGB)
                );

                QuantizedPixelType = EImagePixelType::Color;
                break;
            }

            case EImageFormat::EXR:
                break;
        }

        if (RenderPassData.Key.Name == TEXT("FinalImage"))
        {
            for (const MoviePipeline::FCompositePassInfo& CompositePass : CompositedPasses)
            {
                TileImageTask->PixelPreProcessors.Add(
                    TAsyncCompositeImage<FColor>(MoveTemp(PixelData), CompositeOffset)
                );
            }
        }

        if (!IsAlphaAllowed() && !Payload->bRequireTransparentOutput)
        {
            TileImageTask->AddPreProcessorToSetAlphaOpaque();
        }

        GetPipeline()->AddOutputFuture(
            ImageWriteQueue->Enqueue(MoveTemp(TileImageTask)),
            OutputData
        );
    }
}
```

### 5.2 量化处理器

```cpp
namespace UE { namespace MoviePipeline {
    FAsyncImageQuantization::FAsyncImageQuantization(
        FImageWriteTask* InWriteTask,
        const bool bInConvertToSRGB)
        : ParentWriteTask(InWriteTask)
        , bConvertToSRGB(bInConvertToSRGB)
    {
    }

    void FAsyncImageQuantization::operator()(FImagePixelData* PixelData)
    {
        TUniquePtr<FImagePixelData> QuantizedPixelData =
            QuantizeImagePixelDataToBitDepth(
                PixelData,
                8,
                nullptr,
                bConvertToSRGB
            );
        ParentWriteTask->PixelData = MoveTemp(QuantizedPixelData);
    }
}}
```

---

## 6. 渲染Pass与色彩空间 (DeferredPass)

### 文件位置
`H:\UE_5.6\Engine\Plugins\MovieScene\MovieRenderPipeline\Source\MovieRenderPipelineRenderPasses\Public\MoviePipelineDeferredPasses.h`

### 6.1 RenderPass分类

```cpp
void UMoviePipelineDeferredPassBase::MoviePipelineRenderShowFlagOverride(
    FEngineShowFlags& OutShowFlag)
{
    if (bDisableMultisampleEffects)
    {
        OutShowFlag.SetAntiAliasing(false);
        OutShowFlag.SetDepthOfField(false);
        OutShowFlag.SetMotionBlur(false);
        OutShowFlag.SetBloom(false);
        OutShowFlag.SetSceneColorFringe(false);
    }
}
```

### 6.2 预设Pass

| Pass名称 | 显示模式 | 输出色彩空间 | 用途 |
|---------|---------|-----------|------|
| FinalImage | VMI_Lit | 线性(内部) | 主渲染结果 |
| Unlit | VMI_Unlit | 线性无光照 | 基础色 |
| DetailLighting | VMI_Lit_DetailLighting | 仅细节光 | 光照分解 |
| LightingOnly | VMI_LightingOnly | 仅间接光 | 光照隔离 |
| ReflectionsOnly | VMI_ReflectionOverride | 仅镜像反射 | 反射通道 |
| PathTracer | VMI_PathTracing | 物理渲染 | 高精度全局光 |

**所有Pass都使用线性色彩空间进行计算**

---

## 7. CaptureSource对比分析

### 文件位置
`H:\UE_5.6\Engine\Source\Runtime\Engine\Classes\Engine\EngineTypes.h`

### 7.1 ESceneCaptureSource枚举

```cpp
enum ESceneCaptureSource : int
{
    SCS_SceneColorHDR = 0,
    SCS_SceneColorHDRNoAlpha,
    SCS_FinalColorHDR,
    SCS_FinalToneCurveHDR,
    SCS_FinalColorLDR,
    SCS_SceneColorSceneDepth,
    SCS_SceneDepth,
    SCS_DeviceDepth,
    SCS_Normal,
    SCS_BaseColor,
};
```

### 7.2 SceneCaptureComponent2D vs MovieRenderPipeline

| 维度 | SceneCaptureComponent2D | MovieRenderPipeline |
|-----|----------------------|-------------------|
| **CaptureSource** | 灵活，支持所有ESceneCaptureSource | 固定为SceneColorHDR (内部处理) |
| **TextureRenderTarget2D** | 用户指定，可能线性也可能sRGB | 强制线性格式 (RTF_RGBA16F 或 RTF_RGBA32F) |
| **Gamma应用** | 取决于RenderTarget格式 + ShowFlags | 显式在ImageSequenceOutput中控制 |
| **Tone Curve** | 通过PostProcessSettings + ShowFlags | 通过ColorSetting统一管理 |
| **OCIO支持** | 无直接支持 | 通过FOpenColorIODisplayExtension集成 |
| **异步处理** | 同步（本帧返回） | 异步（GPU异步读回 + 线程文件I/O） |
| **精度** | 8bit或16bit（RenderTarget) | 16bit中间 → 8bit/32bit最终 |
| **多Pass** | 不支持 | 支持6+种预设Pass + 自定义PostProcess |

---

## 8. 完整的Gamma流程链

### 8.1 数据流向图

```
[Deferred Rendering] (Linear 线性空间)
         ↓
[TextureRenderTarget2D] (Linear Float16/Float32)
         ↓
[FMoviePipelineSurfaceReader::ResolveSampleToReadbackTexture_RenderThread]
  ├─ GPU → CPU (FRHIGPUTextureReadback)
  └─ Point采样保留精度
         ↓
[FMoviePipelineImageSequenceOutput::OnReceiveImageDataImpl]
  ├─ 检查ColorSetting.OCIOConfiguration.bIsEnabled
  ├─ 检查ColorSetting.bDisableToneCurve
  └─ 根据OutputFormat决定量化方式
         ↓
[UE::MoviePipeline::FAsyncImageQuantization]
  ├─ 如果OCIO未启用且OutputFormat是PNG/JPG/BMP:
  │  └─ 应用sRGB编码 (查表 + 抖动)
  └─ 如果EXR格式:
     └─ 保持16bit浮点
         ↓
[ImageWriteQueue::Enqueue]
  └─ 异步写入PNG/JPG/BMP/EXR文件
```

### 8.2 关键决策点

```cpp
if (ColorSetting.OCIOConfiguration.bIsEnabled) {
    apply_OCIO_transform();
    tone_curve_forced_disabled = true;
} else {
    if (ColorSetting.bDisableToneCurve == false) {
        apply_filmic_tone_curve();
    }
    if (output_format in [PNG, JPG, BMP]) {
        apply_sRGB_encoding();
    } else if (output_format == EXR) {
        keep_linear_HDR();
    }
}
```

---

## 9. 特殊考虑

### 9.1 Post-Process Materials 的色彩空间

```cpp
USTRUCT(BlueprintType)
struct MOVIERENDERPIPELINERENDERPASSES_API FMoviePipelinePostProcessPass
{
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
    bool bEnabled = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
    TSoftObjectPtr<UMaterialInterface> Material;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings",
              DisplayName="Use High Precision (32-bit) Output")
    bool bHighPrecisionOutput = false;
};
```

**重点**：
- Post-process materials在Tonemapping后运行
- 输出已经是LDR（如果使用默认Tonemapper）
- bHighPrecisionOutput只影响中间纹理精度，最终输出仍受OutputFormat限制

### 9.2 Alpha通道处理

从量化代码可见：
```cpp
OutColor->A = (uint8)FMath::Clamp(
    FMath::FloorToInt((InColor[PixelIndex].A * 255.f) + RandomDither),
    0, 255
);
```

### 9.3 EXR的特殊性

```cpp
case EImageFormat::EXR:
    break;
```

EXR保持：
- 32bit浮点（或16bit可选）
- 线性色彩空间
- 完整HDR范围
- 无sRGB编码

---

## 10. 建议与最佳实践

### 10.1 针对UnrealCV的优化方向

1. **保持线性管道**
   ```cpp
   FString RenderTargetFormat = TEXT("RTF_RGBA32F");
   ```

2. **Gamma输出时机**
   ```cpp
   // 在ImageSequenceOutput中统一处理
   ```

3. **OCIO集成选项**
   ```cpp
   FOpenColorIODisplayConfiguration OCIO;
   OCIO.bIsEnabled = bUseOCIO;
   ```

### 10.2 PNG vs EXR选择

```
PNG (8bit):
  ✓ 压缩率高
  ✓ 广泛兼容
  ✗ 精度低
  ✗ 必须sRGB编码

EXR (16/32bit):
  ✓ 保留HDR数据
  ✓ 无损或可控损失
  ✓ 线性保存
  ✗ 文件大
  ✗ 处理工具较少
```

---

## 11. 源代码位置总结

| 功能模块 | 文件路径 | 关键函数 |
|---------|---------|---------|
| GPU读取 | MoviePipelineSurfaceReader.cpp | `ResolveSampleToReadbackTexture_RenderThread` |
| sRGB编码 | MoviePipelineImageQuantization.cpp | `GenerateSRGBTable`, `ConvertLinearTosRGB8bpp` |
| 输出处理 | MoviePipelineImageSequenceOutput.cpp | `OnReceiveImageDataImpl` |
| 色彩设置 | MoviePipelineColorSetting.h/.cpp | `OCIOConfiguration`, `bDisableToneCurve` |
| 渲染Pass | MoviePipelineDeferredPasses.h | `MoviePipelineRenderShowFlagOverride` |
| Capture对比 | EngineTypes.h / SceneCaptureComponent*.h | `ESceneCaptureSource` 枚举 |

---

## 总结

**MovieRenderPipeline的Gamma管理是一个多层次、可配置的系统**：

1. **GPU层**：渲染使用线性工作流（浮点精度）
2. **读取层**：无损GPU→CPU传输（FRHIGPUTextureReadback）
3. **量化层**：根据ColorSetting决定是否应用sRGB
4. **输出层**：不同格式使用不同编码策略（8bit=sRGB, 16bit/32bit=Linear)
5. **高级控制**：OCIO集成用于专业色彩管理

与SceneCaptureComponent2D的主要区别在于**MRP提供了完整的管道控制和异步处理能力**，适合高质量数据集输出和电影级渲染。
