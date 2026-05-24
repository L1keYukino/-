# 开发日志 #14 — LLM/纯转录模式切换

**日期**: 2026-05-24

---

## 做了什么

### 双模式切换
- Ctrl+Alt+B 切换 LLM 智能整理 ↔ 纯转录直出
- LLM 模式（白点）：ASR → DeepSeek → 智能输出
- 纯转录模式（蓝点）：ASR → 松手立即输出原文
- 蓝点/白点颜色区分当前模式

### 双快捷键自定义
- 设置对话框：录音快捷键 + 切换模式快捷键 分别录制
- 支持键盘+鼠标组合
- 保存到 config JSON

### 引擎 bypass
- VoiceEngine 新增 `llm_bypass_` 标志
- bypass 模式下跳过 LLM 直接输出 ASR 原文

## 修改文件

| 文件 | 变更 |
|------|------|
| `src/main.cpp` | 模式切换热键、纯转录输出、llm_bypass 控制 |
| `src/core/engine.hpp` | llm_bypass_ 成员 + set_llm_bypass() |
| `src/core/engine.cpp` | LLM bypass 路径：直接输出 raw ASR |
| `src/ui/overlay_window.hpp/cpp` | pure_mode_ 蓝点/白点颜色切换 |
| `src/ui/settings_dialog.cpp` | 重写：双热键录制器 + API Key + 音律条开关 |
| `src/config/config.hpp` | ModeConfig 新增 mode_hotkey |
