# UnrealCV File Path Index

Auto-generated index of frequently accessed files. Last updated: 2026-03-06

Use these shorthand paths in prompts instead of copy-pasting full paths.

## Session Context Files (2026-03-06)

**Session Topic**: Semantic annotation system implementation - adding material and texture metadata extraction for dataset generation; created MetaDataBPLib with physical material properties (Roughness, Metallic, BaseColor, etc.)

### Blueprint Function Libraries (Semantic Annotation)
- `Source/UnrealCV/Public/BPFunctionLib/MetaDataBPLib.h` - FMaterialSemanticMetadata/FTextureSemanticMetadata structs with physical properties (Roughness, Metallic, BaseColor, EmissiveColor); JSON serialization helpers
- `Source/UnrealCV/Private/BPFunctionLib/MetaDataBPLib.cpp` - Material parameter extraction from ScalarParameterValues/VectorParameterValues; texture metadata from UTexture resource

### Recording & Capture (Metadata Integration)
- `Source/UnrealCV/Private/Actor/FusionCamCaptureActor.cpp` - SaveOverviewMetadata() integration with semantic annotations via UMetaDataBPLib::GetSemanticAnnotationsJson()
- `Source/UnrealCV/Public/Actor/FusionCamCaptureActor.h`

### UE5 Engine References (Material System)
- `H:\UE_5.6\Engine\Source\Runtime\Engine\Public\Materials\MaterialInterface.h` - UMaterialInterface base class with GetBlendMode(), GetShadingModels(), IsTwoSided(), GetOpacityMaskClipValue()
- `H:\UE_5.6\Engine\Source\Runtime\Engine\Public\Materials\Material.h` - UMaterial class with MaterialDomain, GetBaseMaterial()
- `H:\UE_5.6\Engine\Source\Runtime\Engine\Public\Materials\MaterialInstance.h` - UMaterialInstance with ScalarParameterValues, VectorParameterValues, TextureParameterValues arrays; BasePropertyOverrides struct
- `H:\UE_5.6\Engine\Source\Runtime\Engine\Public\Materials\MaterialCachedData.h` - FMaterialCachedExpressionData with bHasMaterialLayers, bHasSceneColor, ReferencedTextures array
- `H:\UE_5.6\Engine\Source\Runtime\Engine\Public\Materials\MaterialInstanceBasePropertyOverrides.h` - FMaterialInstanceBasePropertyOverrides with override flags for BlendMode, ShadingModel, OpacityMaskClipValue

### UE5 Engine References (Texture System)
- `H:\UE_5.6\Engine\Source\Runtime\Engine\Classes\Engine\Texture.h` - UTexture base class with CompressionSettings, SRGB property; GetResource() for size info; GetTextureClass() enum
- `H:\UE_5.6\Engine\Source\Runtime\Engine\Public\TextureResource.h` - FTextureResource with GetSizeX(), GetSizeY(), GetCurrentMipCount()

## Previous Session Files (2026-03-01)

**Session Topic**: MainViewportRenderComponent async GPU readback implementation - migrating from synchronous ReadSurfaceData to FUnrealCVSurfaceQueue for 30-50% performance improvement; fixed camera projection matrix mismatch causing RGB/mask misalignment

### Sensor System (Viewport Capture & Async Readback)
- `Source/UnrealCV/Private/Sensor/CameraSensor/MainViewportRenderComponent.cpp` - Async GPU readback via FUnrealCVSurfaceQueue (line 148-158 initialization, line 334-366 async capture); CaptureFrameSync() synchronous interface (line 371-501); projection matrix debug logging (line 281-294)
- `Source/UnrealCV/Public/Sensor/CameraSensor/MainViewportRenderComponent.h` - Added FUnrealCVSurfaceQueue member (line 67), CaptureFrameSync() declaration (line 38)
- `Source/UnrealCV/Private/Sensor/CameraSensor/BaseCameraSensor.cpp` - Fixed AspectRatio calculation from TextureTarget dimensions in GetCameraView() to match projection matrix (line 1088-1093); added projection matrix debug logging
- `Source/UnrealCV/Private/Sensor/CameraSensor/UnrealCVSurfaceReader.cpp` - FUnrealCVSurfaceQueue implementation with OnRenderTargetReady_RenderThread for async GPU readback
- `Source/UnrealCV/Public/Sensor/CameraSensor/UnrealCVSurfaceReader.h` - FUnrealCVSurfaceQueue class with SetFrameResolveLatency() for sync/async control

### Recording & Capture
- `Source/UnrealCV/Private/Actor/FusionCamCaptureActor.cpp` - Recording actor; converted bUseMovieQualityRendering/bRecordViaViewport to static global settings
- `Source/UnrealCV/Public/Actor/FusionCamCaptureActor.h` - Static global settings for recording control

### Command Handlers (Recording Settings)
- `Source/UnrealCV/Private/Commands/CaptureActorHandler.cpp` - Added TCP commands: /captureactor/use_movie_quality_rendering, /captureactor/record_via_viewport
- `Source/UnrealCV/Private/Commands/CaptureActorHandler.h`

### UE5 Engine References (Viewport & Backbuffer)
- `H:\UE_5.6\Engine\Source\Runtime\Engine\Private\Slate\SceneViewport.cpp` - FSceneViewport class; GetViewportRHI(), UseSeparateRenderTarget(), BeginRenderFrame() with backbuffer logic; SetViewportSize(), ResizeViewport()
- `H:\UE_5.6\Engine\Source\Runtime\Engine\Private\UnrealClient.cpp` - FRenderTarget::ReadPixels() implementation using RHICmdList.ReadSurfaceData
- `H:\UE_5.6\Engine\Source\Runtime\Engine\Public\UnrealClient.h` - FViewport class with GetViewportRHI() method (line 666), FViewportRHIRef member (line 799)
- `H:\UE_5.6\Engine\Source\Runtime\Slate\Private\Widgets\SViewport.cpp` - SViewport widget with ShouldRenderDirectly() check for drawing viewport quad
- `H:\UE_5.6\Engine\Source\Runtime\RHI\Public\DynamicRHI.h` - RHIGetViewportBackBuffer() pure virtual (line 680), inline implementation (line 1400-1402)
- `H:\UE_5.6\Engine\Source\Runtime\RHI\Public\RHIFwd.h` - FViewportRHIRef = TRefCountPtr<FRHIViewport> (line 138)
- `H:\UE_5.6\Engine\Source\Runtime\RHI\Public\RHIValidation.h` - RHI validation layer implementation

## Session Context Files (2026-03-01)

**Session Topic**: MQRC rendering pipeline upgrade - migrating from FCanvas to FSceneRenderBuilder, comparing with UE's SceneCaptureRendering pipeline, resolving FSceneRenderer private type access issues

### Sensor System (MQRC Rendering Pipeline)
- `Source/UnrealCV/Private/Sensor/CameraSensor/MovieQualityRenderComponent.cpp` - MQRC implementation; attempted FSceneRenderBuilder migration (reverted); SetupViewFamily/SetupView calls in ExecuteCaptureFrame (line 439-444); SubmitToRendererWithCallback uses FCanvas path
- `Source/UnrealCV/Public/Sensor/CameraSensor/MovieQualityRenderComponent.h` - FMovieQualityViewExtension class with BeginRenderViewFamily override
- `Source/UnrealCV/Private/Sensor/CameraSensor/UnrealCVSurfaceReader.cpp` - Surface queue with OnRenderTargetReady_RenderThread for GPU readback

### Build Configuration
- `Source/UnrealCV/UnrealCV.Build.cs` - Module dependencies; attempted PrivateIncludePathModuleNames for Renderer Private access

### UE5 Engine References (SceneRenderBuilder & Rendering Pipeline)
- `H:\UE_5.6\Engine\Source\Runtime\Renderer\Private\SceneRenderBuilder.cpp` - CreateSceneRenderer (line 472); AddRenderer (line 562); Execute (line 1101); ViewExtension callbacks at line 506-523
- `H:\UE_5.6\Engine\Source\Runtime\Renderer\Private\SceneCaptureRendering.cpp` - SetupSceneViewExtensionsForSceneCapture (line 806-822); CreateSceneRendererForSceneCapture; AddRenderer lambda pattern for render capture
- `H:\UE_5.6\Engine\Source\Runtime\Renderer\Private\SceneRendering.h` - FSceneRenderer class definition (line 2067, private type)
- `H:\UE_5.6\Engine\Source\Runtime\Engine\Public\SceneRenderBuilderInterface.h` - ISceneRenderBuilder interface; CreateSceneRenderer; AddRenderer; FSceneRenderFunction type (line 48)
- `H:\UE_5.6\Engine\Source\Runtime\Engine\Public\SceneViewExtension.h` - ISceneViewExtension with SetupViewFamily/SetupView/BeginRenderViewFamily/PostCreateSceneRenderer methods
- `H:\UE_5.6\Engine\Source\Runtime\Engine\Public\SceneView.h` - FSceneViewFamilyContext class (line 2576)
- `H:\UE_5.6\Engine\Source\Runtime\Renderer\Public\SceneRendererInterface.h` - ISceneRenderer interface (line 46)
- `H:\UE_5.6\Engine\Source\Runtime\Engine\Public\StereoRendering.h` - IStereoRendering for multi-view rendering

### UE5 Engine References (Deferred Shading Renderer)
- `H:\UE_5.6\Engine\Source\Runtime\Renderer\Private\DeferredShadingRenderer.h` - FDeferredShadingSceneRenderer class (line 315): public FSceneRenderer
- `H:\UE_5.6\Engine\Source\Runtime\Renderer\Private\DeferredShadingRenderer.cpp` - Main deferred rendering implementation with Render() method; includes various rendering passes

## Session Context Files (2026-02-28)

**Session Topic**: UE 5.6 Landscape LOD crash with Ray Tracing - FLandscapeSceneViewExtension view counting issue when MQRC's global ViewExtension interferes with SceneCaptureComponent2D rendering pipeline

### Sensor System (Landscape LOD Crash Investigation)
- `Source/UnrealCV/Private/Sensor/CameraSensor/BaseCameraSensor.cpp` - USceneCaptureComponent2D subclass; bUseRayTracingIfEnabled=true (line 39), bCaptureEveryFrame=true (line 31), bAlwaysPersistRenderingState=true (line 40); LaunchCapture() calls CaptureScene() (line 744-754)
- `Source/UnrealCV/Public/Sensor/CameraSensor/BaseCameraSensor.h` - BaseCameraSensor class definition with async capture pipeline
- `Source/UnrealCV/Private/Sensor/CameraSensor/MovieQualityRenderComponent.cpp` - Custom rendering pipeline with manual ViewFamily/View creation; FMovieQualityViewExtension registration (line 102); SubmitToRendererWithCallback (line 588-646); attempted manual Landscape LOD computation (reverted due to linking errors)
- `Source/UnrealCV/Public/Sensor/CameraSensor/MovieQualityRenderComponent.h` - FMovieQualityViewExtension class with GetPriority()=-100 (line 29); UMovieQualityRenderComponent with custom CreateSceneView (sets bSceneCaptureUsesRayTracing=true, bIsSceneCapture=true)
- `Source/UnrealCV/Private/Sensor/CameraSensor/PanoramicCamSensor.cpp` - Panoramic sensor with bUseRayTracingIfEnabled=true (line 18)
- `Source/UnrealCV/Private/Sensor/CameraSensor/LitCamSensor.cpp` - RGB sensor with Lumen configuration when ray tracing supported (line 24-31)

### Build Configuration
- `Source/UnrealCV/UnrealCV.Build.cs` - Module dependencies; added "Landscape" module (line 69) for manual LOD computation attempt

### Documentation (Created During Session)
- `MQRC-Landscape-LOD-Crash-Analysis.md` - Comprehensive technical analysis of UE 5.6 Landscape LOD caching crash with SceneCaptureComponent2D + Ray Tracing

### UE5 Engine References (Landscape Rendering System)
- `H:\UE_5.6\Engine\Source\Runtime\Landscape\Public\LandscapeRender.h` - FLandscapeRenderSystem class (lines 507-656); FLandscapeSceneViewExtension with PreRenderView_RenderThread and view counting logic
- `H:\UE_5.6\Engine\Source\Runtime\Landscape\Private\LandscapeRender.cpp` - GetCachedSectionLODValues() crash site (line 1023); PreRenderView_RenderThread with view counting condition `LandscapeViews.Num() == InView.Family->AllViews.Num()` (line 1218); ComputeSectionsLODForView() (line 1199-1287); GetDynamicRayTracingInstances() (line 3072-3122)
- `H:\UE_5.6\Engine\Source\Runtime\Landscape\Private\LandscapeModule.cpp` - FLandscapeSceneViewExtension global registration in OnPostEngineInit() (line 263-267)

### UE5 Engine References (SceneCapture & ViewExtension Pipeline)
- `H:\UE_5.6\Engine\Source\Runtime\Renderer\Private\SceneCaptureRendering.cpp` - SetupSceneViewExtensionsForSceneCapture() (line 806-822); SetupViewFamilyForSceneCapture() (line 666-1003); UpdateSceneCaptureContent()
- `H:\UE_5.6\Engine\Source\Runtime\Renderer\Private\SceneRendering.cpp` - ViewExtension PreRenderView_RenderThread callbacks (line 3929-3943); main viewport rendering flow
- `H:\UE_5.6\Engine\Source\Runtime\Renderer\Private\SceneVisibility.cpp` - ViewExtension PreInitViews_RenderThread callbacks (line 4912-4915)
- `H:\UE_5.6\Engine\Source\Runtime\Engine\Public\SceneViewExtension.h` - ISceneViewExtension base class with GetPriority() default=0 (line 217)
- `H:\UE_5.6\Engine\Source\Runtime\Engine\Private\Components\SceneCaptureComponent.cpp` - SceneCapture2D view setup and ViewState management

## Previous Session Files (2026-02-25)

**Session Topic**: Gamma control mechanism analysis - SceneCaptureComponent2D vs MovieRenderPipeline color space handling, sRGB encoding, TextureRenderTarget configuration, and FReadSurfaceDataFlags behavior

### Sensor System (Gamma & Color Space)
- `Source/UnrealCV/Private/Sensor/CameraSensor/BaseCameraSensor.cpp` - USceneCaptureComponent2D subclass; InitTextureTarget with bForceLinearGamma=false (line 79-87); CaptureSource=SCS_FinalColorLDR (line 36); ReadFlags.SetLinearToGamma(false) in GPU readback (line 427, 949)
- `Source/UnrealCV/Public/Sensor/CameraSensor/BaseCameraSensor.h` - Async capture pipeline with FQueuedCapture, ECaptureFormat enum, CaptureCache for Fast path

### UE5 Engine References (Gamma & Color Management)
- `H:\UE_5.6\Engine\Source\Runtime\Engine\Classes\Components\SceneCaptureComponent.h` - USceneCaptureComponent base class with CaptureSource enum
- `H:\UE_5.6\Engine\Source\Runtime\Engine\Private\Components\SceneCaptureComponent.cpp` - SceneCapture view setup and FSceneViewStateReference management
- `H:\UE_5.6\Engine\Source\Runtime\Engine\Classes\Components\SceneCaptureComponent2D.h` - USceneCaptureComponent2D class definition
- `H:\UE_5.6\Engine\Source\Runtime\Engine\Public\SceneView.h` - FSceneView class with color space and gamma processing
- `H:\UE_5.6\Engine\Source\Runtime\Engine\Classes\Engine\TextureRenderTarget2D.h` - bForceLinearGamma property (line 127-129); InitCustomFormat method; GetDisplayGamma implementation (line 703-721)
- `H:\UE_5.6\Engine\Source\Runtime\Engine\Private\TextureRenderTarget2D.cpp` - GetDisplayGamma logic: TargetGamma priority → bForceLinearGamma → default 2.2 (line 703-721); InitCustomFormat (line 135-158)
- `H:\UE_5.6\Engine\Source\Runtime\Renderer\Private\SceneCaptureRendering.cpp` - SCS_FinalColorLDR rendering path (line 213-216); SetupViewFamilyForSceneCapture
- `H:\UE_5.6\Engine\Source\Runtime\RHI\Public\RHITypes.h` - FReadSurfaceDataFlags with SetLinearToGamma method (line 15-123); bLinearToGamma default=true

### UE5 Engine References (MovieRenderPipeline - Gamma Control)
- `H:\UE_5.6\Engine\Plugins\MovieScene\MovieRenderPipeline\Source\MovieRenderPipelineCore\Private\MoviePipelineSurfaceReader.cpp` - FRHIGPUTextureReadback async GPU readback; SCS_SceneColorHDR linear workflow
- `H:\UE_5.6\Engine\Plugins\MovieScene\MovieRenderPipeline\Source\MovieRenderPipelineCore\Private\MoviePipelineImageQuantization.cpp` - GenerateSRGBTable with precise gamma formula; ConvertLinearTosRGB8bpp with dithering
- `H:\UE_5.6\Engine\Plugins\MovieScene\MovieRenderPipeline\Source\MovieRenderPipelineCore\Private\MoviePipelineImageSequenceOutput.cpp` - Format-dependent gamma strategy (PNG=sRGB, EXR=linear)
- `H:\UE_5.6\Engine\Plugins\MovieScene\MovieRenderPipeline\Source\MovieRenderPipelineCore\Public\MoviePipelineColorSetting.h` - OCIOConfiguration and bDisableToneCurve properties

## Previous Session Files (2026-02-25)

**Session Topic**: UE 5.6 SceneCaptureComponent2D anti-aliasing source code analysis - tracing how TSR/TAA is determined from Project Settings → `r.AntiAliasingMethod` CVar → `FSceneView::SetupAntiAliasingMethod()`

### Sensor System
- `Source/UnrealCV/Private/Sensor/CameraSensor/BaseCameraSensor.cpp` - USceneCaptureComponent2D subclass; ShowFlags.SetAntiAliasing/TemporalAA currently commented out (AA disabled)
- `Source/UnrealCV/Public/Sensor/CameraSensor/BaseCameraSensor.h` - UBaseCameraSensor class inheriting USceneCaptureComponent2D; controls FOV, film size, async capture pipeline

### UE5 Engine References (SceneCapture & Anti-Aliasing)
- `H:\UE_5.6\Engine\Source\Runtime\Engine\Public\SceneView.h` - FSceneView class; `AntiAliasingMethod` field at line 1725; `SetupAntiAliasingMethod()` declaration at line 1940
- `H:\UE_5.6\Engine\Source\Runtime\Engine\Private\SceneView.cpp` - FSceneView constructor (line 857): `State(InitOptions.SceneViewStateInterface)`; `SetupAntiAliasingMethod()` called at line 1009; `r.AntiAliasingMethod` CVar defined at line 219 (default=4=TSR)
- `H:\UE_5.6\Engine\Source\Runtime\Engine\Private\SceneUtils.cpp` - `GetDefaultAntiAliasingMethod()` implementation (line 72): reads `r.AntiAliasingMethod` CVar; TSR→TAA fallback if platform unsupported (line 128-135)
- `H:\UE_5.6\Engine\Source\Runtime\Engine\Public\SceneUtils.h` - `GetDefaultAntiAliasingMethod(FeatureLevel)` declaration
- `H:\UE_5.6\Engine\Source\Runtime\Engine\Classes\Engine\RendererSettings.h` - `DefaultFeatureAntiAliasing` UPROPERTY bound to `r.AntiAliasingMethod` CVar (line 839); maps Project Settings UI → CVar
- `H:\UE_5.6\Engine\Source\Runtime\Engine\Private\Components\SceneCaptureComponent.cpp` - SceneCapture2D view setup; `GetViewState()` (line 403); `FSceneViewStateReference` allocation for TAA history
- `H:\UE_5.6\Engine\Source\Runtime\Renderer\Private\SceneCaptureRendering.cpp` - `SetupViewFamilyForSceneCapture()` (line 666): creates `FSceneViewInitOptions`, sets `SceneViewStateInterface` from `GetViewState()`, calls `new FSceneView(ViewInitOptions)` at line 743
- `H:\UE_5.6\Engine\Source\Runtime\Engine\Private\LocalPlayer.cpp` - Standard game render path; `new FSceneView(ViewInitOptions)` at line 880 (comparison reference)

## Previous Session Files (2026-02-24)

**Session Topic**: Global MQRC render quality configuration system - centralized runtime control for anti-aliasing, exposure, motion blur, Lumen, and color grading

### Sensor System (Rendering Configuration)
- `Source/UnrealCV/Public/Sensor/CameraSensor/MovieQualityRenderComponent.h` - FMQRCSettings struct with static GlobalSettings for unified render quality control (anti-aliasing, exposure, Lumen, color grading)
- `Source/UnrealCV/Private/Sensor/CameraSensor/MovieQualityRenderComponent.cpp` - Applies GlobalSettings to PostProcessSettings and AntiAliasingMethod (line 382, 470-550)

### Command Handlers (Render Quality)
- `Source/UnrealCV/Private/Commands/RenderQualityHandler.h` - FMQRCHandler class with TCP command methods for render quality configuration
- `Source/UnrealCV/Private/Commands/RenderQualityHandler.cpp` - /mqrc/* TCP commands: antialiasing, exposure_method/bias/min_brightness/max_brightness, motion_blur, lumen_quality, saturation, contrast, gamma, gain

### Server Core
- `Source/UnrealCV/Private/Server/UnrealcvServer.cpp` - Registered FMQRCHandler in CommandHandlers (line 126)

### UE5 Engine References (Post-Processing)
- `H:\UE_5.6\Engine\Source\Runtime\Engine\Classes\Engine\Scene.h` - FPostProcessSettings struct with AutoExposureMinBrightness/MaxBrightness properties

## Previous Session Files (2026-02-24)

**Session Topic**: Python client TCP resilience - fixing process hang on Ctrl+C and TCP disconnection in Windows

### Python Client Library (TCP Connection Management)
- `client/python/unrealcv/__init__.py` - TCP client with daemon thread + timeout join() to prevent hang on disconnect (daemon=True, join(timeout=2.0))
- `docs/python/run_matting_generation.py` - Dataset generation script with AssertionError exception handling for TCP failures

## Earlier Session Files (2026-02-21)

**Session Topic**: Asset discovery and spawn system - implementing vget /objects/scan_assets command and vset /objects/spawn_from_path with robust Cook-friendly loading

### Blueprint Function Libraries (Asset Management & Spawn)
- `Source/UnrealCV/Public/BPFunctionLib/AssetDiscoveryBPLib.h` - Asset registry scanning for spawnable resources (StaticMesh/SkeletalMesh/Blueprint)
- `Source/UnrealCV/Private/BPFunctionLib/AssetDiscoveryBPLib.cpp` - ScanSpawnableAssets with FAssetRegistryModule, Actor Blueprint filtering
- `Source/UnrealCV/Public/BPFunctionLib/SpawnBPLib.h` - Unified spawn from asset path API
- `Source/UnrealCV/Private/BPFunctionLib/SpawnBPLib.cpp` - 7-layer spawn strategy: StaticLoadClass→LoadObject→StaticLoadObject fallbacks
- `Source/UnrealCV/Private/BPFunctionLib/SceneCompositionBPLib.cpp` - SpawnActorFromMetadata refactored to use SpawnBPLib (line 1117)
- `Source/UnrealCV/Public/BPFunctionLib/SceneCompositionBPLib.h`

### Command Handlers (Object Management)
- `Source/UnrealCV/Private/Commands/ObjectHandler.cpp` - Added ScanAssets and SpawnFromPath commands (delegates to SpawnBPLib)
- `Source/UnrealCV/Private/Commands/ObjectHandler.h` - Object manipulation commands: spawn, destroy, location, rotation, bounds

## Earlier Session Files (2026-02-21)

**Session Topic**: TCP command execution pipeline optimization - analyzing vget /camera/0/lit request flow and reducing TCP latency

### Server Core & Command Dispatch
- `Source/UnrealCV/Private/Server/UnrealcvServer.cpp` - Main server with Tick-based request processing, HandleRawMessage, ProcessPendingRequest
- `Source/UnrealCV/Private/Server/UnixTcpServer.cpp` - TCP socket server with FSocket management, **TCP_NODELAY optimization added**
- `Source/UnrealCV/Public/Server/UnixTcpServer.h` - TCP server class definition
- `Source/UnrealCV/Public/Server/CommandDispatcher.h` - Command routing interface with BindCommand and Exec methods
- `Source/UnrealCV/Private/Server/CommandDispatcher.cpp` - Command regex matching and delegate dispatch (Exec method at line 184)

### Python Client
- `client/python/unrealcv/__init__.py` - Python TCP client with batch_cmd, request, **TCP_NODELAY optimization added** (line 219)
- `Source/uezoo/unrealcv/api.py` - High-level Python API wrapper with batch_cmd implementation (line 189-212)

### UE5 Engine References (Networking)
- `H:\UE_5.6\Engine\Source\Runtime\Sockets\Public\Sockets.h` - FSocket class definition with SetNoDelay() method
- `H:\UE_5.6\Engine\Source\Runtime\Sockets\Public\SocketSubsystem.h` - Socket subsystem interface

## Documentation
- `cmd.md` - TCP command reference (vget/vset commands)
- `SOW-基于CG的音视频分层数据生产-latest.md` - Project specification

## Frequently Modified Files (By Commit Count)

### Sensor System (Rendering Core)
- `Source/UnrealCV/Private/Sensor/CameraSensor/MovieQualityRenderComponent.cpp` - High-quality rendering (32 commits - added GlobalSettings configuration)
- `Source/UnrealCV/Public/Sensor/CameraSensor/MovieQualityRenderComponent.h`

### Blueprint Function Libraries
- `Source/UnrealCV/Private/BPFunctionLib/DatasetAutomationBPLib.cpp` - Batch generation (29 commits)
- `Source/UnrealCV/Public/BPFunctionLib/DatasetAutomationBPLib.h`
- `Source/UnrealCV/Private/BPFunctionLib/AutomationBPLib.cpp` - Automation utilities

### Recording & Capture
- `Source/UnrealCV/Private/Actor/FusionCamCaptureActor.cpp` - Recording lifecycle manager (26 commits)
- `Source/UnrealCV/Public/Actor/FusionCamCaptureActor.h`

### Utilities
- `Source/UnrealCV/Private/Utils/MetaHumanCacheManager.cpp` - MetaHuman optimization
