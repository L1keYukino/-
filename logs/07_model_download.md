# 开发日志 #07 — 模型和SDK下载

**日期**: 2026-05-23

---

## 做了什么

### SenseVoice 模型下载
- 来源：sherpa-onnx asr-models release
- 文件：`sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17.tar.bz2` (999 MB)
- 解压后 (`models/sensevoice/`)：
  - `model.int8.onnx` — 229 MB（int8 量化，推荐使用）
  - `model.onnx` — 895 MB（全精度）
  - `tokens.txt` — 309 KB（词表）
  - `test_wavs/` — 5个测试音频（zh/en/ja/ko/yue）

### sherpa-onnx SDK 下载
- 版本：v1.13.2
- 文件：`sherpa-onnx-v1.13.2-win-x64-shared-MD-Release.tar.bz2` (18 MB)
- 解压后 (`models/sherpa-onnx/`)：
  - `include/sherpa-onnx/c-api/c-api.h` — C API 头文件
  - `lib/sherpa-onnx-c-api.dll` — C API 动态库
  - `lib/sherpa-onnx-c-api.lib` — MSVC 导入库
  - `lib/onnxruntime.dll` — ONNX Runtime

### 代码适配 v1.13 C API
- 修正 API 函数名：
  - `SherpaOnnxAcceptWaveformOffline()` — 喂入音频
  - `SherpaOnnxDecodeOfflineStream()` — 执行解码
  - `SherpaOnnxGetOfflineStreamResult()` — 获取结果
- Offline stream 单次使用，每次 end_utterance 后重建 stream
- SenseVoice 配置：`model_config.sense_voice.model` + `use_itn=1`

### CMake 更新
- MinGW 直接链接 DLL（`target_link_libraries(... sherpa-onnx-c-api.dll)`）
- sherpa-onnx SDK 路径：`models/sherpa-onnx/`
- 条件编译：`VIM_HAS_SHERPA_ONNX=1`

---

## 构建验证

```
sherpa-onnx SDK found — linking against DLL
→ libvoice_input_method.a 编译通过
→ voice_input_method_app.exe 4.1 MB
→ ctest: 23/23 passed
```

---

## 当前依赖状态

| 依赖 | 状态 |
|------|------|
| nlohmann/json v3.11.3 | ✓ 已接入 |
| spdlog v1.14.1 | ✓ 已接入 |
| Catch2 v3.7.0 | ✓ 已接入 |
| SenseVoice 模型 | ✓ 已下载 (229 MB int8) |
| sherpa-onnx v1.13.2 | ✓ SDK 已下载 + 代码已适配 |
| PortAudio | ✗ 待下载 |
| llama.cpp + Qwen2.5 GGUF | ✗ 待下载 |

---

## 运行时依赖

运行 `voice_input_method_app.exe` 前需要确保 DLL 在 PATH 中：
```
models/sherpa-onnx/lib/sherpa-onnx-c-api.dll
models/sherpa-onnx/lib/onnxruntime.dll
models/sherpa-onnx/bin/onnxruntime.dll
```

或复制到 exe 同目录。
