# 开发日志 #09 — PortAudio 音频采集接入

**日期**: 2026-05-23

---

## 做了什么

### PortAudio 源码克隆
- 版本：v19.7.0（shallow clone）
- 路径：`models/portaudio/`

### 直接编译方案
PortAudio 的 CMake 太旧不支持 CMake 4.2，改为直接编译源文件：
- `src/common/*.c` — 跨平台核心代码（14个文件）
- `src/os/win/*.c` — Windows 平台适配（6个文件）
- `src/hostapi/wasapi/` — WASAPI 后端
- `src/hostapi/wdmks/` — WDM Kernel Streaming
- `src/hostapi/dsound/` — DirectSound
- `src/hostapi/wmme/` — Windows MME

### 链接外部库
- `winmm` — Windows 多媒体
- `ole32` — COM
- `uuid` — UUID
- `ksuser` — Kernel Streaming
- `setupapi` — 设备枚举
- `winhttp` — HTTP（OpenAI 引擎）

### CMake 更新
- 项目语言改为 C + CXX（PortAudio 是 C）
- C 标准设为 C11
- `VIM_HAS_PORTAUDIO=1` 编译宏
- 所有头文件路径自动注入

---

## 最终依赖状态

| 依赖 | 状态 |
|------|------|
| nlohmann/json v3.11.3 | ✓ |
| spdlog v1.14.1 | ✓ |
| Catch2 v3.7.0 | ✓ |
| SenseVoice 模型 (int8) | ✓ 229 MB |
| sherpa-onnx v1.13.2 | ✓ SDK + C API |
| Qwen2.5-1.5B Q4_K_M | ✓ 1.1 GB |
| llama.cpp b4927 | ✓ 静态链接 |
| PortAudio v19.7.0 | ✓ WASAPI + WDM-KS + DS + MME |
| WinHTTP | ✓ Windows 内置 |

**全部依赖就绪！**

---

## 构建验证

```
→ voice_input_method_app.exe 编译通过
→ ctest: 23/23 passed
```

## 当前可运行状态

```
voice_input_method_app.exe
  ├── 音频采集:  PortAudio WASAPI (真实录音)
  ├── ASR 识别:  Sherpa-ONNX + SenseVoice (本地)
  ├── LLM 纠错:  llama.cpp + Qwen2.5-1.5B (本地)
  ├── LLM 格式化: llama.cpp + Qwen2.5-1.5B (本地)
  ├── 文本输出:  SendInput (键盘模拟)
  └── 热键:     Win32 RegisterHotKey (Ctrl+Alt+V)
```

运行时需要 DLL 在 PATH 或 exe 同目录：
```
models/sherpa-onnx/lib/sherpa-onnx-c-api.dll
models/sherpa-onnx/lib/onnxruntime.dll
```
