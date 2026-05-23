# 开发日志 #10 — 最终版：语音输入法

**日期**: 2026-05-23

---

## 架构

```
麦克风 → PortAudio → SenseVoice ASR → DeepSeek LLM → SendInput 输出到文本框
```

## 三个引擎

| 引擎 | 技术 | 作用 |
|------|------|------|
| 音频采集 | PortAudio v19.7.0 (WASAPI) | 48kHz/44.1kHz 录音，环形缓冲 |
| 语音识别 | Sherpa-ONNX v1.13.2 + SenseVoice | 中文离线 ASR，16000Hz 输出 |
| 文本整理 | DeepSeek API (deepseek-chat) | 理解内容，自动判断输出格式 |

## LLM 策略

**不做关键词匹配，不做硬编码规则。** 一个 prompt 让 LLM 自己判断：

- 短文本 → 整理通顺输出
- 长文本/多要点 → 自动总结为清单
- 写邮件 → 自动输出完整邮件格式
- 技术内容 → 自动输出代码注释或文档

## 交互方式

`Ctrl+Alt+V` 切换录音（按一次开始，再按一次结束）

## 核心代码

```
src/
├── core/      engine.cpp/hpp       — 总调度(状态机、管线)
│              state_machine.cpp    — 线程安全状态机
│              events.hpp           — 观察者事件系统
│              types.hpp            — 枚举、结构体
├── audio/     portaudio_capture    — PortAudio WASAPI 录音
│              audio_buffer.hpp     — 无锁环形缓冲(SPSC)
│              vad.hpp              — 能量VAD(连续模式预留)
├── asr/       sherpa_onnx_asr      — SenseVoice 离线识别
│              iflytek_cloud_asr    — 科大讯飞云端备用
│              asr_fallback.hpp     — ASR降级组合器
├── llm/       llamacpp_engine      — llama.cpp 本地引擎
│              openai_engine        — DeepSeek/OpenAI HTTP
│              prompt_templates     — Prompt模板库
│              context_manager      — 多轮对话上下文
├── output/    sendinput_output     — SendInput 键盘模拟
│              clipboard_output     — 剪贴板 Ctrl+V 备用
├── hotkey/    win32_hotkey         — Win32 全局热键
└── config/    config.cpp           — JSON 配置加载
```

## 依赖

| 库 | 来源 | 大小 |
|----|------|------|
| SenseVoice ONNX | GitHub releases | 229MB(int8)/895MB(fp32) |
| Sherpa-ONNX SDK | GitHub v1.13.2 | 18MB |
| Qwen2.5-1.5B GGUF | HuggingFace | 1.1GB |
| llama.cpp b4927 | GitHub shallow clone | 源码编译 |
| PortAudio v19.7.0 | GitHub shallow clone | 源码编译 |
| nlohmann/json v3.11.3 | CMake FetchContent | 自动 |
| spdlog v1.14.1 | CMake FetchContent | 自动 |
| Catch2 v3.7.0 | CMake FetchContent | 自动 |

## 测试

23/23 单元测试通过 (ContextManager, PromptCatalog, StateMachine, Config)

## 已知限制

1. ASR 偶尔在静音时输出 "The." — 已加过滤
2. llama.cpp 本地模型 (1.5B) 不够智能 — 云端 DeepSeek 兜底
3. 端口被强杀时 PortAudio 偶有残留 — 重启耳机可恢复
