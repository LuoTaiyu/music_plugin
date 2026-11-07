# 架构 / Architecture

## 1. 目标与边界

Expressive Composer 是一个**以 MIDI/工程数据为核心**的作曲与演奏生成插件，不是端到端音频生成器。它：

- 生成可编辑的音符、和弦、乐句、段落、动机追踪、四条情绪曲线与自动化；
- 用程序化自然感合成器试听，支持离线 WAV 渲染；
- 通过锁与范围实现局部重生成；
- 提供 JSON 工程、SMF MIDI、WAV 导出；
- 可在 DAW 中作为 VST3 乐器运行，也可独立运行/使用 CLI。

它**不**声称：单个插件能读取 DAW 全部轨道、能做全局母带、能训练基础模型、能保证某种情绪。

## 2. 模块图

```
┌──────────────────────────────────────────────────────────────────┐
│  UI (JUCE)                                                       │
│  PluginEditor: emotion arcs / arrangement / locks / export       │
└───────────────────────────────┬──────────────────────────────────┘
                                │ message thread API
┌───────────────────────────────▼──────────────────────────────────┐
│  Plugin shell (JUCE)                                             │
│  PluginProcessor: host transport, state, APVTS, background gen   │
│  snapshot publication + retirement                               │
└───────────────┬───────────────────────────────┬──────────────────┘
                │ audio thread                  │ worker/message
┌───────────────▼───────────────┐   ┌───────────▼──────────────────┐
│  DSP / Renderer               │   │  Core                        │
│  SynthEngine (voices)         │   │  Composer / Expression       │
│  MasterBus (EQ/comp/reverb)   │   │  Theory / Motif / Humanizer  │
│  Offline Renderer             │   │  ProjectIO / TimeMap         │
└───────────────┬───────────────┘   └───────────┬──────────────────┘
                │                               │
┌───────────────▼───────────────────────────────▼──────────────────┐
│  I/O                                                            │
│  SMF MIDI read/write (tempo, meter, markers, notes, CC)          │
│  WAV 16/24-bit read/write                                       │
└──────────────────────────────────────────────────────────────────┘
```

依赖方向：`core` 不依赖 JUCE、不依赖 DSP；`dsp` 只依赖 `core` 的类型/常量；`io` 依赖 `core`；`render` 依赖 `core + dsp + io`；`plugin` 依赖 `render`。UI 不直接操作音频线程。

## 3. 目录

| 路径 | 责任 |
|---|---|
| `src/core/Types.*` | 音符、轨道、段落、乐句、和弦、曲线、工程模型 |
| `src/core/Expression.*` | 四维情绪状态、曲线、映射到音乐 affordances |
| `src/core/Theory.*` | 音阶、和弦、声部进行、对位检查 |
| `src/core/Motif.*` | 动机表示与变换 |
| `src/core/Composer.*` | 三风格规则包、生成、局部重生成 |
| `src/core/Humanizer.cpp` | 乐句级力度/发音/微时序/音色联动 |
| `src/core/ProjectIO.*` | JSON 序列化、版本迁移、校验 |
| `src/core/TimeMap.*` | tick ↔ 秒/bpm 积分 |
| `src/io/MidiFile.*` | SMF 读写、ECP1 工程负载、标准 MIDI 回退 |
| `src/io/WavWriter.*` | WAV 读写 |
| `src/dsp/Synth.*` | 5 类乐器 voice、声部管理 |
| `src/dsp/Effects.*` | FDN 混响、EQ、压缩、宽度、限幅、DC 阻断 |
| `src/render/Renderer.*` | 离线渲染、WAV 输出、统计 |
| `src/plugin/PluginProcessor.*` | 宿主同步、状态、后台生成、实时调度 |
| `src/plugin/PluginEditor.*` | 自绘 UI、曲线编辑、时间线、导出 |
| `src/cli/main.cpp` | 无 DAW 的生成/导出/审计入口 |
| `tests/` | 自动化测试与基准 |

## 4. 时间单位

严格区分：

| 单位 | 用途 |
|---|---|
| tick | 工程编辑、MIDI 导出；`ppq` 默认 960 |
| beat / quarter note | 情绪曲线横轴、段落位置、宿主 PPQ |
| bar | 段落/乐句、UI 选择范围 |
| second | 离线渲染、DSP hold/release |
| sample | 音频回调、事件偏移、WAV |
| millisecond | 微时序（人类演奏尺度） |

转换只在边界发生：

- `TimeMap` 负责 tick ↔ 秒，支持 tempo map 分段积分；
- MIDI 导出把 `microMs` 转为 tick 偏移；
- 插件把宿主 PPQ 转 sample offset，使用宿主 BPM；
- 内部试听使用工程 BPM。

## 5. 生成流水线

1. **Brief**：`Intent` + 四条曲线 + 风格 id + seed。
2. **Form**：风格包给出段落数、每段 bars、层级掩码、和声进行、配器模板。
3. **Harmony**：按段落构建 `ChordEvent`；`voiceChord` 最小化声部移动、限制间距、避免交叉。
4. **Motif**：选择基础动机，按段落/乐句变换；记录 `MotifUse`。
5. **Melody**：在乐句内重复/变奏动机，强拍拟合和弦音，弱拍允许经过/邻音，句尾设计解决或悬置。
6. **Accompaniment**：低音、琶音/pad、对位、打击乐按模板与层级掩码生成。
7. **Humanize**：乐句级力度拱形、发音、微时序、音色联动、声像；不是逐音随机。
8. **Metadata**：四条曲线导出为自动化 lane，每轨 reverb send 自动化点。
9. **Validate**：clamp/repair、排序、ID 唯一、时长。
10. **Snapshot**：插件把工程转成按 beat 排序的只读音符快照。

生成是确定性的：相同工程、seed、设置 → 相同 JSON/MIDI（除了 `composeMs` 等报告统计，不写入工程）。

## 6. 局部重生成与锁

`RegenerationOptions`：

```cpp
seed, locks, variation, rangeStartBar, rangeEndBar
```

- `locks` 是位掩码：Melody/Harmony/Rhythm/Orchestration/Humanization/Expression。
- 生成候选完整工程后，按范围合并：
  - 范围外：保留原音符；
  - 范围内：按锁决定保留还是替换；
  - Rhythm 锁：用原 onset/duration 对齐新音符；
  - Humanization 锁：复制原力度/亮度/微时序/发音/声像；
  - Orchestration 锁：保留原轨道名/乐器/路由，保留合并后的音符；
  - Expression 锁：保留曲线与段落目标。
- 全锁：返回不变副本并给出警告。
- 这是“保守合并”，不是无损的 DAW 级 undo 树；UI 还有工程级 undo/redo。

**重要限制**：局部重生成的和声候选仍可能改变范围边界外的和弦（因为候选整曲重新生成后再合并）。当前 P0 对和弦没有逐段锁；`Harmony` 锁会保留范围外的轨道音符，但 chords 仍来自候选。此行为已在路线图中标记为待改进，不应被解读为完全局部化和声。

## 7. 实时线程与快照

音频回调 `processBlock` 的约束：

- 不分配堆内存；
- 不做文件/网络/模型推理；
- 不等待长锁；
- 所有 voice/filter/delay buffer 在 `prepareToPlay` 预分配；
- Snapshot 通过 `std::atomic<const Snapshot*>` 发布；
- 旧 Snapshot 在 message thread 的 retired 列表中延迟 5 秒回收，避免音频线程还在读取时释放；
- 插件 UI 的工程数据修改在 message thread，生成任务在独立 `std::thread`。

潜在风险：

- `master_.setParams()` 每个 block 计算 Biquad 系数，可能包含 `cos/sin`；这不在音频回调中分配，但有 CPU 成本。P1 可以改为参数变化时计算。
- 宿主 block 大于 8192 samples 时直接返回静音（保护），应在兼容性测试中记录。
- 没有报告 lookahead 延迟，因为 limiter 是零延迟软限幅。

## 8. 宿主同步

- 读取 `AudioPlayHead::PositionInfo` 的 PPQ、BPM、isPlaying。
- 按 `[ppq, ppq + blockBeats)` 触发快照音符。
- 检测 transport jump/loop：向后跳或向前跳超过阈值时 `allNotesOff`。
- 宿主停止时释放音符，保留 release 尾音。
- 无宿主播放头时，UI 的 PLAY 使用内部试听时钟。
- MIDI 输入作为 live felt piano 试听层，不写入工程。
- **不输出 MIDI 到宿主**：`producesMidi()` 为 false。MIDI 交付通过 UI/CLI 导出文件完成。这是 P0 的明确限制。

## 9. 状态与版本

工程 JSON：

- `schemaVersion`（当前 1）；
- 版本迁移：`EC_SKETCH_0` legacy sketch → v1；
- 比当前新的 schema 被拒绝；
- 所有字段显式序列化，包括 seed、四条曲线、段落、轨道、和弦、乐句、动机使用、自动化；
- 插件状态：APVTS XML + `projectA`/`projectB` JSON + active variant；
- A/B：两个工程槽，切换时保存当前槽并安装快照；
- undo/redo：最多 40 个工程快照，message thread；redo 在 commit 时清空。

## 10. 导出

- **JSON**：可继续编辑的完整工程。
- **MIDI**：Type 1，PPQ 960；conductor 轨含 tempo、time signature、section markers、text；每轨含 program、CC7/10/91、note on/off、CC11/74/10；`ECP1` meta 保存完整工程 JSON。没有 ECP1 时也能标准导入。
- **WAV**：16/24-bit PCM，立体声，离线渲染，带 tail。
- **音频分轨**：P0 未实现单轨分轨导出；路线图 P1。

## 11. 依赖与许可

- JUCE 8.0.6：AGPLv3 或商业许可；本仓库以 AGPL-3.0 发布。
- VST3 SDK：随 JUCE，GPLv3/专有双许可；分发需遵守。
- clap-juce-extensions：MIT（当前可选，子模块未初始化，CLAP 构建未验证）。
- CLAP：MIT。
- 无采样库、无字体文件、无云模型。
- 详见 `DEPENDENCIES_AND_LICENSES.md`。

## 12. 扩展接口

- **风格规则包**：新增 `StylePlan`（段落、和声、轨道模板、变换集）即可；建议做成数据文件/注册表，当前在 `Composer.cpp` 内编译期实现。
- **理论/音律**：`Scale`/`Mode` 目前 12-TET；`Theory.hpp` 可扩展为任意音分集合。
- **AI 适配器**：未实现。设计上放在 `core` 外的 `GenerationProvider`，只接收/返回 `Project` 或动机/和弦约束，密钥不写入工程；必须由用户显式开启。
- **采样乐器**：`SynthEngine` 的 `Instrument` 枚举可增加 sample player voice；采样来源/许可必须记录。
- **分轨导出**：`Renderer` 已能按轨过滤，扩展为 per-track WAV 不需要改核心模型。
- **MPE/CC 自动化**：当前仅 MIDI 文件 CC；实时 MPE 未实现。

## 13. 已知限制（必须与代码一起阅读）

1. AU 未在 Windows 构建/验证；CLAP 未构建。
2. 插件不向宿主输出 MIDI。
3. 单插件不做跨轨分析/母带。
4. 和弦没有局部锁定；局部重生成的和声可能越界。
5. 没有真实采样；声音是程序化合成。
6. 没有进行人工听音/盲测；所有“音质”表述仅为设计/客观测量。
7. 生成规则包是编译期 C++；没有用户可编辑的风格 DSL。
8. UI 的曲线编辑目前保存曲线但不自动重生成；需要按 APPLY ARCS。
9. WAV 在插件 UI 中未提供导出按钮（CLI 提供）；`exportWavFile` API 存在但 UI 未接线，且会阻塞调用线程。
10. 宿主兼容性尚未在真实 DAW 中验证。
