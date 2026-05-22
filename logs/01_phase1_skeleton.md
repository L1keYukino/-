# 开发日志 #01 — 第1阶段：项目骨架

**日期**: 2026-05-23

---

## 做了什么

### 项目初始化
- 空仓库克隆，C++20 标准，CMake 4.2 + MinGW g++ 15.2.0 构建系统
- 纯标准库，零外部依赖。Phase 2 再接入 nlohmann/json / spdlog / Catch2
- .gitignore 配置（build/、models/、IDE文件等）

### 5 个核心抽象接口
- `IAudioCapture` — 音频采集（枚举设备、启动/停止、回调在独立音频线程）
- `IASREngine` — 语音识别（流式输入PCM、Partial/Final结果回调）
- `ILLMEngine` — LLM 文本处理（异步推理、流式token回调、KV缓存重置）
- `ITextOutput` — 文本输出（UTF-8模拟键盘输入、Unicode字符、特殊按键）
- `IHotkeyManager` — 全局热键（注册/注销、消息循环）

### 事件系统 + 观察者模式
- `IEngineObserver` — 5种回调：状态变化、音频电平、转写结果、LLM输出、错误
- 事件结构体：StateChangeEvent、AudioLevelEvent、TranscriptionEvent、LLMOutputEvent、ErrorEvent

### 状态机
- 7个状态：Idle → Listening → Recognizing → Correcting → Formatting → Outputting → Idle
- Error 为终端状态，只能 force_state 跳出
- 线程安全（atomic + CAS），非法跳转自动拒绝

### VoiceEngine 总调度器
- 持有所有子系统指针（构造时注入，或工厂方法创建）
- PTT 控制（ptt_press / ptt_release）
- 意图切换（set_intent）
- 观察者管理（add/remove observer，线程安全分发事件）

### Prompt 模板系统
- 7套模板：纠错(Error Correction) + 6种意图(General/Email/Chat/CodeComment/Documentation/Command)
- 每套包含 system prompt + few-shot examples
- build_messages() 自动拼接系统提示 + 示例 + 历史 + 用户输入

### 上下文管理器
- 环形缓冲，turn 数量上限 + token 估算上限双重裁剪
- add_turn / add_user / add_assistant 接口
- clear / set_max_turns / set_max_tokens 动态调整

### 配置系统
- 完整配置结构体树（EngineConfig → 各子系统Config）
- validate_config() — 参数合法性校验
- make_default_config() — 开箱即用默认值
- JSON 加载/保存预留接口（Phase 2 补 nlohmann/json 实现）
- default_config.json 模板文件

### 目录结构
```
voice_input_method/
├── CMakeLists.txt
├── cmake/                (CompilerWarnings, Find*.cmake)
├── src/
│   ├── core/             (engine, state_machine, events, types)
│   ├── audio/            (IAudioCapture)
│   ├── asr/              (IASREngine)
│   ├── llm/              (ILLMEngine, prompt_templates, context_manager)
│   ├── output/           (ITextOutput)
│   ├── hotkey/           (IHotkeyManager)
│   └── config/           (config 结构体 + 校验)
├── include/voice_input_method/  (公共API)
├── tests/unit/           (单元测试，Phase 2 激活)
├── config/               (default_config.json)
├── scripts/              (setup_deps.ps1, build.ps1)
└── models/               (.gitkeep)
```

---

## 构建验证

```
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
→ libvoice_input_method.a 编译通过
```

编译单元：
- `config.cpp` — 配置校验
- `state_machine.cpp` — 状态机逻辑
- `prompt_templates.cpp` — 模板初始化
- `engine.cpp` — VoiceEngine 骨架

---

## 已知限制

1. 外部依赖（nlohmann/json, spdlog, Catch2）因 GitHub 网络问题暂未接入，Phase 2 补齐
2. 所有后端实现为空壳（接口已定义，待实现）
3. 单元测试代码已写好，但编译开关暂时关闭
4. 构建脚本（build.ps1）依赖 CMake + MinGW，未覆盖 MSVC

---

## 下一步

第2阶段：接入 PortAudio 音频采集 + Sherpa-ONNX ASR 引擎，实现 PTT 模式端到端链路（录音→转写文本）
