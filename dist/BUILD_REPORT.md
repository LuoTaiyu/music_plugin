# 构建与验证报告

日期：2026-11-08（Asia/Hong_Kong）
工作区：`D:\文档\dsh_music`
构建路径：核心 `build-core/`；插件 `build-plugin-ascii/`（通过 `D:\ecwork` junction，避免 JUCE 中文路径问题）

> 所有数字为本机实测。它们不是跨机器承诺，也不是听感评价。

## 1. 环境

| 项目 | 值 |
|---|---|
| OS | Windows 11 / NT 10.0.22621 |
| CPU | Intel Core i5-12500H，12 cores / 16 logical processors |
| Compiler | MSVC 19.44.35222.0（VS 2022 Build Tools 14.44.35207） |
| CMake | 4.4.4 |
| Ninja | 1.13.2 |
| JUCE | 8.0.6（SHA-256 `95752f8e8b37d802b4c10182bc757de7d88f0c8899e879afa8798589be306a11`） |
| Display | 1440×900，200% Windows scaling |
| DAW | 未安装/未测试 |

## 2. 构建命令

### 核心 + CLI + 测试

```powershell
cmake -S . -B build-core -G Ninja -DCMAKE_BUILD_TYPE=Release `
  -DEC_BUILD_PLUGIN=OFF -DEC_BUILD_TESTS=ON -DEC_BUILD_BENCH=ON
cmake --build build-core --parallel
```

### 插件（ASCII 路径）

```powershell
# D:\ecwork 是指向工作区的 junction；scripts/build.ps1 会自动创建
cmake -S D:\ecwork -B D:\ecwork\build-plugin-ascii -G Ninja -DCMAKE_BUILD_TYPE=Release `
  -DEC_BUILD_PLUGIN=ON -DEC_BUILD_CLAP=OFF -DEC_BUILD_TESTS=OFF
cmake --build D:\ecwork\build-plugin-ascii --parallel 4
```

结果：**成功**。

## 3. 自动化测试

`ec_tests.exe`：**32 passed, 0 failed**（Release 构建）。覆盖：

- `TimeMap` 分段 tempo；
- 四曲线插值/边界；
- F 小调音阶、和弦、voicing、声部交叉/间距；
- 表达映射的所有轴；
- 动机原形/倒影/逆行/扩大/缩小；
- 三示范结构、音符/和弦/乐句/动机使用数量；
- 同 seed JSON 完全一致；不同 seed/风格不同；
- Humanizer 范围与分组；
- 全锁不变、局部锁保留旋律、范围外保留；
- JSON parse 错误、Unicode/转义；
- 工程 JSON 全字段往返；
- legacy `EC_SKETCH_0` 迁移；
- 新 schema 拒绝；非法音符 clamp；
- 7/8、5/4 的小节计算；
- MIDI ECP1 精确往返、标准 MIDI 往返（tempo/拍号/marker/overlap/微时序）、坏文件、SMPTE 拒绝；
- Felt Piano 有限值/力度改变频谱、96 声部压力/NaN/Inf/极端参数；
- 44.1/48/96 kHz × 64/257/1024 block；
- 输出立体声、无 DC；
- WAV 16/24-bit 往返；
- 实际写 WAV 文件。

最终命令：`.\build-core\ec_tests.exe`
输出尾行：`32 passed, 0 failed, 32 total`

## 4. 插件兼容性验证

### 4.1 pluginval

- 工具：pluginval v1.0.4（JUCE 8.0.3 构建）
- 插件：`build-plugin-ascii/src/plugin/ExpressiveComposer_artefacts/Release/VST3/Expressive Composer.vst3`
- 参数：strictness 5；sample rates 44100/48000；block sizes 64/512/1024；含 GUI 测试
- 结果：**SUCCESS**
- 通过的测试阶段：scan、cold/warm open、plugin info、programs、editor、open editor whilst processing、audio processing、state、automation、editor automation、automatable parameters、basic bus、bus layout。
- VST3 SDK 自带 validator 未运行：pluginval 未配置 validator 路径（日志：`Skipping vst3 validator as validator path hasn't been set`）。
- 日志：`build-plugin-ascii/pluginval.log`（末尾 `SUCCESS`；pluginval 之后打印 `can't open file for write !`，但进程 exit code 0 且测试判定成功）。

### 4.2 Standalone

- 启动：成功，窗口标题 `Expressive Composer`。
- 编辑器实际创建并绘制（离屏快照 `docs/assets/editor.png`）。
- `CloseMainWindow()` 正常退出：exit code **0**。
- 空闲/编辑器打开后内存（3–4 秒）：Working set 约 **91.7–139.2 MB**（多次启动测量）；19 threads。
- 报告延迟 0 samples；tail 4.0 s（pluginval 读取）。

### 4.3 未验证

- 真实 DAW 中的 VST3 扫描/加载/离线渲染/状态恢复；
- AU（需要 macOS/Xcode/auval）；
- CLAP（子模块未初始化，未构建）；
- VST3 SDK 官方 validator；
- 32-bit、Linux、Windows ARM。

## 5. 性能（本机实测）

### 5.1 生成（`ec_cli bench --iterations 5`）

| 风格 | 音符 | min | median | max |
|---|---:|---:|---:|---:|
| Chamber | 746 | 7.680 ms | 12.346 ms | 13.101 ms |
| Cinematic | 758 | 12.128 ms | 13.403 ms | 15.256 ms |
| Kinetic | 1251 | 16.797 ms | 18.187 ms | 18.831 ms |

说明：不同运行/seed 的音符数略有差异；上表为实际输出。

### 5.2 离线渲染

`ec_cli bench` 的 Chamber（48 kHz / block 512，tail 0.2s）：

- 音频 169.61 s
- 渲染 66,127.37 ms
- realtime factor **2.56×**
- peak 0.5312，RMS 0.0818，max voices 25

三套 demo（44.1 kHz / block 512 / 24-bit / tail 3s）：

| 风格 | 音频 | 渲染 | realtime | peak | RMS | max voices |
|---|---:|---:|---:|---:|---:|---:|
| Chamber | 172.412 s | 61,843.9 ms | 2.788× | 0.5559 | 0.0807 | 25 |
| Cinematic | 146.478 s | 41,257.2 ms | 3.550× | 0.6460 | 0.0674 | 57 |
| Kinetic | 73.820 s | 44,354.1 ms | 1.664× | 0.9613 | 0.1030 | 96（上限） |

说明：

- 离线渲染是单线程；这些不是实时 CPU 占用率。
- Kinetic 达到 96 声部上限，可能发生声部抢占；这是已知质量风险。
- 插件实时回调未做长时间 stress test；pluginval 的 audio processing 通过，但不等于所有 DAW 的实时压力测试。

## 6. 产物

| 产物 | 路径 | 大小 |
|---|---|---:|
| VST3 bundle | `dist/Expressive Composer.vst3` | 3,971,072 bytes；内层二进制 SHA-256 `75F238A6D278869DC5AE592B5C1096902C17BC7251E9BCD590E0B7DFC45D33BB` |
| Standalone | `dist/Expressive Composer.exe` | 12,807,168 bytes |
| UI 截图 | `dist/editor.png` / `docs/assets/editor.png` | 90,338 bytes |
| 三套工程 | `presets/demos/*/project.json` | 约 0.38 MB each |
| 三套 MIDI | `presets/demos/*/*.mid` | 0.23–0.38 MB |
| 三套 WAV | `presets/demos/*/*.wav` | 45.6 / 约 38.7 / 约 19.5 MB |
| 报告 | `presets/demos/*/report.txt` | 2–3 KB |

> WAV 与可执行文件是构建产物；仓库最终交付可保留或重新构建。若用 Git 管理，建议用 Git LFS 或发布页存放 WAV/EXE。

## 7. 已知问题

1. 200% 缩放的小屏上，1200×800 逻辑编辑器会超出屏幕；离屏快照完整，但实际窗口可能需要缩放/滚动。
2. `exportWavFile` 插件 API 未接到 UI；WAV 请用 CLI。
3. 和弦局部锁定缺失。
4. Kinetic 达到 96 声部上限。
5. 无真实 DAW / AU / CLAP / VST3 SDK validator 验证。
6. 无人工听音/盲测。
7. pluginval 末尾的 `can't open file for write !` 需在 CI 中复查（不影响 SUCCESS/exit 0）。

## 8. 交付时的构建目录

为减小交付体积，`build/`、`build-core/`、`build-plugin-ascii/` 在本报告定稿后已清理；验证过的二进制放在 `dist/`：

- `dist/tools/ec_cli.exe`、`dist/tools/ec_tests.exe`、`dist/tools/ec_bench.exe`
- `dist/Expressive Composer.vst3`
- `dist/Expressive Composer.exe`
- `dist/pluginval.log`

重新构建请使用 `scripts/build.ps1`；这不会改变上面的测试/兼容性结论。
