# 语音输入法

基于 LLM 的智能语音输入法。SenseVoice 本地 ASR + DeepSeek 云端 LLM。

## 架构

```
麦克风 → PortAudio → SenseVoice ASR → DeepSeek LLM → SendInput → 目标窗口
                     本地离线识别           云端智能整理        键盘模拟
```

## 功能

- **按住说话**：自定义快捷键按住录音，松手输出
- **智能整理**：LLM 自动判断意图——短文本整理通顺，长内容自动总结，写邮件自动生成格式
- **音律条 UI**：9 个纯白圆点，常驻右下角，说话时跳动，可拖动
- **系统托盘**：右击菜单 → 设置（改快捷键、API Key、开关音律条）
- **自定义快捷键**：键盘 + 鼠标（侧键/中键/右键）组合

## 交互

| 操作 | 效果 |
|------|------|
| 按住快捷键 | 开始录音，白点跳动 |
| 松开 | 停止录音，文字自动输入 |
| 拖动音律条 | 调整位置 |
| 右键托盘 → 设置 | 改快捷键、API Key、开关音律条 |

## 构建

```powershell
# 1. 下载依赖
git clone --depth 1 --branch b4927 https://github.com/ggml-org/llama.cpp.git models/llama.cpp
git clone --depth 1 --branch v19.7.0 https://github.com/PortAudio/portaudio.git models/portaudio

# 2. 模型（放在 models/）
# SenseVoice: int8 模型 (229MB)
# Sherpa-ONNX SDK: v1.13.2 (18MB)

# 3. 构建
mkdir build && cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release

# 4. 复制 DLL 到 exe 同目录后运行
./src/voice_input_method_app.exe
```

## 依赖

| 组件 | 说明 |
|------|------|
| SenseVoice ONNX | 中文离线 ASR |
| Sherpa-ONNX v1.13.2 | 推理引擎 |
| DeepSeek API | 云端 LLM |
| PortAudio v19.7.0 | 音频采集 |
| GDI+ | Windows 内置，抗锯齿渲染 |

## 配置

右键托盘 → 设置，或在 `config/default_config.json` 中修改。
