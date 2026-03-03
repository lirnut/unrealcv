# MovieQualityRenderComponent vs MainViewport 渲染管线对比

**生成时间**: 2026-03-02
**源**: UE 5.6 Engine Source + UnrealCV Plugin

---

## 1. MQRC 渲染流程

### 入口函数

**文件**: `MovieQualityRenderComponent.cpp`

**调用链**:
```
UMovieQualityRenderComponent::ExecuteCaptureFrame() [line 436]
    │
    ├── CreateViewFamily(RenderTarget) [line 533]
    │       ├── FSceneViewFamilyContext construction
    │       ├── bIsMainViewFamily = false
    │       ├── bIsHDR = true (强制)
    │       ├── SceneCaptureSource, bResolveScene = true
    │
    ├── CreateSceneView(ViewFamily) [line 540]
    │       ├── FSceneView construction
    │       ├── FinalPostProcessSettings from cached main view
    │
    ├── ViewExtensions Setup/BeginRenderViewFamily [lines 547-551]
    │
    └── SubmitToRendererWithCallback() [line 554]
            │
            └── GetRendererModule().BeginRenderingViewFamily() [line 957]
```

### 关键代码片段

```cpp
// CreateViewFamily - line 603-664
TSharedPtr<FSceneViewFamilyContext> ViewFamily = MakeShared<FSceneViewFamilyContext>(
    FSceneViewFamily::ConstructionValues(
        RenderTargetResource,
        World->Scene,
        ShowFlags
    )
    .SetTime(FGameTime::CreateUndilated(World->GetTimeSeconds(), World->GetDeltaSeconds()))
    .SetRealtimeUpdate(true)
);

ViewFamily->bIsMainViewFamily = false;
ViewFamily->bAdditionalViewFamily = true;
ViewFamily->bIsHDR = true;  // CRITICAL: 强制启用 HDR
ViewFamily->EngineShowFlags.MotionBlur = false;  // 强制禁用
ViewFamily->SceneCaptureCompositeMode = ESceneCaptureCompositeMode::SCCM_Composite;
```

```cpp
// CreateSceneView - line 667-752
FSceneView* View = new FSceneView(ViewInitOptions);

View->bIsOfflineRender = true;
View->bForceCameraVisibilityReset = true;
View->AntiAliasingMethod = GlobalSettings.AntiAliasingMethod;

// 从主 View 缓存 PostProcessSettings
if (bHasCachedMainViewPostProcessSettings)
{
    View->FinalPostProcessSettings = CachedMainViewPostProcessSettings;
}
```

### PostProcess 缓存机制

**FMovieQualityViewExtension** (line 36-62):
```cpp
void FMovieQualityViewExtension::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
    if (InViewFamily.bIsMainViewFamily)
    {
        if (InViewFamily.Views.Num() > 0 && InViewFamily.Views[0] != nullptr)
        {
            // 缓存主 View 的 PostProcessSettings
            Component->CachedMainViewPostProcessSettings = InViewFamily.Views[0]->FinalPostProcessSettings;
            Component->LastMainViewportFrameNumber = InViewFamily.FrameNumber;
        }
    }
}
```

---

## 2. MainViewport 渲染流程

### 入口函数

**文件**: `UnrealClient.cpp`, `GameViewportClient.cpp`

**调用链**:
```
FViewport::Draw() [UnrealClient.cpp line 1725]
    │
    ├── EnqueueBeginRenderFrame() [line 1776]
    │
    ├── ViewportClient->Draw(this, &Canvas) [line 1814]
    │       │
    │       └── UGameViewportClient::Draw() [GameViewportClient.cpp line 1411]
    │               │
    │               ├── FSceneViewFamilyContext construction [line 1463]
    │               │       ├── bIsMainViewFamily = false (初始)
    │               │
    │               ├── ViewExtensions SetupViewFamily() [line 1484-1487]
    │               │
    │               ├── LocalPlayer->CalcSceneView() [line 1643]
    │               │
    │               ├── ViewFamily.bIsMainViewFamily = true [line 1896]
    │               │
    │               └── GetRendererModule().BeginRenderingViewFamily() [line 1903]
    │
    ├── Canvas.Flush_GameThread() [line 1816]
    │
    └── EnqueueEndRenderFrame() [line 1824]
```

### 关键代码片段

```cpp
// GameViewportClient.cpp - line 1463-1468
FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
    InViewport,
    MyWorld->Scene,
    EngineShowFlags)
    .SetRealtimeUpdate(true)
    .SetRequireMobileMultiView(bRequireMultiView));

// GameViewportClient.cpp - line 1893-1896
ViewFamily.bIsHDR = GetWindow().IsValid() ? GetWindow().Get()->GetIsHDR() : false;
ViewFamily.bIsMainViewFamily = true;  // 关键差异
```

---

## 3. 渲染管线对比

### 共同点

两者最终都使用相同的渲染管线:
1. `FRendererModule::BeginRenderingViewFamily()` → SceneRendering.cpp:4940
2. `FSceneRenderBuilder::CreateSceneRenderers()` → SceneRenderBuilder.cpp:984
3. `FSceneRenderProcessor::CreateSceneRenderers()` → SceneRenderBuilder.cpp:472
4. 根据 ShadingPath 创建 `FDeferredShadingSceneRenderer`

### 差异对比表

| 阶段 | MainViewport | MQRC |
|------|-------------|------|
| **ViewFamily.bIsMainViewFamily** | true | false |
| **ViewFamily.bIsHDR** | 取决于窗口HDR设置 | 强制 true |
| **EngineShowFlags.MotionBlur** | 取决于设置 | 强制 false |
| **View 创建方式** | LocalPlayer->CalcSceneView() | 手动 FSceneView 构造 |
| **PostProcessSettings** | 实时计算 | 从主View缓存 |
| **SceneCaptureCompositeMode** | 不适用 | SCCM_Composite |
| **ScreenPercentage** | 动态分辨率 | FLegacyScreenPercentageDriver |
| **bIsOfflineRender** | false | true |
| **bForceCameraVisibilityReset** | false | true |
| **Frame机制** | 有Begin/EndFrame | 无完整Frame机制 |

---

## 4. 对PostProcess各阶段的影响

### MQRC PostProcess 处理

#### 1. TAA (Temporal Anti-Aliasing)
- **影响**: 取决于 `GlobalSettings.AntiAliasingMethod`
- **代码**: `View->AntiAliasingMethod = GlobalSettings.AntiAliasingMethod;`
- **说明**: 可以配置使用TSR/TAA/FXAA

#### 2. DOF (Depth of Field)
- **影响**: 正常执行
- **来源**: 缓存自主View的 `FinalPostProcessSettings`
- **说明**: 使用与主Viewport相同的DOF设置

#### 3. Motion Blur
- **影响**: **强制禁用**
- **代码**: `ViewFamily->EngineShowFlags.MotionBlur = false;`
- **说明**: MQRC强制禁用，这是与主Viewport的主要差异

#### 4. Bloom
- **影响**: 正常执行
- **来源**: 缓存自主View的 `FinalPostProcessSettings`
- **说明**: HDR模式下正确执行

#### 5. Tonemapper
- **影响**: 正确执行
- **原因**: `bIsHDR = true` 强制启用HDR，确保正确的Tonemapper处理
- **说明**: 这是MQRC的关键设计决策

#### 6. GTAO (Ambient Occlusion)
- **影响**: 正常执行
- **来源**: 通过ShowFlags控制
- **说明**: 在Deferred阶段执行

#### 7. Lumen/SSGI
- **影响**: 正常执行
- **控制**: 通过ShowFlags控制
- **说明**: 使用与主Viewport相同的全局光照设置

#### 8. Exposure (自动曝光)
- **影响**: 正常执行
- **来源**: 缓存自主View的 `FinalPostProcessSettings`
- **说明**: EyeAdaptation正常工作

#### 9. Lens Flares / Vignette / Chromatic Aberration
- **影响**: 正常执行
- **来源**: 缓存自主View的 `FinalPostProcessSettings`
- **说明**: 作为Tonemapper的一部分执行

---

## 5. PostProcess 执行流程对比

### MainViewport PostProcess 流程

```
FSceneViewFamilyContext
    │
    ▼
FSceneRenderer (FDeferredShadingSceneRenderer)
    │
    ├── Render() → RenderBasePass()
    ├── RenderGBuffers()
    ├── RenderLights()
    │
    ▼
PostProcessing.cpp::AddPostProcessingPasses()
    │
    ├── TAA (TemporalAA)
    ├── DOF (DiaphragmDOF)
    ├── MotionBlur (如果启用)
    ├── PostProcessMaterial (BeforeBloom)
    ├── Downsample
    ├── EyeAdaptation
    ├── LocalExposure
    ├── Bloom
    ├── LensFlares
    ├── Tonemapper (合成所有效果)
    ├── PostProcessMaterial (AfterTonemapping)
    └── FXAA/Upscale
    │
    ▼
EndRenderFrame → Present
```

### MQRC PostProcess 流程

```
FSceneViewFamilyContext (bIsMainViewFamily=false, bIsHDR=true)
    │
    ▼
FSceneRenderer (FDeferredShadingSceneRenderer)
    │
    ├── Render() → RenderBasePass()
    ├── RenderGBuffers()
    ├── RenderLights()
    │
    ▼
PostProcessing.cpp::AddPostProcessingPasses()
    │
    ├── TAA (取决于配置)
    ├── DOF (从缓存应用)
    ├── MotionBlur ← **强制禁用**
    ├── PostProcessMaterial (BeforeBloom)
    ├── Downsample
    ├── EyeAdaptation (从缓存应用)
    ├── LocalExposure (从缓存应用)
    ├── Bloom (从缓存应用)
    ├── LensFlares (从缓存应用)
    ├── Tonemapper ← **HDR模式**
    ├── PostProcessMaterial (AfterTonemapping)
    └── FXAA/Upscale
    │
    ▼
输出到 RenderTarget (不Present)
```

---

## 6. 关键差异总结

### MQRC的优势

1. **HDR渲染**: 强制启用HDR，确保正确的线性色彩空间处理
2. **PostProcess缓存**: 从主View缓存设置，保证视觉一致性
3. **离线渲染**: `bIsOfflineRender = true`，针对质量优化
4. **无Present开销**: 直接输出到RenderTarget，不需要SwapChain

### MQRC的潜在问题

1. **MotionBlur禁用**: 强制禁用可能导致动态场景不自然
2. **缓存延迟**: PostProcessSettings变化可能延迟一帧
3. **Frame机制缺失**: 没有完整的Begin/EndRenderFrame，可能影响某些状态
4. **bIsMainViewFamily=false**: 某些渲染器行为可能有差异

### MainViewport的优势

1. **完整Frame机制**: Begin/EndRenderFrame确保正确的资源状态
2. **动态分辨率**: 支持ScreenPercentage动态调整
3. **Present控制**: 可以精确控制Present时机
4. **实时更新**: PostProcessSettings实时计算

---

## 7. 关键代码位置索引

### MQRC 相关

| 功能 | 文件:行号 |
|------|----------|
| ExecuteCaptureFrame | MovieQualityRenderComponent.cpp:436 |
| CreateViewFamily | MovieQualityRenderComponent.cpp:533 |
| CreateSceneView | MovieQualityRenderComponent.cpp:540 |
| bIsHDR = true | MovieQualityRenderComponent.cpp:636 |
| MotionBlur = false | MovieQualityRenderComponent.cpp:631 |
| PostProcess缓存 | MovieQualityRenderComponent.cpp:745-751 |
| FMovieQualityViewExtension | MovieQualityRenderComponent.h:36-62 |

### MainViewport 相关

| 功能 | 文件:行号 |
|------|----------|
| FViewport::Draw | UnrealClient.cpp:1725 |
| EnqueueBeginRenderFrame | UnrealClient.cpp:1776 |
| UGameViewportClient::Draw | GameViewportClient.cpp:1411 |
| bIsMainViewFamily = true | GameViewportClient.cpp:1896 |
| Canvas.Flush | UnrealClient.cpp:1816 |
| EnqueueEndRenderFrame | UnrealClient.cpp:1824 |

### 渲染管线共同入口

| 功能 | 文件:行号 |
|------|----------|
| BeginRenderingViewFamily | SceneRendering.cpp:4940 |
| CreateSceneRenderers | SceneRenderBuilder.cpp:984 |
| AddPostProcessingPasses | PostProcessing.cpp:347 |
| FDeferredShadingSceneRenderer | DeferredShadingRenderer.cpp |
