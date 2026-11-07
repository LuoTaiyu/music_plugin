# 依赖与许可证清单

本文件不是法律意见。分发前请让法务复核，尤其是 JUCE 与 VST3 SDK 的双许可选择。

## 1. 本项目许可

**Expressive Composer 0.1.0 — AGPL-3.0**

选择原因：JUCE 8 的免费/开源路径是 AGPLv3；本项目使用 JUCE 构建 VST3/Standalone。若你要闭源商业分发：

1. 购买 JUCE 商业许可；并
2. 遵守 Steinberg VST3 SDK 的许可条款；并
3. 重新评估本仓库代码的许可（当前整体按 AGPL-3.0 发布）。

AGPL 完整文本见 `LICENSE`（构建时从 GNU 官方获取；若缺失，请以 https://www.gnu.org/licenses/agpl-3.0.txt 为准）。

## 2. 直接依赖

| 依赖 | 版本/来源 | 用途 | 许可 | 分发注意 |
|---|---|---|---|---|
| JUCE | 8.0.6，`third_party/JUCE`，源码包 SHA-256 `95752f8e8b37d802b4c10182bc757de7d88f0c8899e879afa8798589be306a11` | 插件外壳、VST3/Standalone、GUI | AGPLv3 或 JUCE 商业许可（双许可） | 本仓库按 AGPLv3；闭源需商业许可 |
| VST3 SDK | JUCE 内 `modules/juce_audio_processors/format_types/VST3_SDK` | VST3 | GPLv3 或 Steinberg 专有许可（双许可） | 必须遵守所选许可；不要移除版权声明 |
| clap-juce-extensions | `third_party/clap-juce-extensions`（可选；子模块未初始化） | CLAP 包装 | MIT（见其 `LICENSE.md`） | 当前未构建/未验证 |
| CLAP | 通过 clap-juce-extensions 子模块获取 | CLAP API | MIT | 当前未构建/未验证 |
| CMake | 3.22+（本机 4.4.4） | 构建 | BSD-3-Clause | 构建工具，不随插件分发 |
| Ninja | 1.13.2（本机 pip 包） | 构建 | Apache-2.0 | 构建工具，不随插件分发 |
| MSVC / Windows SDK | Visual Studio 2022 Build Tools | 编译 | Microsoft 许可 | 不随源码分发 |
| Python 3.12 / NumPy（仅分析时可选） | 本机 | 离线分析/图表（非产品核心） | PSF / BSD | 不进入实时插件 |

**没有使用**：采样库、第三方合成器、字体文件、云模型 SDK、机器学习框架、音频分析库。

## 3. 代码来源声明

- 所有 `src/core`、`src/dsp`、`src/io`、`src/render`、`src/plugin`、`src/cli` 代码为本会话从零编写；未复制 `tools.zip`（未提供）。
- JUCE 源码按原样放在 `third_party/JUCE`，未修改（除构建生成物）。
- clap-juce-extensions 为可选第三方源码，当前子模块未初始化，CLAP 目标未构建。
- 用户提供的两份指南复制到 `docs/reference/`，仅作为规范与审计材料；其版权归用户/原作者，未在本项目代码中声称原创。

## 4. 字体与 UI

- UI 使用系统字体：Windows 上为 Segoe UI / Consolas；macOS 上会回退到系统可用字体。
- 没有随包分发字体文件，因此不引入额外字体许可。
- 如果未来打包自定义字体，必须记录字体名称、来源、许可（如 SIL OFL）与嵌入方式。

## 5. 采样与音色

- **没有采样库**。Felt Piano / Bowed String / Glass / Bass / Drums 全部为程序化合成。
- 如果未来接入采样库：
  - 采样来源、录音者、许可、是否允许商业分发、是否允许再分发必须逐项记录；
  - 不得把未授权的商业采样打包进插件；
  - 需要提供缺失采样库时的降级音色。

## 6. API 密钥与隐私

- 当前没有网络请求，没有模型 API，没有 API 密钥。
- 工程 JSON、预设、日志中不包含密钥。
- 如果未来添加云端模型：
  - 密钥只能从用户环境/系统钥匙串读取，不能写入源码、预设、日志、工程文件；
  - 必须显式 opt-in，并提示音频/MIDI 可能离开本机；
  - 必须提供完全离线的降级路径。

## 7. 构建产物

- VST3/Standalone 二进制包含 JUCE 与 VST3 SDK 代码；分发二进制同样受 JUCE/VST3 许可约束。
- 本仓库的 `build-*` 目录是构建产物，不应作为源码许可的依据。
- `third_party/downloads/*.tar.gz` 是依赖缓存；可删除，重新由 `scripts/fetch_dependencies.ps1` 获取。

## 8. 合规检查清单

- [ ] 选择 JUCE AGPLv3 或购买商业许可。
- [ ] 随二进制提供 AGPL 源码获取方式（若使用 AGPL 路径）。
- [ ] 保留 JUCE / VST3 SDK 版权与许可文件。
- [ ] 记录所有新增第三方代码/采样/字体。
- [ ] 确认插件名称、厂商代码、VST3 ID 不侵犯他人商标。
- [ ] 若闭源：确认 JUCE 商业许可覆盖 VST3/Standalone，并复核 VST3 SDK 专有许可。
- [ ] 若使用 CLAP：初始化子模块并复核 clap-juce-extensions 与 CLAP 的 MIT 文本。
