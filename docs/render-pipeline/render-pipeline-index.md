# UE MainViewport 渲染管线分析索引

**生成时间**: 2026-03-02
**工作流**: 6步Agent研究任务

---

## 渲染管线全景图

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        UE MainViewport 渲染管线                               │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  [Z-Buffer/Depth]  ──────►  [G-Buffer]  ──────►  [Lumen GI]               │
│       │                      │                      │                        │
│  SceneDepthZ           GBufferA-F           CardSystem                    │
│  DepthPass             BasePass              RadianceCache                │
│  DepthRendering.cpp    DeferredShading        ScreenProbeGather            │
│                        Common.ush                                     │
│                                                                      │
│       │                      │                      │                        │
│       ▼                      ▼                      ▼                        │
│                                                                 [Deferred Shading             │
│ ]  ──────►  [Post-Processing]  ──────►  [Screen Output]  │
│       │                      │                      │                        │
│  LightAccumulator      MotionBlur              Tonemapper                 │
│  ClusteredDeferred     Bloom                   GammaCorrection            │
│  IndirectLight         DoF                     SceneViewport              │
│  DeferredShading       TAA/FXAA                 SwapChain                  │
│    Renderer.cpp        PostProcessing.cpp       RHIBackBuffer             │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 步骤1: G-Buffer RenderPass

**目标**: G-Buffer各个RT的创建和写入

| RT | 用途 | 创建位置 | Shader |
|---|---|---|---|
| GBufferA | WorldNormal + PerObjectGBufferData | SceneTextures.cpp:697 | DeferredShadingCommon.ush:870 |
| GBufferB | Metallic, Specular, Roughness, ShadingModelID | SceneTextures.cpp:703 | DeferredShadingCommon.ush:882-885 |
| GBufferC | BaseColor + AO/IndirectIrradiance | SceneTextures.cpp:709 | DeferredShadingCommon.ush:887 |
| GBufferD | CustomData (ClearCoat, Cloth) | SceneTextures.cpp:715 | DeferredShadingCommon.ush:898 |
| GBufferE | PrecomputedShadowFactors | SceneTextures.cpp:721 | DeferredShadingCommon.ush:899 |
| GBufferF | WorldTangent + Anisotropy | SceneTextures.cpp:729 | AnisotropyPassShader.usf:123 |
| Velocity | 屏幕空间速度向量 | SceneTextures.cpp:687 | Common.ush:2043 |

**写入Pass**:
- BasePass → GBufferA,B,C,D,E,Velocity
- AnisotropyPass → GBufferF

**关键文件**:
- `M:\UnrealEngine\Engine\Source\Runtime\Renderer\Private\SceneTextures.cpp`
- `M:\UnrealEngine\Engine\Shaders\Private\DeferredShadingCommon.ush`

---

## 步骤2: Depth Pass

**目标**: Z-Buffer写入和Depth处理

| 项目 | 位置 |
|---|---|
| Depth Buffer创建 | SceneTextures.cpp:462 (PF_DepthStencil) |
| Depth Pass执行 | DepthRendering.cpp:480 |
| Depth Vertex Shader | DepthOnlyVertexShader.usf |
| Depth Pixel Shader | DepthOnlyPixelShader.usf |
| Light Grid / Tiled | MegaLights.cpp, LightGridInjection.cpp |

**Pass流程**:
```
SceneDepthZ创建 → ClearDepthStencilPass → RenderPrePass → DepthOnlyVS/PS
```

**关键文件**:
- `M:\UnrealEngine\Engine\Source\Runtime\Renderer\Private\DepthRendering.cpp`
- `M:\UnrealEngine\Engine\Source\Runtime\Renderer\Private\SceneTextures.cpp`

---

## 步骤3: Lumen光照

**目标**: Lumen读取G-Buffer进行全局光照

**Lumen读取的G-Buffer**:
| GBuffer | 用途 |
|--------|------|
| SceneDepth | 深度测试、表面位置重建 |
| GBufferA | BaseColor, Metallic, Specular |
| GBufferB | Roughness, Indirect Irradiance |
| GBufferC | ShadingModel, Primitive material ID |
| GBufferVelocity | 运动模糊temporally reprojection |

**Lumen核心组件**:
1. **CardSystem** - LumenSceneCardCapture.cpp - 场景表面映射到2D纹理卡片
2. **RadianceCache** - LumenRadianceCache.cpp - 远距离光照的多层Clipmap
3. **ScreenProbeGather** - LumenScreenProbeGather.cpp - 屏幕探针局部光照采样
4. **LightSampling** - LumenSceneDirectLighting.cpp, LumenRadiosity.cpp

**集成点** (DeferredShadingRenderer.cpp):
- `RenderLumenSceneLighting()` - 主入口
- `RenderDirectLightingForLumenScene()` - 直接光照
- `RenderRadiosityForLumenScene()` - 间接光照

**关键文件**:
- `M:\UnrealEngine\Engine\Source\Runtime\Renderer\Private\LumenScreenProbeGather.cpp`
- `M:\UnrealEngine\Engine\Source\Runtime\Renderer\Private\LumenRadianceCache.cpp`
- `M:\UnrealEngine\Engine\Source\Runtime\Renderer\Private\DeferredShadingRenderer.cpp`

---

## 步骤4: Deferred Shading合流

**目标**: Lighting Accumulator收集光照并合成

**光照类型收集**:
- `TotalLight` - 总累积光 (RGB)
- `TotalLightDiffuse/TotalLightSpecular` - 分离的漫反射/镜面反射
- `ScatterableLight` - 次表面散射

**Shader文件**: `LightAccumulator.ush`

**Deferred Pass执行顺序**:
```
1. RenderDiffuseIndirectAndAmbientOcclusion (DFAO/Lumen/SSGI)
2. RenderIndirectCapsuleShadows
3. RenderDFAOAsIndirectShadowing
4. RenderLights (Clustered Deferred)
   - Additive blend: BF_One, BF_One → SceneColor
5. RenderTranslucencyLightingVolume
6. RenderDeferredReflectionsAndSkyLighting
```

**关键文件**:
- `M:\UnrealEngine\Engine\Shaders\Private\LightAccumulator.ush`
- `M:\UnrealEngine\Engine\Source\Runtime\Renderer\Private\ClusteredDeferredShadingPass.cpp`
- `M:\UnrealEngine\Engine\Source\Runtime\Renderer\Private\DeferredShadingRenderer.cpp`

---

## 步骤5: Post-Processing后处理链

**目标**: Tonemapper之前的各种后处理Pass

**执行顺序** (PostProcessing.cpp:347):
```
1. TAA (Temporal Anti-Aliasing) - TemporalAA.cpp
2. PostProcessMaterial (BL_SceneColorBeforeDOF)
3. DiaphragmDOF (景深) - DiaphragmDOF.cpp
4. PostProcessMaterial (BL_SceneColorAfterDOF)
5. MotionBlur - PostProcessMotionBlur.cpp
6. PostProcessMaterial (BL_SceneColorBeforeBloom)
7. Downsample Chain (1/2, 1/4, 1/8)
8. Eye Adaptation (自动曝光)
9. Local Exposure
10. Bloom - PostProcessBloomSetup.cpp
11. Lens Flares
12. PostProcessMaterial (BL_SceneColorAfterTonemapping)
13. Tonemapper
14. FXAA
15. Upscale
```

**Pass间依赖**:
- SceneColorTexture (来自SceneTextures.Color.Target)
- SceneDepthTexture (用于DoF, Motion Blur)
- GBufferVelocityTexture (用于TSR)

**关键文件**:
- `M:\UnrealEngine\Engine\Source\Runtime\Renderer\Private\PostProcess\PostProcessing.cpp:347`
- `M:\UnrealEngine\Engine\Source\Runtime\Renderer\Private\PostProcess\PostProcessTonemap.cpp:569`
- `M:\UnrealEngine\Engine\Source\Runtime\Renderer\Private\PostProcess\PostProcessMotionBlur.cpp:1314`

---

## 步骤6: Tonemapper与Screen Output

**目标**: 最终输出到屏幕的过程

**Tonemapper实现**:
- 文件: `PostProcessTonemap.cpp:569`
- 函数: `AddTonemapperPass()`
- Shader: `Tonemapper.usf`

**Gamma Correction**:
- 位置: Tonemapper shader内
- 公式: `Linear → sRGB Gamma 2.2` 或 `ACES Tone Curve`

**最终输出路径**:
```
SceneColor (PostProcessing)
    ↓
Tonemapper (Linear→Gamma)
    ↓
GammaCorrection
    ↓
SceneViewport::BeginRenderFrame()
    ↓
RHIGetViewportBackBuffer()
    ↓
SwapChain Present
```

**HDR vs SDR**:
- SCS_FinalColorHDR → 线性HDR色彩空间 → Tonemap输出
- SCS_FinalColorLDR → sRGB Gamma编码 → 直接输出

**关键文件**:
- `M:\UnrealEngine\Engine\Source\Runtime\Renderer\Private\PostProcess\PostProcessTonemap.cpp`
- `M:\UnrealEngine\Engine\Source\Runtime\Engine\Private\Slate\SceneViewport.cpp`
- `M:\UnrealEngine\Engine\Source\Runtime\RHI\Public\DynamicRHI.h`

---

## 核心UE源码索引

### Renderer模块 (M:\UnrealEngine\Engine\Source\Runtime\Renderer\Private\)

| 文件 | 职责 |
|---|---|
| `SceneTextures.cpp` | GBuffer/Depth/SceneColor创建 |
| `DeferredShadingRenderer.cpp` | 主渲染管线编排 |
| `DepthRendering.cpp` | Depth Pass执行 |
| `ClusteredDeferredShadingPass.cpp` | Light Grid光照计算 |
| `PostProcessing.cpp` | 后处理Pass链编排 |
| `PostProcessTonemap.cpp` | Tonemapper实现 |

### Shader模块 (M:\UnrealEngine\Engine\Shaders\Private\)

| 文件 | 职责 |
|---|---|
| `DeferredShadingCommon.ush` | GBuffer编码/解码 |
| `LightAccumulator.ush` | 光照累加 |
| `DepthOnlyVertexShader.usf` | Depth写入 |
| `DepthOnlyPixelShader.usf` | Depth写入 |

### Engine模块

| 文件 | 职责 |
|---|---|
| `SceneViewport.cpp` | Viewport渲染入口 |
| `UnrealClient.cpp` | ReadPixels实现 |

---

## 附录: UnrealCV集成点

**与MainViewport渲染管线的关系**:
- `MainViewportRenderComponent.cpp` - 使用Viewport后buffer (通过SceneViewport::GetViewportRHI)
- `BaseCameraSensor.cpp` - 使用SceneCaptureComponent2D (独立渲染管线)
- `MovieQualityRenderComponent.cpp` - 自定义ViewExtension渲染 (类似MRQ)

**GPU Readback方式**:
- 同步: `ReadSurfaceData` (RenderThread同步读取)
- 异步: `FRHIGPUTextureReadback` + `OnRenderTargetReady_RenderThread`
