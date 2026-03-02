# UE5.6 PostProcessing Effects 详细分析

**生成时间**: 2026-03-02
**源**: UE 5.6 Engine Source

---

## 1. Tonemapper (色调映射)

| 项目 | 内容 |
|------|------|
| **源文件** | `H:\UE_5.6\Engine\Source\Runtime\Renderer\Private\PostProcess\PostProcessTonemap.cpp` |
| **Shader** | `H:\UE_5.6\Engine\Shaders\Private\PostProcessTonemap.usf` |
| **关键类** | `FTonemapPS`, `FTonemapCS` |
| **入口函数** | `AddTonemapPass()` |

**与SceneColor交互**:
- 从FScreenPassTextureSlice接收HDR SceneColor
- 支持多种输出设备 (SDR_sRGB, HDR_LinearNoToneCurve等)
- 在Tonemap过程中合成: Bloom, FilmGrain, ChromaticAberration, Vignette
- 支持ACES和Filmic两种Tonemapper模式

---

## 2. Bloom (光晕效果)

### Gaussian Bloom

| 项目 | 内容 |
|------|------|
| **源文件** | `PostProcessBloomSetup.cpp` |
| **Shader** | `PostProcessBloom.usf` |
| **关键函数** | `BloomSetupPS`, `AddGaussianBloomPasses()` |

### FFT Bloom

| 项目 | 内容 |
|------|------|
| **源文件** | `PostProcessFFTBloom.cpp` |
| **Shader** | `Bloom\` 目录 |

**FFT Bloom子Shader**:
- `BloomFindKernelCenter.usf` - 寻找内核中心
- `BloomSurveyMaxScatterDispersion.usf` - 散射计算
- `BloomClampKernel.usf` - 内核钳制
- `BloomResizeKernel.usf` - 内核缩放

**与SceneColor交互**:
- Gaussian: 通过Downsample逐级降采样提取亮度 > Threshold的区域
- FFT: 使用GPUFFT库进行频域卷积，支持光谱域缓存内核

---

## 3. Exposure (自动曝光)

| 项目 | 内容 |
|------|------|
| **源文件** | `PostProcessEyeAdaptation.cpp` |
| **Shader** | `PostProcessEyeAdaptation.usf`, `PostProcessHistogram.usf` |
| **关键函数** | `AddHistogramEyeAdaptationPass()`, `AddBasicEyeAdaptationPass()` |

**曝光方法**:
- **Basic**: 简单平均亮度
- **Histogram**: 直方图统计 (排除极亮/极暗像素)
- **Local Exposure**: 使用Bilateral Grid进行局部曝光

**与SceneColor交互**:
- 从SceneColor提取Luminance
- EyeAdaptationBuffer存储计算结果，供Tonemap使用

---

## 4. DepthOfField (景深)

| 项目 | 内容 |
|------|------|
| **源文件** | `DiaphragmDOF.cpp` |
| **Shader** | `DiaphragmDOF\` 目录 |
| **关键类** | `FPhysicalCocModel`, `FBokehModel` |
| **入口函数** | `AddPasses()` |

**Shader子模块**:
- `DOFSetup.usf` - CoC设置
- `DOFDownsample.usf` - 降采样
- `DOFGatherPass.usf` - 散射采集
- `DOFRecombine.usf` - 重新合成
- `DOFBokehLUT.usf` - Bokeh形状查找表

**与SceneDepth交互**:
- 使用SceneDepth计算CoC (Circle of Confusion)
- 支持Foreground/Background分离
- 使用Hybrid Scatter/Gather模式进行模糊

---

## 5. CameraEffects

### Chromatic Aberration (色差)

| 项目 | 内容 |
|------|------|
| **位置** | PostProcessTonemap.cpp (集成在Tonemapper中) |
| **Shader** | PostProcessTonemap.usf |
| **参数** | ChromaticAberrationParams |

**计算**: 基于波长 (PrimaryR=611.3nm, PrimaryG=549.1nm, PrimaryB=464.3nm)

### Vignette (暗角)

| 项目 | 内容 |
|------|------|
| **位置** | PostProcessTonemap.cpp |
| **参数** | TonemapperParams.x = VignetteIntensity |

---

## 6. Lens Flares (镜头光晕)

| 项目 | 内容 |
|------|------|
| **源文件** | `PostProcessLensFlares.cpp` |
| **Shader** | `PostProcessLensFlares.usf` |
| **关键函数** | `AddLensFlaresPass()` |
| **实现** | 基于Bokeh的模糊 |

---

## 7. Dirt Mask (镜头脏迹)

| 项目 | 内容 |
|------|------|
| **位置** | PostProcessTonemap.cpp |
| **参数** | `BloomDirtMaskTexture`, `BloomDirtMaskTint` |
| **来源** | `PostProcessSettings.BloomDirtMask` |

---

## 8. Grain (胶片颗粒)

| 项目 | 内容 |
|------|------|
| **源文件** | `PostProcessTonemap.cpp` |
| **Shader** | `FilmGrainPackConstants.usf`, `FilmGrainReduce.usf` |
| **关键函数** | `FFilmGrainReduceCS`, `ComputeFilmGrainIntensity()` |

**特性**:
- 使用Halton序列生成时序稳定的随机值
- 支持Shadows/Midtones/Highlights不同强度

---

## 9. SSGI (Screen Space Global Illumination)

**说明**: UE5.6中SSGI主要通过**Lumen**实现，而非传统SSGI

| 组件 | 文件 |
|------|------|
| Lumen屏幕探针 | `LumenScreenProbeTracing.usf` |
| Lumen法线 | `LumenScreenSpaceBentNormal.usf` |
| SSRT | `SSRT\SSRTDiffuseIndirect.usf` |

---

## 10. GTAO (Ground Truth Ambient Occlusion)

| 项目 | 内容 |
|------|------|
| **源文件** | `CompositionLighting\PostProcessAmbientOcclusion.cpp` |
| **Shader** | `PostProcessAmbientOcclusion.usf` |
| **关键类** | `FGTAOContext` |

**Pass流程**:
1. **HorizonSearch** - 搜索地平线
2. **Integrate** - 积分计算
3. **SpatialFilter** - 空间滤波
4. **TemporalFilter** - 时序滤波
5. **Upsample** - 上采样

**与SceneDepth交互**:
- 使用SceneDepth和GBuffer法线
- 支持HZB (Hierarchical Z-Buffer) 优化

---

## 11. PostProcessMaterials (自定义后处理材质)

| 项目 | 内容 |
|------|------|
| **源文件** | `PostProcessMaterial.cpp` |
| **Shader** | `PostProcessMaterialShaders.usf` |
| **关键函数** | `AddPostProcessMaterialPass()`, `AddPostProcessMaterialChain()` |

**输入类型** (EPostProcessMaterialInput):
- SceneColor, SceneDepth, GBuffer各通道

---

## 12. ScreenSpaceSubSurfaceScattering

| 项目 | 内容 |
|------|------|
| **源文件** | `PostProcessSubsurface.cpp` |
| **Shader** | `PostProcessSubsurface.usf`, `PostProcessSubsurfaceTile.usf` |

**Pass流程**:
1. **Setup** - 记录需要处理的Tile
2. **Burley** - 基于Burley模型的散射
3. **Separable** - 可分离滤波
4. **Recombine** - 重新合成

---

## 13. BurleySSSSS

| 项目 | 内容 |
|------|------|
| **相关CVar** | `r.SSS.Scale`, `r.SSS.HalfRes`, `r.SSS.Quality`, `r.SSS.Burley.Quality` |

**与SceneColor交互**:
- 通过SceneColor的Alpha通道传输SSS数据
- 使用Bilateral Filter (Depth和Normal)

---

## PostProcessing Pass执行顺序总结

```
SceneColor
    │
    ▼
┌─────────────────────────────────────────────┐
│  1. DOF (BeforeDOF Passes)                  │
│     - DiaphragmDOF::AddPasses()             │
└─────────────────────────────────────────────┘
    │
    ▼
┌─────────────────────────────────────────────┐
│  2. MotionBlur                              │
│     - PostProcessMotionBlur.cpp             │
└─────────────────────────────────────────────┘
    │
    ▼
┌─────────────────────────────────────────────┐
│  3. PostProcessMaterial (BeforeBloom)      │
│     - AddPostProcessMaterialChain()         │
└─────────────────────────────────────────────┘
    │
    ▼
┌─────────────────────────────────────────────┐
│  4. Downsample (1/2, 1/4, 1/8)              │
│     - 用于EyeAdaptation, LocalExposure      │
└─────────────────────────────────────────────┘
    │
    ▼
┌─────────────────────────────────────────────┐
│  5. EyeAdaptation / LocalExposure           │
│     - PostProcessEyeAdaptation.cpp           │
│     - PostProcessLocalExposure.cpp          │
└─────────────────────────────────────────────┘
    │
    ▼
┌─────────────────────────────────────────────┐
│  6. Bloom (Gaussian 或 FFT)                  │
│     - PostProcessBloomSetup.cpp              │
│     - PostProcessFFTBloom.cpp                │
└─────────────────────────────────────────────┘
    │
    ▼
┌─────────────────────────────────────────────┐
│  7. LensFlares                               │
│     - PostProcessLensFlares.cpp             │
└─────────────────────────────────────────────┘
    │
    ▼
┌─────────────────────────────────────────────┐
│  8. Tonemapper (合成所有效果)                 │
│     - PostProcessTonemap.cpp                │
│       - ChromaticAberration                  │
│       - Vignette                             │
│       - FilmGrain                            │
│       - DirtMask                            │
│       - Bloom合成                           │
└─────────────────────────────────────────────┘
    │
    ▼
┌─────────────────────────────────────────────┐
│  9. PostProcessMaterial (AfterTonemapping)  │
│     - AddPostProcessMaterialChain()         │
└─────────────────────────────────────────────┘
    │
    ▼
    FXAA / TSR Upscale → Final Output
```

---

## 关键文件索引

### PostProcess核心

| 文件 | 职责 |
|---|---|
| `PostProcess\PostProcessing.cpp` | Pass链编排入口 (AddPostProcessingPasses) |
| `PostProcess\PostProcessTonemap.cpp` | Tonemapper + ChromaticAberration + Vignette + Grain |
| `PostProcess\PostProcessBloomSetup.cpp` | Gaussian Bloom |
| `PostProcess\PostProcessFFTBloom.cpp` | FFT Bloom |
| `PostProcess\PostProcessEyeAdaptation.cpp` | 自动曝光 |
| `PostProcess\PostProcessLocalExposure.cpp` | 局部曝光 |
| `PostProcess\DiaphragmDOF.cpp` | 景深 |
| `PostProcess\PostProcessMotionBlur.cpp` | 运动模糊 |
| `PostProcess\PostProcessLensFlares.cpp` | 镜头光晕 |
| `PostProcess\PostProcessSubsurface.cpp` | 次表面散射 |
| `PostProcess\PostProcessMaterial.cpp` | 自定义材质 |

### Shader文件

| 目录 | 内容 |
|---|---|
| `Shaders\Private\PostProcessTonemap.usf` | Tonemapper主Shader |
| `Shaders\Private\PostProcessBloom.usf` | Gaussian Bloom |
| `Shaders\Private\Bloom\` | FFT Bloom子Shader |
| `Shaders\Private\DiaphragmDOF\` | DOF子Shader |
| `Shaders\Private\PostProcessEyeAdaptation.usf` | 曝光 |
| `Shaders\Private\PostProcessLensFlares.usf` | 镜头光晕 |
| `Shaders\Private\PostProcessSubsurface.usf` | SSS |
| `Shaders\Private\PostProcessAmbientOcclusion.usf` | GTAO |

### GTAO (在Deferred阶段执行)

| 文件 | 职责 |
|---|---|
| `CompositionLighting\PostProcessAmbientOcclusion.cpp` | GTAO实现 |
| `Shaders\Private\PostProcessAmbientOcclusion.usf` | GTAO Shader |
