# 开发日志 #02 — 第2阶段：音频采集 + ASR 引擎

**日期**: 2026-05-23

---

## 做了什么

### 无锁环形缓冲 (`audio_buffer.hpp`)
- 单生产者单消费者（SPSC）模型，无锁设计
- 生产者：PortAudio 音频回调线程（实时优先级）
- 消费者：ASR 处理线程
- atomic read/write 指针，CAS 保证线程安全
- `write()` / `read()` / `skip()` / `peek()` / `reset()`
- `read_available()` / `write_available()` / `capacity_ms()`

### 能量 VAD 检测器 (`vad.hpp`)
- 纯能量 RMS 判定的语音活动检测
- 双阈值：静音阈值(-40dB) / 语音阈值(-30dB)
- 迟滞逻辑：语音确认 100ms 避免误触发，静音确认 800ms 避免断句抖动
- State 枚举：Silence / Speech
- 为 Phase 5 Silero VAD 集成预留接口

### PortAudio 音频采集 (`portaudio_capture.hpp/.cpp`)
- `#if __has_include(<portaudio.h>)` 条件编译
- PortAudio 可用：完整 WASAPI 底层采集链路（Pa_Initialize → Pa_OpenStream → Pa_StartStream）
- PortAudio 不可用：编译通过但不采集真实音频（stub 模式）
- `enumerate_devices()` 枚举输入设备（含设备名/采样率/通道数）
- `select_device()` 设备选择
- `start()` / `stop()` / `is_running()`
- 音频回调自动写入环形缓冲（每帧 `frame_count * channels` 样本）
- PIMPL 模式封装 PortAudio 内部类型

### Sherpa-ONNX ASR 引擎 (`sherpa_onnx_asr.hpp/.cpp`)
- `#if __has_include("sherpa-onnx/c-api/c-api.h")` 条件编译
- sherpa-onnx 可用：SenseVoice 模型加载 + 离线识别（C API）
- sherpa-onnx 不可用：stub 模式返回占位结果供测试
- `process_audio()` 累积音频样本
- `end_utterance()` 触发离线识别 → ASRResult 回调
- `reset()` 清除状态、释放 recognizer
- 识别耗时打点（chrono 计时）

### VoiceEngine 管线串联 (`engine.cpp` 重写)
- `initialize()` 创建 PortAudioCapture + SherpaOnnxASR 实例，自动发现设备
- `ptt_press()` → 启动音频采集 → 环形缓冲开始写入
- `ptt_release()` → 停止采集 → 排空环形缓冲 → 喂入 ASR → end_utterance()
- `on_recognition_result()` → ASR 结果回调 → 状态机流转（Recognizing→Correcting→Outputting→Idle）
- `notify_transcription()` 新增，通知观察者转写结果
- 完整的 PTT 端到端链路：**按键 → 录音 → 缓冲 → ASR → 事件通知**

### 资产清理
- 删除重复的 `asr_types.hpp`（类型已在 `i_asr_engine.hpp` 中定义）
- 修复 HotkeyMod 重复定义（统一从 `types.hpp` 引入）
- 移除所有外部依赖，保持纯标准 C++20 编译

---

## 新增文件

| 文件 | 说明 |
|------|------|
| `src/audio/audio_buffer.hpp` | 无锁环形缓冲 |
| `src/audio/vad.hpp` | 能量 VAD 检测器 |
| `src/audio/portaudio_capture.hpp` | PortAudio 采集头文件 |
| `src/audio/portaudio_capture.cpp` | PortAudio 采集实现 |
| `src/asr/sherpa_onnx_asr.hpp` | Sherpa-ONNX ASR 头文件 |
| `src/asr/sherpa_onnx_asr.cpp` | Sherpa-ONNX ASR 实现 |

## 修改文件

| 文件 | 变更 |
|------|------|
| `src/core/engine.hpp` | 使用具体类型 PortAudioCapture/SherpaOnnxASR，新增 notify_transcription |
| `src/core/engine.cpp` | 完整重写：音频→ASR 管线串联 |
| `src/hotkey/i_hotkey_manager.hpp` | 移除重复 HotkeyMod，引用 types.hpp |

---

## 构建验证

```
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
→ libvoice_input_method.a 编译通过，零警告
```

编译单元（新增）：
- `portaudio_capture.cpp` — PortAudio 采集
- `sherpa_onnx_asr.cpp` — ASR 识别
- `engine.cpp` — 管线重写

## 已知限制

1. PortAudio 和 sherpa-onnx 未实际链接（`__has_include` 均为 false），stub 模式运行
2. 非流式识别（accumulate-then-decode），Phase 5 升级为流式
3. 未连接 LLM 纠错/格式化（Phase 3）
4. VAD 仅用于连续模式，PTT 模式暂不使用（Phase 5）

---

## 下一步

第3阶段：接入 llama.cpp + Qwen2.5-3B GGUF 模型，实现 ASR→纠错→格式化→输出 完整链路
