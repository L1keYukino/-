# 语音输入法

基于 LLM 的智能语音输入法。SenseVoice 本地 ASR + DeepSeek 云端 LLM。

## 架构

```
麦克风 → PortAudio → SenseVoice ASR → DeepSeek LLM → SendInput → 目标窗口
                     本地离线识别           云端智能整理        键盘模拟
```

## 功能

- 按住说话：自定义快捷键按住录音，松手输出
- 智能整理：LLM 自动判断意图（短文本整理 / 长内容总结 / 写邮件自动生成格式）
- 音律条：9 个纯白圆点常驻右下角，说话时跳动，可拖动
- 系统托盘：右击菜单 → 设置（改快捷键、API Key、开关音律条）
- 快捷键支持键盘 + 鼠标组合（含侧键）

## 交互

| 操作 | 效果 |
|------|------|
| 按住快捷键 | 开始录音，白点跳动 |
| 松开 | 文字自动输入到光标位置 |
| 拖动音律条 | 调整位置 |
| 右键托盘 → 设置 | 改快捷键、API Key、开关音律条 |

## 构建

需要 MinGW g++ / CMake 4.x / Python 3（辅助脚本）。

```powershell
# 下载源码依赖
git clone --depth 1 --branch b4927 https://github.com/ggml-org/llama.cpp.git models/llama.cpp
git clone --depth 1 --branch v19.7.0 https://github.com/PortAudio/portaudio.git models/portaudio

# 下载模型（放在 models/）
#   SenseVoice int8: sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17.tar.bz2 → models/sensevoice/
#   Sherpa-ONNX SDK v1.13.2: sherpa-onnx-v1.13.2-win-x64-shared-MD-Release.tar.bz2 → models/sherpa-onnx/

# 构建
mkdir build && cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release

# 运行前复制 DLL 到 exe 同目录
cp ../models/sherpa-onnx/lib/sherpa-onnx-c-api.dll src/
cp ../models/sherpa-onnx/lib/onnxruntime.dll src/
cp ../models/sherpa-onnx/lib/onnxruntime_providers_shared.dll src/

# 运行
./src/voice_input_method_app.exe
```

## 依赖

| 组件 | 版本 | 说明 |
|------|------|------|
| SenseVoice ONNX | int8 | 中文离线 ASR (229MB) |
| Sherpa-ONNX | v1.13.2 | ASR 推理引擎 (18MB) |
| DeepSeek | API | 云端 LLM 文本整理 |
| PortAudio | v19.7.0 | WASAPI 音频采集 |
| llama.cpp | b4927 | 本地 LLM（可选，纠错用） |
| GDI+ | 系统内置 | 抗锯齿渲染音律条 |
| nlohmann/json spdlog Catch2 | 自动获取 | JSON/日志/测试 |

## 配置

右键托盘 → 设置，或在 `config/default_config.json` 中修改。

## 测试

```powershell
ctest -C Release   # 23/23 单元测试
```
