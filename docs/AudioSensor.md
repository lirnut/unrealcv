# UnrealCV Audio Sensor 使用说明

## 概述

UnrealCV Audio Sensor 提供了在虚幻引擎中获取单独发声源声音的能力。基于 UE5 的 AudioMixer 系统，通过 `ISourceBufferListener` 接口捕获特定音频源的原始 PCM 数据。

**注意**: 音频系统已从相机系统完全分离，使用独立的 `FusionAudioSensor` 类。

## 音频传感器系统架构

```
USceneComponent (UE引擎基类)
    │
    ├── UBaseAudioSensor (音频传感器基类)
    │       │
    │       ├── UAudioSourceSensor (源特定音频捕获)
    │       └── UAmbientAudioSensor (环境音频捕获)
    │
    └── UFusionAudioSensor (音频融合传感器)
            # 聚合多个音频传感器，提供统一接口
```

## 音频传感器类型

### 1. BaseAudioSensor (基类)
所有音频传感器的基类，提供通用功能。

### 2. AudioSourceSensor (源音频传感器)
捕获来自特定 `UAudioComponent` 的音频数据。

**特点**:
- 使用 `ISourceBufferListener` 监听特定源的音频
- 捕获距离衰减前的原始音频数据
- 可选择是否在捕获后清零音频缓冲区（静音原声）

**主要方法**:
```cpp
// 开始从特定音频组件捕获
void StartCaptureFromComponent(UAudioComponent* TargetAudioComponent);

// 从特定演员捕获（使用第一个找到的 AudioComponent）
void StartCaptureFromActor(AActor* TargetActor);

// 停止捕获
void StopCapture();

// 获取当前源ID
int32 GetCurrentSourceId() const;

// 获取捕获的音频数据
FAudioCaptureData GetCapturedAudio() const;
```

### 3. AmbientAudioSensor (环境音频传感器)
捕获世界中的环境音频（主输出混音）。

**特点**:
- 使用 `ISubmixBufferListener` 监听主子混音输出
- 捕获最终混合的世界音频
- 可选择从特定子混音捕获

### 4. FusionAudioSensor (音频融合传感器)
**新类**: 独立的音频融合传感器，提供统一的音频捕获接口。

**特点**:
- 不依赖 FusionCamSensor
- 可同时捕获环境音频和源特定音频
- 支持多种捕获预设
- 提供统一的文件保存接口

## 使用示例

### 在 C++ 中使用独立音频传感器

```cpp
#include "Sensor/AudioSensor/FusionAudioSensor.h"

// 创建融合音频传感器
UFusionAudioSensor* AudioSensor = NewObject<UFusionAudioSensor>(this);
AudioSensor->SetupAttachment(RootComponent);

// 设置捕获预设
AudioSensor->Initialize(EAudioCapturePreset::SplitCapture);

// 设置目标音频组件（用于源特定捕获）
UAudioComponent* AudioComp = MyActor->FindComponentByClass<UAudioComponent>();
AudioSensor->SetTargetAudioComponent(AudioComp);

// 开始捕获
AudioSensor->StartCapture();

// ... 稍后 ...

// 获取音频数据
FAudioCaptureData AmbientData = AudioSensor->GetAmbientAudioData();
FAudioCaptureData SourceData = AudioSensor->GetSourceAudioData();

// 保存到文件
AudioSensor->SaveAllAudioToFiles(TEXT("ambient.wav"), TEXT("source.wav"));

// 停止捕获
AudioSensor->StopCapture();
```

### 在蓝图中使用

1. 添加 `UFusionAudioSensor` 组件到 Actor
2. 设置 `CapturePreset` 属性
3. 调用 `StartCapture()` 开始捕获
4. 调用 `GetAmbientAudioData()` 或 `GetSourceAudioData()` 获取数据
5. 调用 `StopCapture()` 停止捕获

### 捕获预设

```cpp
UENUM(BlueprintType)
enum class EAudioCapturePreset : uint8
{
    Ambient,        // 仅环境音频
    SingleSource,   // 仅特定源
    SplitCapture,   // 同时捕获环境和源
    MultiSource     // 多源捕获
};
```

## FAudioCaptureData 结构

```cpp
USTRUCT(BlueprintType)
struct FAudioCaptureData
{
    TArray<float> Samples;    // 原始音频样本 (32-bit float)
    int32 NumChannels;        // 通道数 (通常是 2 为立体声)
    int32 SampleRate;         // 采样率 (通常是 48000 Hz)
    float Duration;           // 持续时间 (秒)
    float Timestamp;          // 捕获时间戳

    int32 GetNumFrames() const;  // 获取帧数
};
```

**注意**: Samples 数据是交错存储的 (对于立体声: L,R,L,R...)

## FusionAudioSensor 完整接口

```cpp
// 初始化
void Initialize(EAudioCapturePreset Preset);

// 捕获控制
void StartCapture();
void StopCapture();
bool IsCapturing() const;

// 单独控制
void StartAmbientCapture();
void StopAmbientCapture();
void StartSourceCapture();
void StopSourceCapture();

// 数据获取
FAudioCaptureData GetAmbientAudioData() const;
FAudioCaptureData GetSourceAudioData() const;
void FlushAllAudioData(FAudioCaptureData& OutAmbient, FAudioCaptureData& OutSource);

// 源设置
void SetTargetAudioComponent(UAudioComponent* TargetAudioComponent);
UAudioComponent* GetTargetAudioComponent() const;

// 文件保存
void SaveAmbientAudioToFile(const FString& Filename);
void SaveSourceAudioToFile(const FString& Filename);
void SaveAllAudioToFiles(const FString& AmbientFilename, const FString& SourceFilename);

// 设置
void SetSampleRate(int32 InSampleRate);
void SetNumChannels(int32 InNumChannels);
void SetMaxCaptureDuration(float InMaxDuration);
```

## 委托事件

```cpp
// 捕获完成事件
UFUNCTION(BlueprintAssignable)
FOnAudioCaptureFinished OnAmbientCaptureFinished;

UFUNCTION(BlueprintAssignable)
FOnAudioCaptureFinished OnSourceCaptureFinished;

// 数据接收事件
UFUNCTION(BlueprintAssignable)
FOnAudioDataReceived OnAmbientDataReceived;

UFUNCTION(BlueprintAssignable)
FOnAudioDataReceived OnSourceDataReceived;
```

## 技术细节

### 音频数据流

```
AudioComponent (Game Thread)
    |
    v
FMixerSourceVoice (Audio Thread)
    |
    v
FMixerSourceManager::FSourceInfo
    |-- PreDistanceAttenuationBuffer (捕获点)
    |-- SourceBuffer
    v
Submix (效果处理)
    |
    v
硬件输出
```

**捕获点**: `PreDistanceAttenuationBuffer` - 距离衰减前的原始音频

### 线程安全

- `OnNewBuffer` 在**音频渲染线程**中调用
- 内部使用 `FCriticalSection` 保护数据
- 异步回调通过 `AsyncTask` 队列到游戏线程

### 性能考虑

- 捕获会增加内存使用（存储音频缓冲区）
- 使用 `SetMaxCaptureDuration()` 限制缓冲区大小
- 对于长期捕获，定期调用 `FlushCapturedAudio()`

## 与 FusionCamSensor 的关系

**重要**: 音频系统已完全从 `FusionCamSensor` 分离。

- `FusionCamSensor` 仅处理视觉传感器
- `FusionAudioSensor` 独立处理音频捕获
- 两者可以同时使用，互不干扰

```cpp
// 视觉传感器
UFusionCamSensor* CamSensor = ...;
CamSensor->GetLit(...);

// 音频传感器（独立）
UFusionAudioSensor* AudioSensor = ...;
AudioSensor->StartCapture();
```

## 限制和注意事项

1. **源特定捕获**: 只能捕获启用了 `SourceBufferListener` 的 `UAudioComponent`
2. **音频线程**: 回调在音频线程，注意线程安全
3. **内存管理**: 长时间捕获会占用大量内存，需定期清理
4. **格式**: 输出始终是 32-bit float，范围 -1.0 到 1.0

## 调试

使用控制台命令检查音频系统状态:
```
au.submix.drawgraph
au.AudioThreadCommand.LogEveryExecution 1
```

## 参考文件

- `Source/UnrealCV/Public/Sensor/AudioSensor/BaseAudioSensor.h`
- `Source/UnrealCV/Public/Sensor/AudioSensor/AudioSourceSensor.h`
- `Source/UnrealCV/Public/Sensor/AudioSensor/AmbientAudioSensor.h`
- `Source/UnrealCV/Public/Sensor/AudioSensor/FusionAudioSensor.h` (新)
- `Source/UnrealCV/Private/Sensor/AudioSensor/AudioSensorExample.cpp`
