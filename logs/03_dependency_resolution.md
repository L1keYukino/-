# 开发日志 #03 — 依赖解析 + 单元测试

**日期**: 2026-05-23

---

## 做了什么

### 三方库接入（CMake FetchContent）

趁 VPN 开启，通过 CMake FetchContent 从 GitHub 拉取了全部依赖：

| 库 | 版本 | 用途 |
|----|------|------|
| nlohmann/json | v3.11.3 | JSON 配置文件解析 |
| spdlog | v1.14.1 | 结构化日志 |
| Catch2 | v3.7.0 | 单元测试框架 |

### 代码升级

- `config.cpp` 恢复完整 nlohmann/json 实现（JSON 加载/保存/校验）
- `engine.cpp` 从 fprintf 迁移到 spdlog（`spdlog::info/debug/warn`）
- LLMFallbackConfig / ModeConfig 手动序列化（enum 成员无法用宏）
- `context_manager.hpp` 将 `estimated_tokens()` 改为 public（测试需要）

### 修复的问题

1. **LLMFallbackConfig 重复定义** — `NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE` 和手动 `to_json`/`from_json` 冲突，删除宏改为纯手动
2. **ADL 查找失败** — `static void to_json/from_json` 中 `static` 阻止了参数依赖查找，改为普通函数
3. **nlohmann brace-init 歧义** — `j = {{...}}` 对象初始化改用 `j["key"] = value` 逐个赋值
4. **Json 字段缺失** — `default_config.json` 中 `asr_fallback` 缺少 `endpoint_url` 字段
5. **测试文件路径** — 相对路径在 ctest 下失效，改用 `PROJECT_SOURCE_DIR` 编译期宏
6. **DLL 缺失 (0xc0000139)** — MinGW 需要 `-static` 标志静态链接运行时库
7. **ContextManager 访问权限** — `estimated_tokens()` 改为 public 供测试调用

### 单元测试结果

```
23/23 tests passed — 0 failures

ContextManager      7 tests  ✓ (增删改查, 裁剪, token估算)
PromptCatalog       6 tests  ✓ (模板完整性, 消息构建, few-shot)
StateMachine        5 tests  ✓ (合法/非法跳转, 终端状态, 强制覆盖)
Config              5 tests  ✓ (JSON解析, 校验, 往返, 意图解析)
```

---

## 构建命令

```powershell
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DVIM_USE_PORTAUDIO=OFF
cmake --build . --config Release
ctest --output-on-failure -C Release
```

编译产物：
- `libvoice_input_method.a` — 主库
- `test_voice_input.exe` — 测试可执行文件

---

## 当前依赖状态

| 依赖 | 状态 |
|------|------|
| nlohmann/json | ✓ 已接入 |
| spdlog | ✓ 已接入 |
| Catch2 | ✓ 已接入 |
| PortAudio | ✗ 未接入（待后续） |
| sherpa-onnx | ✗ 未接入（待后续） |

---

## 下一步

第3阶段：接入 llama.cpp + Qwen2.5-3B GGUF 模型，实现 LLM 纠错 + 意图格式化
