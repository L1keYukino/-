# 开发日志 #04 — 第3阶段：LLM 引擎接入

**日期**: 2026-05-23

---

## 做了什么

### llama.cpp 本地引擎 (`llamacpp_engine.hpp/.cpp`)
- 封装 llama.h C API，条件编译 `#if __has_include("llama.h")`
- 模型加载：`llama_model_load()` → `llama_init_from_model()`
- ChatML 格式 prompt 构建（`<|system|>`, `<|user|>`, `<|assistant|>` 标签）
- Tokenize + 自回归生成（`llama_decode` + `llama_sampler_sample`）
- 流式输出：逐 token 回调 `StreamingCallback`
- `reset_context()` — 清除 KV Cache
- llama.h 不可用时 stub 模式（返回 `[stub:llama.cpp]` 前缀文本）

### OpenAI 云端引擎 (`openai_engine.hpp/.cpp`)
- Windows 原生 WinHTTP 实现，零额外依赖
- Chat Completions API 兼容（OpenAI / Claude / 本地 Ollama 等）
- JSON 请求体构建 + JSON 响应解析（提取 `choices[0].message.content`）
- 持 API Key 鉴权（`Authorization: Bearer`）
- 非 Windows 平台 fallback 为 stub

### LLM 降级组合器 (`llm_fallback.hpp`)
- `LLMFallbackEngine` 包装 primary + fallback 两个 ILLMEngine
- `is_ready()` 检查 primary → fallback 可用性
- `process_async/streaming` 自动路由到可用引擎
- 全部不可用时返回 `llm_error("No LLM engine available")`
- 纯头文件实现（inline）

### VoiceEngine 管线升级 (`engine.cpp` 重写)
- `initialize()` 创建三引擎链路：
  - primary: `LlamaCppEngine`（本地 Qwen2.5-3B）
  - fallback: `OpenAIEngine`（云端，可选启用）
  - 包装: `LLMFallbackEngine`
- LLM 纠错 pass：ASR final → error correction prompt → corrected text
- LLM 格式化 pass：corrected text → intent prompt templates → formatted output
- 多轮上下文：`ContextManager.add_turn()` 保存对话历史
- 完整状态机流转：Recognizing → Correcting → Formatting → Outputting → Idle

### 新增管线

```
PTT按下 → PortAudio录音 → 环形缓冲
PTT松开 → 停止 → 排空缓冲 → ASR识别(raw_text)
  → [LLM pass 1] 纠错 prompt → corrected_text
  → [LLM pass 2] 意图 prompt + few-shot + 历史 → formatted_text
  → 通知观察者 → 状态机归位 Idle
```

---

## 新增文件

| 文件 | 说明 |
|------|------|
| `src/llm/llamacpp_engine.hpp` | llama.cpp 引擎头文件 |
| `src/llm/llamacpp_engine.cpp` | llama.cpp 引擎实现（条件编译） |
| `src/llm/openai_engine.hpp` | OpenAI 引擎头文件（WinHTTP） |
| `src/llm/openai_engine.cpp` | OpenAI 引擎实现 |
| `src/llm/llm_fallback.hpp` | LLM 降级组合器（纯头文件） |

## 修改文件

| 文件 | 变更 |
|------|------|
| `src/core/engine.hpp` | 新增 PromptCatalog、ContextManager、LLM 引擎成员 |
| `src/core/engine.cpp` | 完全重写：双 pass LLM 管线 |

---

## 构建验证

```
cmake --build . --config Release
→ libvoice_input_method.a 编译通过
→ ctest: 23/23 passed, 0 failed
```

---

## 当前依赖状态

| 依赖 | 状态 |
|------|------|
| nlohmann/json v3.11.3 | ✓ 已接入 |
| spdlog v1.14.1 | ✓ 已接入 |
| Catch2 v3.7.0 | ✓ 已接入 |
| PortAudio | ✗ 未接入（代码就绪，条件编译） |
| sherpa-onnx | ✗ 未接入（代码就绪，条件编译） |
| llama.cpp | ✗ 未接入（代码就绪，条件编译） |
| WinHTTP | ✓ Windows 内置，零额外依赖 |

---

## 下一步

第4阶段：接入 SendInput 文本输出 + Win32 全局热键（PTT 按键说话 → 文本自动输出到目标窗口）
