# 开发日志 #13 — 设置对话框、热键录制、音律条常驻、云LLM

**日期**: 2026-05-24

---

## 做了什么

### 设置对话框
- API Key 输入框（DeepSeek/OpenAI 兼容）
- 热键录制器：点击按钮 → 按下组合键 → 自动捕获
  - 支持键盘 (Ctrl/Alt/Shift/Win + 任意键)
  - 支持鼠标 (侧键/中键/右键)
  - 过滤纯修饰键、5秒超时保护
- 音律条开关（勾选=显示，取消=隐藏）
- 保存写入 config JSON

### 热键改为轮询
- 移除 RegisterHotKey（不支持鼠标按键）
- WM_TIMER 30ms 轮询 GetAsyncKeyState
- 同时检测键盘和鼠标状态
- 设置里改快捷键立即生效，无需重启

### 音律条常驻
- 启动即显示 9 个白点（空闲态）
- 录音时白点根据音量跳动画
- 松手后回到空闲态（不消失）
- 鼠标左键按住拖动调整位置
- 设置里可关闭

### 云 LLM
- DeepSeek API 默认启用
- llama.cpp ChatML 格式修复：`<|im_start|>/<|im_end|>` 替代错误标签
- 上下文 2048，速度优化

### 麦克风
- 默认设备改为 HECATE G6 PRO 48kHz mono (device 58)

---

## 修改文件

| 文件 | 变更 |
|------|------|
| `src/ui/settings_dialog.cpp` | 重写：API Key + 热键录制 + 音律条开关 |
| `src/main.cpp` | 轮询热键、音律条常驻、show_viz 配置 |
| `src/ui/overlay_window.cpp` | 拖动支持、空闲态绘制、去掉 WS_EX_TRANSPARENT |
| `src/llm/llamacpp_engine.cpp` | ChatML 格式修复 |
| `config/default_config.json` | 云 LLM 启用、麦克风选 device 58 |
