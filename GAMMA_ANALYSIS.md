# UE 5.6 Gamma值处理机制详细分析

## 一、概述

本文档详细分析了UE5.6引擎中gamma值的处理方式，重点关注SceneCapture组件、TextureRenderTarget以及色彩空间转换的实现。

---

## 二、核心Gamma控制机制

### 2.1 TextureRenderTarget2D中的Gamma参数

#### **bForceLinearGamma 属性**
位置：`TextureRenderTarget2D.h` 第127-129行
```cpp
/** True to force linear gamma space for this render target */
UPROPERTY()
uint8 bForceLinearGamma:1;
```

**含义**：
- `true` = 强制使用线性gamma（gamma = 1.0），即色彩值直接存储为线性空间
- `false` = 允许使用sRGB gamma（gamma = 2.2），色彩值按sRGB曲线编码存储

**默认值**：
- 在 `TextureRenderTarget2D::UTextureRenderTarget2D()` 中默认为 `true`（第47行）
- 但当用户选择 `RTF_RGBA8_SRGB` 格式时，自动设置为 `false`（第252-259行）

#### **InitCustomFormat 函数中的 bInForceLinearGamma 参数**

位置：`TextureRenderTarget2D.h` 第183行
```cpp
ENGINE_API void InitCustomFormat(uint32 InSizeX, uint32 InSizeY, EPixelFormat InOverrideFormat, bool bInForceLinearGamma);
```

**参数解释**：
- `InSizeX, InSizeY` - 渲染目标分辨率
- `InOverrideFormat` - 像素格式（PF_B8G8R8A8等）
- `bInForceLinearGamma` - 是否强制线性gamma空间

**实现细节**（`TextureRenderTarget2D.cpp` 第135-158行）：
```cpp
void UTextureRenderTarget2D::InitCustomFormat( uint32 InSizeX, uint32 InSizeY, EPixelFormat InOverrideFormat, bool bInForceLinearGamma )
{
    check(InSizeX > 0 && InSizeY > 0);
    check(FTextureRenderTargetResource::IsSupportedFormat(InOverrideFormat));

    SizeX = InSizeX;
    SizeY = InSizeY;
    OverrideFormat = InOverrideFormat;
    bForceLinearGamma = bInForceLinearGamma;  // 直接赋值

    // 验证尺寸范围
    if (!ensureMsgf(SizeX >= 0 && SizeX <= 65536, ...))
        SizeX = 1;
    if (!ensureMsgf(SizeY >= 0 && SizeY <= 65536, ...))
        SizeY = 1;

    UpdateResource();  // 重新创建资源
}
```

**关键点**：
1. 该函数直接设置 `bForceLinearGamma` 标志
2. 调用 `UpdateResource()` 重新创建RHI纹理资源
3. 如果需要线性空间，应设置 `bInForceLinearGamma = true`
4. 如果需要sRGB空间，应设置 `bInForceLinearGamma = false`

---

### 2.2 IsSRGB() 方法的实现

位置：`TextureRenderTarget2D.cpp` 第71-86行
```cpp
bool UTextureRenderTarget2D::IsSRGB() const
{
    // 当使用OverrideFormat时的逻辑
    if (OverrideFormat == PF_Unknown)
    {
        // 仅当RenderTargetFormat == RTF_RGBA8_SRGB时返回true
        return RenderTargetFormat == RTF_RGBA8_SRGB;
    }
    else
    {
        // 与bForceLinearGamma相反：bForceLinearGamma=true => IsSRGB()=false
        return !bForceLinearGamma;
    }
}
```

**逻辑关系**：
- `bForceLinearGamma = true` → `IsSRGB() = false` → 线性色彩空间
- `bForceLinearGamma = false` → `IsSRGB() = true` → sRGB色彩空间

---

### 2.3 GetDisplayGamma() 方法

位置：`TextureRenderTarget2D.cpp` 第703-721行
```cpp
float UTextureRenderTarget2D::GetDisplayGamma() const
{
    // 如果TargetGamma被显式设置，优先使用
    if (TargetGamma > UE_KINDA_SMALL_NUMBER * 10.0f)
    {
        return TargetGamma;
    }

    // 特殊处理浮点格式和线性gamma
    EPixelFormat Format = GetFormat();
    if (Format == PF_FloatRGB || Format == PF_FloatRGBA || bForceLinearGamma)
    {
        return 1.0f;  // 线性gamma = 1.0
    }

    // 默认返回sRGB gamma = 2.2
    return UTextureRenderTarget::GetDefaultDisplayGamma(); // hard-coded 2.2
}
```

**Gamma值对应关系**：
- `GetDisplayGamma() = 1.0f` → 线性色彩空间，无gamma校正
- `GetDisplayGamma() = 2.2f` → sRGB色彩空间，应用gamma校正

---

## 三、ESceneCaptureSource 与 FinalColorLDR

### 3.1 ESceneCaptureSource 枚举定义

位置：`EngineTypes.h` 第527-536行
```cpp
enum ESceneCaptureSource : int
{
    SCS_SceneColorHDR,           // RGB=SceneColor(HDR), A=InvOpacity
    SCS_SceneColorHDRNoAlpha,    // RGB=SceneColor(HDR), A=0
    SCS_FinalColorLDR,           // RGB=FinalColor(LDR) - 经过Tonemap + Gamma校正
    SCS_SceneColorSceneDepth,    // RGB=SceneColor(HDR), A=SceneDepth
    SCS_SceneDepth,              // R=SceneDepth
    SCS_DeviceDepth,             // RGB=DeviceDepth
    SCS_Normal,                  // RGB=Normal(Deferred only)
    SCS_BaseColor,               // RGB=BaseColor(Deferred only)
    SCS_FinalColorHDR,           // RGB=FinalColor(HDR) - 线性色彩空间
    SCS_FinalToneCurveHDR,       // RGB=FinalColor(ToneCurve) - sRGB gamut

    SCS_MAX
};
```

### 3.2 FinalColorLDR 的Gamma特性

**定义**（SceneView.h 第2328行注释）：
```
If SCS_FinalColorLDR this indicates do nothing.
```

**含义解释**：
1. `FinalColorLDR` 是**LDR（Low Dynamic Range）格式**，范围 [0, 1]
2. 数据已经过**Tonemap**处理（将HDR转换为LDR）
3. 数据已经过**Gamma校正**（应用inverse sRGB曲线进行编码）
4. 最终存储为sRGB编码的色彩值

**渲染流程**：
```
HDR SceneColor (线性)
    ↓ Tonemap (色调映射)
    ↓ Gamma Encoding (sRGB编码：linear^(1/2.2))
    ↓ LDR Output [0, 1] sRGB编码
```

### 3.3 CaptureNeedsSceneColor() 函数

位置：`SceneCaptureRendering.cpp` 第213-216行
```cpp
static bool CaptureNeedsSceneColor(ESceneCaptureSource CaptureSource)
{
    return CaptureSource != SCS_FinalColorLDR &&
           CaptureSource != SCS_FinalColorHDR &&
           CaptureSource != SCS_FinalToneCurveHDR;
}
```

**含义**：
- FinalColor系列（LDR/HDR/ToneCurve）**不需要**从SceneColor缓冲区复制
- 这三种模式直接从Tonemap/PostProcess输出
- 其他模式（SceneColorHDR、Depth、Normal等）需要从场景缓冲区读取

---

## 四、Gamma转换在ReadPixels中的处理

### 4.1 FReadSurfaceDataFlags 类结构

位置：`RHITypes.h` 第15-123行
```cpp
class FReadSurfaceDataFlags
{
public:
    FReadSurfaceDataFlags(ERangeCompressionMode InCompressionMode = RCM_UNorm,
                         ECubeFace InCubeFace = CubeFace_MAX)
        : CubeFace(InCubeFace), CompressionMode(InCompressionMode)
    {}

    // Gamma转换标志
    void SetLinearToGamma(bool Value)
    {
        bLinearToGamma = Value;
    }

    bool GetLinearToGamma() const
    {
        return bLinearToGamma;
    }

private:
    bool bLinearToGamma = true;  // 默认为true
    // ... 其他成员
};
```

### 4.2 SetLinearToGamma 的含义

**参数说明**：
- `true` = 将线性色彩值转换为gamma编码（线性 → sRGB）
  - 应用公式：`output = linear^(1/2.2)`
  - 用于从浮点格式读取数据时进行色彩空间转换

- `false` = 不进行gamma转换，原样返回色彩值
  - 保持原始色彩空间
  - 用于深度、法线等非色彩数据

### 4.3 ImageUtils 中的Gamma处理

位置：`ImageUtils.cpp` 第550-557行
```cpp
EGammaSpace GammaSpace = TexRT->IsSRGB() ? EGammaSpace::sRGB : EGammaSpace::Linear;

OutImage.Init(RectSizeX, RectSizeY, NumSlices, ERawImageFormat::BGRA8, GammaSpace);

// "LinearToGamma" is basically moot; that would only be used if we were reading
// float pixels to FColor but in that case the ReadFormat should have been float,
// so we won't be here. Gamma conversion will be handled by FImage after the pixel
// read, not inside RHI
ReadFlags.SetLinearToGamma(GammaSpace == EGammaSpace::sRGB);
```

**处理逻辑**：
1. 检查TextureRenderTarget的 `IsSRGB()` 状态
2. 根据状态设置 `FImage` 的GammaSpace
3. `SetLinearToGamma()` 标志通知RHI如何处理色彩转换
4. **实际的Gamma转换由FImage类处理，而非RHI层**

### 4.4 TextureRenderTarget 中的ReadPixels调用

位置：`TextureRenderTarget.cpp` 第264-274行
```cpp
FReadSurfaceDataFlags ReadSurfaceDataFlags(RCM_UNorm, bIsCube ? (ECubeFace)(SurfaceIndex % 6) : CubeFace_MAX);
ReadSurfaceDataFlags.SetArrayIndex(bIsCube ? SurfaceIndex / 6 : SurfaceIndex);

void * ReadIntoSlice = ReadImage.GetPixelPointer(0, 0, SurfaceIndex);

switch (ReadFormat)
{
case ERawImageFormat::BGRA8:  // FColor格式
{
    TArray<FColor> NewDataColor;
    RenderTarget->ReadPixels(NewDataColor, ReadSurfaceDataFlags);  // 读取FColor像素
    // ... 数据处理
}
```

**流程**：
1. 为每个表面/切片创建读取标志
2. 根据格式选择读取路径（FColor/FLinearColor/FFloat16Color）
3. RHI执行像素读取
4. FImage处理Gamma转换（基于其初始化时指定的GammaSpace）

---

## 五、ETextureRenderTargetFormat 与 SRGB

### 5.1 Format枚举与Gamma的关系

位置：`TextureRenderTarget2D.h` 第19-44行
```cpp
enum ETextureRenderTargetFormat : int
{
    RTF_R8,              // 8-bit, 线性
    RTF_RG8,             // 8-bit, 线性
    RTF_RGBA8,           // 8-bit, 线性 - bForceLinearGamma=true
    RTF_RGBA8_SRGB,      // 8-bit, sRGB编码 - bForceLinearGamma=false（自动设置）
    RTF_R16f,            // 16-bit float, 线性
    RTF_RG16f,           // 16-bit float, 线性
    RTF_RGBA16f,         // 16-bit float, 线性
    RTF_R32f,            // 32-bit float, 线性
    RTF_RG32f,           // 32-bit float, 线性
    RTF_RGBA32f,         // 32-bit float, 线性
    RTF_RGB10A2          // 10-bit packed, 线性
};
```

### 5.2 PostEditChangeProperty 中的自动转换

位置：`TextureRenderTarget2D.cpp` 第250-260行
```cpp
if (PropertyName == GET_MEMBER_NAME_CHECKED(UTextureRenderTarget2D, RenderTargetFormat))
{
    if (RenderTargetFormat == RTF_RGBA8_SRGB)
    {
        bForceLinearGamma = false;  // sRGB格式→关闭线性gamma
    }
    else
    {
        bForceLinearGamma = true;   // 其他格式→启用线性gamma
    }
}
```

**自动关系**：
- 选择 `RTF_RGBA8_SRGB` → `bForceLinearGamma = false`
- 选择其他格式（RTF_RGBA8、RTF_RGBA16f等） → `bForceLinearGamma = true`

---

## 六、SceneCaptureComponent2D 的Gamma处理

### 6.1 TextureTarget 属性

位置：`SceneCaptureComponent2D.h` 第78-80行
```cpp
/** Output render target of the scene capture that can be read in materials. */
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=SceneCapture)
TObjectPtr<class UTextureRenderTarget2D> TextureTarget;
```

**Gamma流程**：
1. 当 `CaptureSource = SCS_FinalColorLDR` 时：
   - 渲染管线输出sRGB编码的LDR色彩
   - 存储到TextureTarget

2. TextureTarget的Gamma设置决定存储方式：
   - 如果 `bForceLinearGamma = true`：存储为线性
   - 如果 `bForceLinearGamma = false` (RTF_RGBA8_SRGB)：存储为sRGB

### 6.2 CaptureSource 的Gamma含义

| CaptureSource | 色彩空间 | Gamma值 | 说明 |
|---|---|---|---|
| SCS_SceneColorHDR | 线性 | 1.0 | 原始HDR场景色，未经Tonemap |
| SCS_SceneColorHDRNoAlpha | 线性 | 1.0 | 同上，Alpha=0 |
| SCS_FinalColorLDR | sRGB编码 | ~2.2 | 经Tonemap + Gamma编码，LDR范围 |
| SCS_FinalColorHDR | 线性 | 1.0 | 经Tonemap，线性HDR色彩空间 |
| SCS_FinalToneCurveHDR | sRGB gamut | ~2.2 | 经Tonemap + ToneCurve，sRGB色域 |
| SCS_SceneDepth | N/A | N/A | 深度数据，无色彩空间 |
| SCS_DeviceDepth | N/A | N/A | 设备深度，无色彩空间 |
| SCS_Normal | 线性 | 1.0 | 法线向量，无Gamma |
| SCS_BaseColor | 线性 | 1.0 | 基础色，线性空间 |

---

## 七、Gamma校正总结表

### 7.1 存储时的Gamma关系

```
TextureTarget配置          | 存储Gamma | ReadPixels中SetLinearToGamma | 说明
bForceLinearGamma=true     | 1.0      | true/false均可             | 存储为线性值
RTF_RGBA8_SRGB或          | 2.2      | true（sRGB转换）           | 存储为sRGB编码
bForceLinearGamma=false    |          |                            |
```

### 7.2 渲染时的Gamma流程（FinalColorLDR）

```
1. 场景渲染 (线性色彩空间)
   ↓
2. Tonemap (色调映射: [0,∞) → [0,1])
   ↓
3. Gamma编码 (sRGB: linear^(1/2.2))
   ↓
4. 写入TextureTarget
   ├─ 如果TextureTarget.IsSRGB()=true → 硬件处理sRGB存储
   └─ 如果TextureTarget.IsSRGB()=false → 原样存储预编码值
   ↓
5. ReadPixels读取
   ├─ 如果TextureTarget.bForceLinearGamma=true → 返回线性值
   └─ 如果TextureTarget.bForceLinearGamma=false → 返回sRGB值
```

---

## 八、常见配置方案

### 8.1 配置方案A：FinalColorLDR → 线性输出

**目标**：获取sRGB编码的LDR色彩值

**配置**：
```cpp
// TextureTarget设置
TextureTarget->InitCustomFormat(Width, Height, PF_B8G8R8A8, true);
// 或选择RTF_RGBA8格式
TextureTarget->RenderTargetFormat = RTF_RGBA8;

// ReadPixels
FReadSurfaceDataFlags ReadFlags;
ReadFlags.SetLinearToGamma(true);  // 应用sRGB→Linear转换
TArray<FColor> OutData;
TextureTarget->ReadPixels(OutData, ReadFlags);
```

**结果**：
- TextureTarget存储：sRGB编码值（Gamma=2.2）
- ReadPixels输出：线性转换后的值（Gamma=1.0）

### 8.2 配置方案B：FinalColorLDR → sRGB输出

**目标**：获取直接的sRGB编码值

**配置**：
```cpp
// TextureTarget设置为RTF_RGBA8_SRGB
TextureTarget->RenderTargetFormat = RTF_RGBA8_SRGB;
// 此时bForceLinearGamma会自动设置为false

// ReadPixels
FReadSurfaceDataFlags ReadFlags;
ReadFlags.SetLinearToGamma(false);  // 不进行转换
TArray<FColor> OutData;
TextureTarget->ReadPixels(OutData, ReadFlags);
```

**结果**：
- TextureTarget存储：sRGB编码值
- ReadPixels输出：sRGB编码值（原样返回）

### 8.3 配置方案C：SceneColorHDR → 线性输出

**目标**：获取原始线性HDR色彩

**配置**：
```cpp
// 设置CaptureSource
SceneCapture->CaptureSource = ESceneCaptureSource::SCS_SceneColorHDR;

// TextureTarget设置为浮点格式（保留范围）
TextureTarget->InitCustomFormat(Width, Height, PF_FloatRGBA, true);

// ReadPixels
FReadSurfaceDataFlags ReadFlags;
ReadFlags.SetLinearToGamma(false);  // 浮点数无需Gamma转换
TArray<FLinearColor> OutData;
TextureTarget->ReadLinearColorPixels(OutData, ReadFlags);
```

**结果**：
- 保留完整的HDR范围
- 线性色彩空间（无Gamma编码）

---

## 九、重要细节与注意事项

### 9.1 bForceLinearGamma 的实际含义

**常见误解**：`bForceLinearGamma=true` 表示"强制应用线性Gamma"

**正确含义**：`bForceLinearGamma=true` 表示"强制使用线性色彩空间存储"
- 即：不应用sRGB Gamma编码
- Gamma值 = 1.0（线性）
- 色彩值直接为线性空间值

### 9.2 FinalColorLDR 的自动Gamma处理

**重点**：`SCS_FinalColorLDR` 源已经包含Gamma编码
- 渲染管线在PostProcess阶段自动应用
- 无需手动在ReadPixels中再次应用
- 直接读取即可获得sRGB编码值

### 9.3 GetDisplayGamma() 的返回值

**值的含义**：
- `1.0f` = 线性色彩空间，RHI/硬件不应用Gamma校正
- `2.2f` = sRGB色彩空间，RHI/硬件应用sRGB Gamma转换
  - 对应公式：`stored = linear^(1/2.2)` 和 `linear = stored^2.2`

### 9.4 ReadPixels 中 SetLinearToGamma() 的作用

**场景1**：读取sRGB格式 (PF_B8G8R8A8) 为FColor
```cpp
ReadFlags.SetLinearToGamma(true);
// RHI会在读取时应用逆sRGB转换
// FColor值 = sRGB编码值的线性转换
```

**场景2**：读取浮点格式 (PF_FloatRGBA)
```cpp
ReadFlags.SetLinearToGamma(false);  // 推荐
// RHI直接返回浮点值，无转换
// 浮点值通常为线性空间
```

### 9.5 IsSRGB() 与 bForceLinearGamma 的对偶关系

```cpp
IsSRGB() == !bForceLinearGamma  // 当使用OverrideFormat时
```

- `IsSRGB() = true` → TextureTarget将创建sRGB硬件格式
- `bForceLinearGamma = true` → GetDisplayGamma()返回1.0
- 两者工作在不同层级，需要一致性配置

---

## 十、调试与验证方法

### 10.1 验证TextureTarget的Gamma设置

```cpp
UTextureRenderTarget2D* RT = ..;

// 检查当前Gamma配置
float Gamma = RT->GetDisplayGamma();
bool bIsSRGB = RT->IsSRGB();
bool bLinearGamma = RT->bForceLinearGamma;

UE_LOG(LogYourCategory, Warning, TEXT("RT Gamma=%.1f, IsSRGB=%d, bLinearGamma=%d"),
    Gamma, bIsSRGB, bLinearGamma);
```

### 10.2 验证ReadPixels行为

```cpp
FReadSurfaceDataFlags Flags;
Flags.SetLinearToGamma(true);

TArray<FColor> Colors;
if (RenderTarget->ReadPixels(Colors, Flags))
{
    // 检查色彩范围
    if (Colors[0].R == 128)  // 近似0.5的sRGB编码
    {
        UE_LOG(LogYourCategory, Warning, TEXT("Got sRGB encoded value"));
    }
}
```

### 10.3 监控PostProcess中的Gamma应用

编辑器中搜索：
- `PostProcessTonemap.cpp` - Tonemap着色器应用
- `PostProcessCombineLUTs.cpp` - LUT色彩分级
- `PostProcessDeviceEncodingOnly.cpp` - 设备编码

查看 `SceneCaptureSource` 值以理解当前处理路径。

---

## 十一、参考代码位置总览

| 功能 | 文件 | 行号 |
|---|---|---|
| bForceLinearGamma定义 | TextureRenderTarget2D.h | 127-129 |
| InitCustomFormat实现 | TextureRenderTarget2D.cpp | 135-158 |
| GetDisplayGamma实现 | TextureRenderTarget2D.cpp | 703-721 |
| IsSRGB实现 | TextureRenderTarget2D.cpp | 71-86 |
| FReadSurfaceDataFlags类 | RHITypes.h | 15-123 |
| SetLinearToGamma方法 | RHITypes.h | 42-50 |
| ESceneCaptureSource枚举 | EngineTypes.h | 527-536 |
| CaptureNeedsSceneColor函数 | SceneCaptureRendering.cpp | 213-216 |
| ReadPixels调用示例 | TextureRenderTarget.cpp | 264-274 |
| ImageUtils Gamma处理 | ImageUtils.cpp | 550-557 |

---

## 十二、结论

### 核心要点

1. **bForceLinearGamma=true** = 线性色彩空间，gamma=1.0，无sRGB编码
2. **bForceLinearGamma=false** = sRGB色彩空间，gamma~2.2，应用sRGB编码
3. **SCS_FinalColorLDR** 源的色彩值已经过Tonemap和Gamma编码（sRGB）
4. **ReadPixels中的SetLinearToGamma()** 影响RHI如何返回像素数据
5. **TextureTarget的IsSRGB()** 与 **bForceLinearGamma** 需要一致
6. **GetDisplayGamma()** 返回值(1.0或2.2)指导硬件如何处理存储

### 最佳实践

- 需要线性色彩→ `bForceLinearGamma=true` 或选择浮点格式
- 需要sRGB编码→ `bForceLinearGamma=false` 或选择 `RTF_RGBA8_SRGB`
- 使用FinalColorLDR→ 记住已包含Gamma编码，无需重复处理
- 读取浮点纹理→ `SetLinearToGamma(false)`
- 读取8位纹理→ `SetLinearToGamma(true)`（进行sRGB→Linear转换）
