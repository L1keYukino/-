# 开发日志 #08 — llama.cpp + Qwen2.5 GGUF 模型接入

**日期**: 2026-05-23

---

## 做了什么

### Qwen2.5-1.5B-Instruct GGUF 模型下载
- 来源：huggingface.co/Qwen/Qwen2.5-1.5B-Instruct-GGUF（via hf-mirror.com）
- 文件：`qwen2.5-1.5b-instruct-q4_k_m.gguf` (1.04 GB)
- 规格：GGUF v3, qwen2, 28 layers, 1536 dim, 32768 ctx, Q4_K_M 量化

### llama.cpp 源码克隆 + 编译
- 版本：b4927（shallow clone, --depth 1）
- 路径：`models/llama.cpp/`
- 编译：作为 CMake 子目录，仅构建库（不含 examples/tests/server）
- 链接：MinGW 静态链接 libllama.a

### 代码适配 llama.cpp b4927 API
- `llama_model_load_from_file()` — 加载 GGUF 模型
- `llama_tokenize()` — 两次调用模式（先取 count，再填充 buffer）
- `llama_decode()` + `llama_batch_get_one()` — 自回归生成
- `llama_sampler_chain_init()` + `llama_sampler_init_greedy()` — 贪婪采样
- `llama_sampler_sample()` — 采样下一个 token
- `llama_kv_self_clear()` — 清除 KV 缓存（替代废弃的 `llama_kv_cache_clear`）
- `ggml_backend_load_all()` — 加载计算后端

### CMake 更新
- llama.cpp 从本地 `models/llama.cpp` 以子目录方式引入
- 条件编译：`VIM_HAS_LLAMACPP=1`
- 若未 clone，提示手动命令

---

## 最终依赖状态

| 依赖 | 状态 |
|------|------|
| nlohmann/json v3.11.3 | ✓ |
| spdlog v1.14.1 | ✓ |
| Catch2 v3.7.0 | ✓ |
| SenseVoice 模型 (int8) | ✓ 229 MB |
| sherpa-onnx v1.13.2 SDK | ✓ |
| Qwen2.5-1.5B Q4_K_M GGUF | ✓ 1.1 GB |
| llama.cpp b4927 | ✓ 已编译链接 |
| WinHTTP | ✓ Windows 内置 |
| PortAudio | ✗ 待下载 |

---

## 构建验证

```
→ libvoice_input_method.a 编译通过 (含 llama.cpp)
→ voice_input_method_app.exe 编译通过
→ ctest: 23/23 passed
```

---

## 模型目录结构

```
models/
├── sensevoice/
│   ├── model.int8.onnx         229 MB  (ASR)
│   └── tokens.txt              309 KB
├── sherpa-onnx/
│   ├── include/sherpa-onnx/c-api/  v1.13.2
│   └── lib/                        DLLs
├── llama.cpp/                   b4927 源码
└── qwen2.5-1.5b-instruct-q4_k_m.gguf  1.1 GB  (LLM)
```

## 运行时 DLL 需求

```
models/sherpa-onnx/lib/sherpa-onnx-c-api.dll
models/sherpa-onnx/lib/onnxruntime.dll
```

llama.cpp 是静态链接，无需额外 DLL。
