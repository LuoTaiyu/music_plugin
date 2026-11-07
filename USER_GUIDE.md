# 用户指南 / User Guide

本指南描述 **0.1.0 P0** 的真实可用工作流。未实现的功能会明确标记。

---

## 1. 三个入口

### A. DAW 插件（VST3 / Standalone）

- 插件是**乐器**（Instrument），输出音频，接受 MIDI 输入用于试听。
- 宿主提供 tempo、拍号、播放位置时，按宿主 PPQ 同步；停止时停止调度。
- 插件不向宿主发送 MIDI；要拿到 MIDI，使用“MIDI”按钮导出 `.mid`。

### B. CLI（无需 DAW）

`ec_cli` 用于生成、导出、检查、渲染和基准。示例见 `README.md`。

### C. JSON 工程

所有示范都是普通 JSON，可以版本管理、diff、脚本处理；字段包括 notes、chords、phrases、motifs、curves、automation。

---

## 2. 从模糊情绪开始（5 分钟）

### 第一步：选一个起点

在插件顶部选择：

- **CHAMBER**：克制、怀念的室内器乐；
- **CINEMATIC**：紧张逐步累积的电影配乐；
- **KINETIC**：节奏驱动的电子律动。

这会生成一个完整 36–48 小节的方案，包含旋律、和声、低音、对位/纹理、打击乐（按风格）和段落。

### 第二步：读“CREATIVE BRIEF”

左侧显示：

- 表达视角（独白/对话/旁观者/抽象氛围）；
- 场景（亲密房间/开放空间/舞台/抽象内心）；
- 克制程度；
- 调性、拍号、BPM。

这些不是 UI 标签：它们会改变配器、空间、句法与演奏生成。要修改它们，当前 P0 需要编辑生成规格/JSON；UI 只读显示。这是明确限制。

### 第三步：改情绪弧线

中央上方是四条曲线：

| 曲线 | 含义 | 编辑方式 |
|---|---|---|
| VALENCE | 阴暗 ↔ 明亮 | 拖动点 |
| AROUSAL | 平静 ↔ 高能量 | 拖动点 |
| TENSION | 松弛 ↔ 悬置/压迫 | 拖动点 |
| DISTANCE | 亲密 ↔ 遥远/宏大 | 拖动点 |

操作：

- **拖动**：改变已有控制点；
- **双击空白**：添加控制点；
- **右键点**：删除控制点（至少保留 2 个）。

曲线使用平滑插值。编辑后按 **APPLY ARCS** 才会重新作曲；仅编辑曲线不会自动改变已有音符。

**注意**：曲线不是“情绪保证”。它只改变可解释的音乐参数（密度、和声张力、音区、空间等）。

### 第四步：选择局部重生成范围

在 **ARRANGEMENT** 时间线点击一个段落。左侧 **REGENERATION RANGE** 会显示该段。选择 “Whole piece” 则整曲重生成。

### 第五步：锁定你喜欢的部分

六个锁：

| 锁 | 锁定内容 |
|---|---|
| MELODY | 旋律音高/时间/力度 |
| HARMONY | 和声/低音/纹理轨的音符 |
| RHYTHM | 新音符沿用原 onset/duration |
| ORCH. | 轨道名、乐器、路由、增益/声像/送量 |
| HUMAN. | 力度、亮度、微时序、发音、声像 |
| ARCS | 四条曲线与段落目标 |

默认 MELODY + HARMONY 打开，避免一键重生成把整首改掉。

### 第六步：REGENERATE

- 只重生成选中范围；
- 范围内按锁合并；
- 范围外尽量保留；
- 生成在后台线程；旧版本继续播放直到新快照安装；
- 完成后可 **UNDO**。

### 第七步：试听

- **PLAY**：内部试听（无宿主或宿主停止时）；
- 宿主播放时自动使用宿主 PPQ/BPM；
- 关闭混响（把 SPACE 拉到 0）检查主体是否仍成立；
- 用 A/B 比较两版。

### 第八步：导出

- **MIDI**：导出 `.mid`，包含 tempo、拍号、段落 markers、轨道、音符、部分 CC；
- **SAVE**：导出完整 `.json` 工程；
- **AUDIO**：当前插件 UI 未接线；用 CLI `render` 生成 WAV（见 README）。

---

## 3. 插件 UI 控件速查

| 控件 | 作用 |
|---|---|
| CHAMBER / CINEMATIC / KINETIC | 生成对应风格完整方案 |
| NEW VARIATION | 用当前风格 + seed 重新生成 |
| REGENERATE | 按锁与范围重生成 |
| APPLY ARCS | 用当前曲线重新作曲 |
| HUMANIZE | 仅重新执行乐句级演奏处理（会叠加在当前结果上，建议少用） |
| PLAY/STOP | 内部试听 |
| A/B | 两个工程槽切换 |
| UNDO / REDO | 工程级历史（最多 40 步） |
| MASTER | 主增益（内部有 +18 dB 校准） |
| SPACE | 混响返回量；不是“情绪”旋钮 |
| WIDTH | 中侧宽度 |
| HUMANIZE DEPTH | HUMANIZE 按钮的强度 |
| SEED | 生成随机种子；相同工程/设置/种子可复现 |
| VARIATION | 局部重生成的变化幅度（0..1） |

### 颜色与含义

- 金色：主操作/当前选择/曲线 VALENCE；
- 青色：AROUSAL/次要强调；
- 红色：TENSION；
- 蓝色：DISTANCE；
- 深灰面板：非激活区域。

---

## 4. 局部重生成的正确预期

### 会保留

- 范围外音符（尽量）；
- 锁定维度的音符/演奏参数；
- Orchestration 锁下的轨道结构；
- Expression 锁下的曲线与段落目标。

### 可能改变

- **和弦**：当前 `chords` 没有按范围锁；候选工程的 chords 可能影响范围外和声。这是 P0 已知限制。
- 乐句编号与 `motifUses`：候选工程重新生成后，编号可能变化。
- 未锁定轨道的音符。

### 不会发生

- 不会修改你的 JSON 文件本身（只有 SAVE 才写文件）；
- 不会把 Python/云模型带入音频线程；
- 不会在未提示的情况下联网。

---

## 5. MIDI 导出与回导

### 导出

- Type 1 SMF，PPQ 960；
- conductor 轨：tempo、拍号、section markers、标题/说明；
- 每轨：track name、GM program、CC7/10/91、note on/off、CC11/74/10；
- `ECP1` meta：完整工程 JSON（用于本系统精确往返）；
- 没有 `ECP1` 时，标准 MIDI 仍可被其他 DAW 读取。

### 回导

- 默认优先 `ECP1` 工程负载，并交叉检查标准 MIDI 的音符数量；
- 若没有负载或数量不符，按标准 MIDI 解析；
- 标准解析会恢复：tempo map、拍号、markers→段落、轨道名、音符、力度、部分 CC；
- **不会**推断调性、和弦符号、动机；这些字段保持为空/默认，并在 warnings 中说明。

### 已知边界

- 标准 MIDI 的 marker 只能恢复段落起点，不能恢复和声/句法；
- 同音高重叠音符使用 FIFO 配对；本系统导出保证 off 在下一个 on 前；
- MIDI 不支持负时间的微时序；导出时会把 `microMs` 转为 tick 偏移并 clamp 到 0。

---

## 6. 在 DAW 中使用

1. 复制 `.vst3` 到 VST3 目录；
2. 在 DAW 中扫描插件；
3. 插入到 Instrument 轨；
4. 设置宿主 tempo（例如 68/92/122）；
5. 按宿主播放，插件按 PPQ 调度；
6. 若宿主不提供 PPQ，使用插件 PLAY 内部试听；
7. 导出 MIDI 到 DAW 进一步编辑；
8. 导出 JSON 保存完整工程。

**未验证**：本机没有实际 DAW 兼容性测试。不同宿主的 PPQ/播放头行为、离线渲染、状态恢复可能不同。请把问题记录到 issue。

---

## 7. 命令行工作流

### 生成并检查

```powershell
.\build-core\ec_cli.exe generate --demo all --seed 20261108 --out presets/demos
.\build-core\ec_cli.exe inspect presets/demos/cinematic/project.json
.\build-core\ec_cli.exe validate presets/demos/kinetic/project.json
```

### 渲染

```powershell
.\build-core\ec_cli.exe render presets/demos/kinetic/project.json `
  --wav presets/demos/kinetic/kinetic.wav --sr 48000 --block 512 --bits 24 --tail 3
```

### MIDI

```powershell
.\build-core\ec_cli.exe export-midi presets/demos/chamber/project.json --out out.mid
.\build-core\ec_cli.exe import-midi out.mid --out out.json --standard
```

### 复现性

同一 seed + 同一工程/设置，输出 JSON 应逐字节一致（除运行报告）。可用：

```powershell
.\build-core\ec_cli.exe generate --demo chamber --seed 42 --no-wav --out A
.\build-core\ec_cli.exe generate --demo chamber --seed 42 --no-wav --out B
fc /b A\chamber\project.json B\chamber\project.json
```

---

## 7.5 高 DPI / 小屏

编辑器设计尺寸为 1200×800 逻辑像素。在 200% Windows 缩放且逻辑桌面较小的机器上，窗口可能超出屏幕。这是 P0 已知限制；可尝试：

- 在 DAW 中把插件编辑器缩放设为 75–100%；
- 使用更大分辨率/较低缩放的显示器；
- 用 CLI 完成生成/导出（不依赖 GUI 尺寸）。

## 8. 常见问题

### Q: 为什么生成的音频很安静？

A: 渲染链内部有 +18 dB 校准，但不同风格/段落的峰值不同。用 MASTER 或 `--gain` 调整；限幅器会保护。

### Q: 为什么改曲线没有马上变化？

A: 曲线是工程元数据；按 **APPLY ARCS** 重新生成。这样避免拖动时不断重写作曲。

### Q: 为什么 REGENERATE 改了整首？

A: 先检查 REGENERATION RANGE 是否选中了段落；Whole piece 会整曲重生成。再用锁保护。

### Q: 为什么和弦没有被局部锁？

A: P0 已知限制。`Harmony` 锁保护和声轨音符，但 `chords` 序列不是按范围独立生成的。见 ROADMAP。

### Q: 为什么插件不输出 MIDI？

A: 当前版本设计为音频乐器 + MIDI 文件导出；`producesMidi()` 为 false。MIDI Out 是 P1。

### Q: 为什么构建插件失败，提示 juceaide？

A: 路径含中文。使用 `scripts/build.ps1`，它会在 `D:\ecwork` 建 junction；或把源码放到 ASCII 路径。

### Q: 为什么没有 AU/CLAP？

A: AU 需要 macOS + Xcode；CLAP 需要初始化 `clap-juce-extensions` 子模块并开启选项。当前 Windows 构建只验证 VST3/Standalone。

### Q: 有没有试听/盲测？

A: 没有。所有音质相关表述只到客观测量与设计审查；不声称“听感已验证”。
