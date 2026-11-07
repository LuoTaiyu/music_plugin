# 阶段执行记录

> 只记录实际修改和实际运行结果。未做的验证不会写成通过。

## 阶段 1：材料审计、假设、风险、架构

**完成**
- 找到并完整阅读两份指南；复制到 `docs/reference/`。
- 搜索 `tools.zip`：未找到；在 `docs/MATERIALS_AUDIT.md` 标记 BLOCKED，不虚构审计。
- 写 `docs/ARCHITECTURE.md`、`docs/THEORY_AND_PSYCHOLOGY.md` 初版。
- 确定 C++20 + 自研 core/DSP + JUCE 插件外壳；AGPLv3 + JUCE 商业许可提示。

**修改**
- 建立 CMake 工程、目录结构、依赖策略。
- 记录 JUCE 8.0.6 版本与哈希。

**实际结果**
- 无代码可构建；阶段目标为审计与决策。
- 最大风险：缺少真实 DAW/听音、音色真实感、和弦局部锁、CPU。

## 阶段 2：最小插件与核心数据模型

**完成**
- `Project`、`Note`、`ChordEvent`、`Section`、`Phrase`、`Motif`、`Curves`、`AutomationLane`。
- JSON 读写、legacy `EC_SKETCH_0` 迁移、校验/修复。
- `TimeMap`（tick ↔ 秒，tempo map）。
- 轻量测试框架。

**构建/测试**
- CMake 核心 + `ec_tests` 编译通过。
- 早期 Debug 构建遇到布局缓存问题；clean rebuild 后解决。
- JSON/TimeMap/工程往返测试通过。

## 阶段 3：动机—和声—乐句—结构 + MIDI

**完成**
- `Theory`：音阶、和弦、voicing、平行五/八度惩罚、对位检查。
- `Motif`：原形/移调/倒影/逆行/扩大/缩小/片段/扩展/改结尾/装饰。
- `Composer`：三风格规则包、段落、乐句、旋律、低音、琶音/pad、对位、打击乐。
- `Humanizer`：乐句级力度、发音、微时序、音色联动。
- SMF 读写：tempo、拍号、markers、notes、CC；`ECP1` 完整工程负载；标准路径回退。
- MIDI 往返测试（含 overlap、微时序、坏文件、SMPTE 拒绝）。

**实际结果**
- 32 项测试中的 MIDI/乐理/生成部分通过。
- 发现并修复：声部进行产生平行五度 → 在 `voiceChord` 评分中加 400 惩罚；三套 seed 重新检查警告为 none。

## 阶段 4：情绪弧线 + 人性化 + 自然感音色

**完成**
- 四维曲线与 `ExpressionMapper`；段落目标从曲线采样。
- `SynthEngine`：Felt Piano（模态 + 锤击噪声 + 拉伸/失谐 + 释放）、Bowed String、Glass、Bass、Drums。
- `MasterBus`：EQ、压缩、FDN 混响、宽度、软限幅、DC 阻断。
- 离线 `Renderer` 与 WAV 16/24-bit。
- 采样率/block、NaN/Inf/声部压力、WAV 测试。

**实际结果**
- 32 项测试全部通过。
- 客观渲染：三套 demo 无削波、无 NaN/Inf；peak 0.556/0.646/0.961。

## 阶段 5：完整示范 + 可编辑 UI

**完成**
- 三套 36–48 小节示范，导出 JSON/MIDI/WAV/report。
- JUCE VST3 + Standalone；自绘编辑器：四曲线、段落时间线、锁、seed、variation、A/B、undo/redo、MIDI 导出。
- 后台生成线程；原子快照发布 + 5 秒 retired 回收。
- 宿主 PPQ/BPM 调度；内部试听；MIDI 输入试听。

**实际结果**
- 插件 Release 构建成功。
- 初次 Standalone 崩溃定位：`setSize()` 在子组件创建前触发 `resized()`；修复为末尾 setSize + null guard。
- UI 离屏快照 `docs/assets/editor.png` 实际生成。
- Standalone 启动/关闭 exit 0。

## 阶段 6：局部重生成、版本、宿主同步、兼容性

**完成**
- 6 个锁 + 范围合并 + variation + seed。
- undo/redo、A/B、项目 JSON 保存/加载。
- pluginval v1.0.4 strictness 5：SUCCESS（含 editor、state、automation、audio processing）。

**限制**
- 和弦没有真正按范围锁；文档明确。
- 插件不输出 MIDI 到宿主。
- 未在真实 DAW、AU、CLAP 验证。

## 阶段 7：性能、质量、文档收尾

**完成**
- `BUILD_REPORT.md`、`ARTISTIC_EVALUATION.md`、`USER_GUIDE.md`、`ROADMAP.md`、依赖许可。
- 客观音频分析 `docs/OBJECTIVE_AUDIO_ANALYSIS.md`。
- 清理调试日志/临时文件；最终 `dist/`。

**实际结果**
- 32/32 tests；pluginval SUCCESS；standalone exit 0。
- 本机生成 median 12.3/13.4/18.2 ms；离线 2.56–3.55× realtime。
- 未执行盲听/人评；艺术结论仍待验证。
