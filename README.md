# 语音输入法

基于 LLM 的智能语音输入法。本地 ASR + 本地/云端 LLM，语音转文字后自动整理输出。

## 架构

```
麦克风 → PortAudio → SenseVoice ASR → LLM(Qwen2.5-3B/DeepSeek) → SendInput → 目标窗口
                     本地离线识别           本地智能整理              键盘模拟
```

## 功能

- **按住说话**：`Ctrl+Alt+V` 按住录音，松手输出
- **智能整理**：LLM 自动判断意图——短文本整理通顺，长内容自动总结，写邮件自动生成格式
- **音律条 UI**：9 个纯白圆点，说话时向上拉成圆柱体（GDI+ 抗锯齿）
- **系统托盘**：灰色图标空闲，右击菜单
- **本地免费**：Qwen2.5-3B GGUF 本地运行，写邮件/总结完全够用

## 交互

| 操作 | 效果 |
|------|------|
| 按住 `Ctrl+Alt+V` | 开始录音，音律条出现 |
| 松手 | 停止录音，文字自动输入 |
| 右键音律条 | 退出 |

## 构建

```powershell
# 1. 下载依赖
git clone --depth 1 --branch b4927 https://github.com/ggml-org/llama.cpp.git models/llama.cpp
git clone --depth 1 --branch v19.7.0 https://github.com/PortAudio/portaudio.git models/portaudio

# 2. 模型（放在 models/）
# SenseVoice: sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17 (895MB)
# Sherpa-ONNX SDK: v1.13.2 win-x64-shared-MD-Release (18MB)
# Qwen2.5-3B GGUF: qwen2.5-3b-instruct-q4_k_m.gguf (1.95GB)

# 3. 构建
mkdir build && cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release

# 4. 运行
# 复制 DLL
cp models/sherpa-onnx/lib/sherpa-onnx-c-api.dll src/
cp models/sherpa-onnx/lib/onnxruntime.dll src/
./src/voice_input_method_app.exe
```

## 依赖

| 组件 | 说明 |
|------|------|
| SenseVoice ONNX | 中文离线 ASR |
| Sherpa-ONNX v1.13.2 | ASR 推理引擎 |
| Qwen2.5-3B GGUF | 本地 LLM，免费 |
| DeepSeek API | 云端 LLM（可选，更强） |
| PortAudio v19.7.0 | 音频 WASAPI |
| nlohmann/json + spdlog + Catch2 | 自动获取 |

## 配置

`config/default_config.json` 修改 LLM 模型路径和 API key。
