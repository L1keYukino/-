# 语音输入法

基于 LLM 的智能语音输入法。SenseVoice 本地 ASR + DeepSeek 云端 LLM，支持双模式切换。

## 双模式

| 模式 | 音律条颜色 | 快捷键 | 行为 |
|------|-----------|--------|------|
| LLM 智能整理 | 白色圆点 | 默认开启 | ASR → DeepSeek 理解意图 → 智能输出（写邮件/总结/整理） |
| 纯转录 | 蓝色圆点 | Ctrl+Alt+B 切换 | ASR → 松手立即输出原文，不经过 LLM |

两种模式的录音快捷键可分别自定义，支持键盘+鼠标组合。

## 架构

```
麦克风 → PortAudio → SenseVoice ASR ─┬→ DeepSeek LLM → SendInput（LLM 模式）
                                       └→ SendInput（纯转录模式，即时输出）
```

## 功能

- 双模式一键切换：LLM 智能整理 / 纯转录直出
- 按住说话：自定义快捷键按住录音，松手输出
- LLM 模式：自动判断意图（短文本整理 / 长内容总结 / 写邮件生成格式）
- 纯转录模式：松手即时输出，适合快速输入
- 音律条：9 个圆点常驻右下角，拖动调整位置，设置可关闭
- 系统托盘：右击菜单 → 设置（改快捷键、API Key、麦克风、开关音律条）

## 交互

| 操作 | 效果 |
|------|------|
| 按住录音快捷键 | 开始录音，圆点跳动 |
| 松开 | 文字自动输入（LLM 模式需等云端处理） |
| Ctrl+Alt+B | 切换 LLM / 纯转录模式 |
| 拖动音律条 | 调整位置 |
| 右键托盘 → 设置 | 改快捷键、API Key、麦克风、开关音律条 |

## 构建

需要 MinGW g++ / CMake 4.x。

```powershell
# 下载源码依赖
git clone --depth 1 --branch b4927 https://github.com/ggml-org/llama.cpp.git models/llama.cpp
git clone --depth 1 --branch v19.7.0 https://github.com/PortAudio/portaudio.git models/portaudio

# 下载模型
#   SenseVoice int8 → models/sensevoice/
#   Sherpa-ONNX SDK v1.13.2 → models/sherpa-onnx/

# 构建
mkdir build && cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release

# 复制 DLL
cp ../models/sherpa-onnx/lib/*.dll src/
./src/voice_input_method_app.exe
```

## 配置

右键托盘 → 设置，或在 `config/default_config.json` 中修改。
