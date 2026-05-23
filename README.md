# 语音输入法

基于 LLM 的智能语音输入法。本地 ASR + 云端 LLM，语音转文字后自动整理输出。

## 架构

```
麦克风 → PortAudio (WASAPI) → SenseVoice ASR → DeepSeek LLM → SendInput → 目标窗口
                                  ↑ 本地离线识别        ↑ 云端智能整理
```

## 功能

- **语音转录**：说话 → 文字输出到光标位置
- **智能整理**：LLM 自动判断意图——短文本整理通顺，长内容自动总结为清单，写邮件自动生成完整格式
- **系统托盘**：灰色图标空闲，红色录音中，右击菜单
- **悬浮提示窗**：录音时显示状态，结束时显示结果
- **全局热键**：`Ctrl+Alt+V` 切换录音

## 使用

```
# PTT 模式（默认）
voice_input_method_app.exe

# 加载指定配置
voice_input_method_app.exe --config my_config.json
```

1. 启动后系统托盘出现灰色图标
2. `Ctrl+Alt+V` 开始录音，图标变红，右下角弹出提示窗
3. 说话
4. 再次 `Ctrl+Alt+V` 结束录音
5. 文字自动输入到光标所在位置

## 依赖

| 组件 | 说明 |
|------|------|
| SenseVoice ONNX | 中文离线 ASR 模型 (895 MB, 需下载到 models/) |
| Sherpa-ONNX SDK v1.13.2 | ASR 推理引擎 (需下载到 models/) |
| DeepSeek API | 云端 LLM，文本整理 (>1.5B 本地模型不够聪明) |
| llama.cpp + Qwen2.5-1.5B | 本地 LLM，纠错用 (可选，1.1 GB) |
| PortAudio v19.7.0 | 音频采集 (WASAPI) |
| nlohmann/json + spdlog + Catch2 | 自动获取 (CMake FetchContent) |

## 构建

```powershell
# 1. 下载模型和 SDK
git clone --depth 1 --branch b4927 https://github.com/ggml-org/llama.cpp.git models/llama.cpp
git clone --depth 1 --branch v19.7.0 https://github.com/PortAudio/portaudio.git models/portaudio

# SenseVoice 模型 (895 MB)
curl -L -o models/sensevoice.tar.bz2 "https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17.tar.bz2"
# 解压到 models/sensevoice/

# Sherpa-ONNX SDK (18 MB)
curl -L -o models/sherpa-onnx.tar.bz2 "https://github.com/k2-fsa/sherpa-onnx/releases/download/v1.13.2/sherpa-onnx-v1.13.2-win-x64-shared-MD-Release.tar.bz2"
# 解压到 models/sherpa-onnx/

# Qwen2.5-1.5B GGUF (1.1 GB, 可选)
curl -L -o models/qwen2.5-1.5b-instruct-q4_k_m.gguf "https://hf-mirror.com/Qwen/Qwen2.5-1.5B-Instruct-GGUF/resolve/main/qwen2.5-1.5b-instruct-q4_k_m.gguf"

# 2. 配置 DeepSeek API Key
# 编辑 config/default_config.json，填写 llm_fallback.api_key

# 3. 构建
mkdir build && cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release

# 4. 运行
# 先复制 DLL
cp models/sherpa-onnx/lib/sherpa-onnx-c-api.dll src/
cp models/sherpa-onnx/lib/onnxruntime.dll src/
cp models/sherpa-onnx/lib/onnxruntime_providers_shared.dll src/

./src/voice_input_method_app.exe
```

## 项目结构

```
src/
├── core/       engine.cpp       总调度 (状态机、管线)
├── audio/      portaudio_capture PortAudio 录音、环形缓冲、VAD
├── asr/        sherpa_onnx_asr   SenseVoice 离线识别 (Sherpa-ONNX)
│               iflytek_cloud_asr 科大讯飞云端 ASR (备用)
│               asr_fallback      ASR 降级组合器
├── llm/        llamacpp_engine   llama.cpp 本地推理
│               openai_engine     DeepSeek/OpenAI HTTP API
│               prompt_templates  Prompt 模板库
│               context_manager   多轮上下文管理
├── output/     sendinput_output  SendInput 键盘模拟
│               clipboard_output  剪贴板 Ctrl+V 备用
├── hotkey/     win32_hotkey      Win32 全局热键
├── ui/         tray_icon         系统托盘图标
│               overlay_window    悬浮状态窗口
└── config/     config.cpp        JSON 配置加载
```

## 配置

`config/default_config.json`：

```json
{
  "audio": { "device_id": "", "sample_rate": 44100 },
  "llm_fallback": {
    "type": "openai",
    "api_key": "sk-xxx",
    "model": "deepseek-chat",
    "endpoint_url": "https://api.deepseek.com/v1",
    "enabled": true
  }
}
```

## 测试

```powershell
cmake --build . --config Release
ctest -C Release
# 23 tests: ContextManager(7), PromptCatalog(6), StateMachine(5), Config(5)
```
