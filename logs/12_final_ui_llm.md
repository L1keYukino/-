# 开发日志 #12 — 最终版：GDI+ 音律条 + 3B 模型

**日期**: 2026-05-24

---

## UI：9 点音频可视化

- GDI+ 抗锯齿渲染，纯白圆点
- 空闲：9 个圆点位于底部基线
- 录音：圆点根据音量向上拉伸成圆柱体（矩形 + 半圆头尾）
- 每个圆点独立灵敏度，随机抖动，无固定模式
- 双缓冲绘制，无闪烁
- 按住快捷键显示，松手消失

## LLM 升级

- 从 Qwen2.5-1.5B 升级到 Qwen2.5-3B Instruct Q4_K_M
- 2GB，本地运行，完全免费
- 写邮件、总结会议足够智能

## 架构简化

- 删除 Python UI（voice_input_ui.py）
- 删除 DLL 桥接方案
- 保留纯 C++ Win32 应用（voice_input_method_app.exe）
- 无需外部运行时依赖（静态链接）

## 修改文件

| 文件 | 变更 |
|------|------|
| `src/ui/overlay_window.hpp/.cpp` | 重写：GDI+ 9 点音律条 |
| `src/main.cpp` | 简化 observer：按显松隐 |
| `src/core/engine.cpp` | 移除门控 |
| `src/asr/sherpa_onnx_asr.cpp` | 切回 int8 模型 |
| `config/default_config.json` | 3B 模型路径、4096 上下文 |
| `voice_input_ui.py` | **已删除** |
