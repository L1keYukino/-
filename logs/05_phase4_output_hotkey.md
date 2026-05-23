# 开发日志 #05 — 第4阶段：文本输出 + 全局热键

**日期**: 2026-05-23

---

## 做了什么

### SendInput 键盘模拟 (`sendinput_output.hpp/.cpp`)
- UTF-8 文本 → Windows `SendInput` API 模拟键盘输入
- 内建 UTF-8 解码器（零外部依赖，纯 C++ 实现）
- BMP 字符：`KEYEVENTF_UNICODE` 直接输入
- 补充平面字符（>U+FFFF）：Alt+Numpad 十进制序列
- 特殊按键支持：Enter/Tab/Backspace/Escape/方向键/Home/End
- 可配置打字延迟（毫秒级）
- 非 Windows 平台 fallback stub

### 剪贴板备选方案 (`clipboard_output.hpp/.cpp`)
- UTF-8 → UTF-16 → 系统剪贴板 → Ctrl+V 模拟粘贴
- 适用场景：SendInput Unicode 兼容性有问题时降级使用
- `OpenClipboard` / `EmptyClipboard` / `SetClipboardData` / `SendInput(Ctrl+V)`

### Win32 全局热键 (`win32_hotkey.hpp/.cpp`)
- `RegisterHotKey` API 注册全局热键（即使窗口在后台也能捕获）
- 消息专用窗口（`HWND_MESSAGE`）：不占任务栏、不可见
- PTT 模式：热键按下 → 开始录音，松键 → 停止录音
- `GetAsyncKeyState` 轮询检测松键事件（RegisterHotKey 只通知按下）
- 完整 Win32 消息循环 + `PeekMessage` 无阻塞泵
- 默认热键：Ctrl+Alt+V

### 可执行入口 (`main.cpp`)
- `voice_input_method_app.exe` — 完整 CLI 入口
- 加载 JSON 配置文件（命令行参数可指定路径）
- 初始化引擎 → 注册热键 → 注册日志观察者 → 进入消息循环
- 信号处理（Ctrl+C 优雅退出）
- 观察者模式实时输出：状态变化、ASR 结果、LLM 输出

### CMake 更新
- 库目标：排除 main.cpp
- 可执行文件目标：`voice_input_method_app`（静态链接 -static）
- MinGW 自动链接 winhttp

---

## 构建产物

```
build/src/libvoice_input_method.a      # 静态库
build/src/voice_input_method_app.exe   # 可执行文件 (4.0 MB)
build/tests/test_voice_input.exe       # 测试可执行文件
```

---

## 新增文件

| 文件 | 说明 |
|------|------|
| `src/main.cpp` | CLI 入口 |
| `src/output/sendinput_output.hpp` | SendInput 头文件 |
| `src/output/sendinput_output.cpp` | UTF-8 → 键盘输入实现 |
| `src/output/clipboard_output.hpp` | 剪贴板头文件 |
| `src/output/clipboard_output.cpp` | 剪贴板 Ctrl+V 实现 |
| `src/hotkey/win32_hotkey.hpp` | Win32 热键头文件 |
| `src/hotkey/win32_hotkey.cpp` | RegisterHotKey + 消息窗口 |

---

## 构建验证

```
cmake --build . --config Release
→ libvoice_input_method.a 编译通过
→ voice_input_method_app.exe 编译通过 (4.0 MB)
→ ctest: 23/23 passed, 0 failed
```

---

## 当前状态总览

| Phase | 状态 | 关键产物 |
|-------|------|---------|
| Phase 1 ✓ | 骨架 + 接口 + 状态机 | 5个接口、7状态状态机、事件系统 |
| Phase 2 ✓ | 音频 + ASR | RingBuffer、PortAudio、Sherpa-ONNX 封装 |
| Phase 3 ✓ | LLM 引擎 | llama.cpp封装、OpenAI引擎、双pass管线 |
| Phase 4 ✓ | 输出 + 热键 | SendInput、剪贴板、Win32热键、可执行文件 |
| Phase 5 ✗ | 连续模式 + 降级 + 打磨 | 下一步 |

## 下一步

第5阶段（最终阶段）：VAD 连续听写模式、科大讯飞云端 ASR 降级、性能优化、综合错误处理
