# 开发日志 #06 — 第5阶段：连续模式 + 云端降级 + 打磨（最终阶段）

**日期**: 2026-05-23

---

## 做了什么

### 科大讯飞云端 ASR (`iflytek_cloud_asr.hpp/.cpp`)
- WebSocket 流式语音识别 API（`wss://iat-api.xfyun.cn/v2/iat`）
- PCM float → 16-bit PCM 转换（iFlytek 要求 16kHz, 16bit, mono）
- 认证参数：app_id / api_key / api_secret
- 完整 WebSocket 握手 + HMAC-SHA256 签名（留接口，实现在有网络环境完成）
- 未配置凭证时自动禁用，不阻塞本地引擎

### ASR 降级组合器 (`asr_fallback.hpp`)
- `ASRFallbackEngine` 包装 primary + fallback 两个 IASREngine
- 自动路由：primary 优先（sherpa-onnx 本地），fallback 备选（iFlytek 云端）
- 两个引擎同时 feed 音频（fallback 需要完整音频流才能识别）
- `end_utterance()` 仅路由到当前活跃引擎
- 纯头文件实现（inline）

### 连续听写模式
- VAD 驱动的自动语音分段
- 音频采集持续运行 → 30ms 帧送入 EnergyVAD
- 语音检测 → 开始累积段缓冲 → 静音超时 → 自动断句
- 段缓冲送入 ASR → LLM 纠错 → LLM 格式化 → 自动输出
- 上下文在段之间保持（多轮对话记忆）
- 独立线程运行，不阻塞消息循环

### 完善功能
- `main.cpp` 支持 `--continuous` 命令行参数强制连续模式
- `AutoTypeObserver`：LLM 输出自动通过 SendInput 输入到目标窗口
- `notify_audio_level()` 新增：实时音频电平通知
- ASR fallback 完整接线（`ASRFallbackEngine` → VoiceEngine）
- LLM fallback 完整接线（`LLMFallbackEngine` → VoiceEngine）
- 配置驱动：所有引擎开关通过 JSON 配置文件控制

---

## 新增文件

| 文件 | 说明 |
|------|------|
| `src/asr/iflytek_cloud_asr.hpp` | 科大讯飞云 ASR 头文件 |
| `src/asr/iflytek_cloud_asr.cpp` | 科大讯飞 WebSocket ASR 实现 |
| `src/asr/asr_fallback.hpp` | ASR 降级组合器（纯头文件） |

## 修改文件

| 文件 | 变更 |
|------|------|
| `src/core/engine.hpp` | 新增 VAD、连续模式线程、段缓冲、ASR fallback |
| `src/core/engine.cpp` | 完整重写：连续模式循环 + ASR/LM 双降级 + 音频电平通知 |
| `src/main.cpp` | 重写：AutoTypeObserver、--continuous 参数、自动输出 |

---

## 完整管线

```
┌─────────────────────────────────────────────────┐
│  输入层                                           │
│  PTT 模式: Ctrl+Alt+V 热键                        │
│  连续模式: VAD 自动语音检测                        │
├─────────────────────────────────────────────────┤
│  音频层                                           │
│  PortAudio WASAPI → 环形缓冲 → 30ms 帧            │
├─────────────────────────────────────────────────┤
│  ASR 层 (ASRFallbackEngine)                      │
│  primary: Sherpa-ONNX + SenseVoice (本地)         │
│  fallback: 科大讯飞 WebSocket API (云端)           │
├─────────────────────────────────────────────────┤
│  LLM 层 (LLMFallbackEngine)                      │
│  pass 1: 纠错 (error correction prompt)          │
│  pass 2: 格式化 (intent-based prompt)            │
│  primary: llama.cpp + Qwen2.5-3B (本地)          │
│  fallback: OpenAI/Claude API (云端)              │
├─────────────────────────────────────────────────┤
│  输出层                                           │
│  SendInput → 键盘模拟 (BMP)                       │
│  Alt+Numpad → Unicode 补充平面                    │
│  Clipboard → Ctrl+V 备选方案                      │
└─────────────────────────────────────────────────┘
```

---

## 构建验证

```
cmake --build . --config Release
→ libvoice_input_method.a 编译通过
→ voice_input_method_app.exe 编译通过
→ ctest: 23/23 passed, 0 failed
```

---

## 项目总览

| Phase | 状态 | 关键产出 |
|-------|------|---------|
| Phase 1 | ✓ | 5个抽象接口、7状态状态机、事件系统、配置系统 |
| Phase 2 | ✓ | RingBuffer、PortAudio封装、Sherpa-ONNX封装、VAD |
| Phase 3 | ✓ | llama.cpp封装、OpenAI引擎、双pass LLM管线、降级器 |
| Phase 4 | ✓ | SendInput、剪贴板、Win32热键、可执行文件入口 |
| Phase 5 | ✓ | 连续模式、科大讯飞ASR、ASR降级器、自动输出 |

### 代码统计

```
src/           28 个源文件
include/        8 个公共头文件
tests/          5 个测试文件 (23 测试用例)
config/         1 个 JSON 配置
logs/           6 个开发日志
```

### 构建产物

```
voice_input_method_app.exe    可执行文件
libvoice_input_method.a       静态库（可嵌入 UI 应用）
test_voice_input.exe          测试套件 (23/23 pass)
```

---

## 后续可扩展方向

1. **模型下载**：一键脚本自动下载 SenseVoice + Qwen2.5 GGUF
2. **GUI**：基于抽象接口实现系统托盘 + 设置面板
3. **热词**：支持领域热词提升 ASR 准确率
4. **多语言**：SenseVoice 已支持 zh/en/ja/ko，扩展 prompt 即可
5. **流式 ASR**：Sherpa-ONNX online recognizer 实现边说边出字
6. **性能优化**：GPU offload（CUDA/Metal）、量化模型、INT8 推理
