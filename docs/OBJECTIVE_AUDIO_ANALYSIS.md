# 客观音频分析

> 这些是**信号测量**，不是听感评价。测量对象：`presets/demos/*/*.wav`，seed 20261108，44.1 kHz / 24-bit / stereo。

## 方法

- 直接解析 RIFF/WAVE，24-bit little-endian PCM；
- 单声道 = 左右平均，用于频谱；
- 峰值、RMS、DC、crest factor；
- STFT：Hann 窗 4096，overlap 2048；spectral centroid 为按幅度加权的频率均值；
- stereo correlation：左右声道 Pearson 相关；
- silence ratio：512-sample 帧 RMS < -60 dBFS 的比例；
- 未做 LUFS/true-peak 标准测量；数值不能直接当作流媒体响度。

复现脚本：`build/analyze_audio.py`（离线分析，不是产品核心）。

## 结果

| 指标 | Chamber | Cinematic | Kinetic |
|---|---:|---:|---:|
| 格式 | 44.1k/24/stereo | 44.1k/24/stereo | 44.1k/24/stereo |
| 时长 | 172.412 s | 146.478 s | 73.820 s |
| 峰值 | 0.5559 | 0.6460 | 0.9613 |
| RMS | 0.0807 | 0.0674 | 0.1030 |
| Crest factor | 16.76 dB | 19.63 dB | 19.40 dB |
| DC offset | 0.0000000 | -0.0000000 | -0.0000000 |
| Spectral centroid | 2198 Hz | 4840 Hz | 9203 Hz |
| Stereo correlation | 0.964 | 0.924 | 0.980 |
| Silence ratio (< -60 dBFS) | 1.06% | 0.33% | 2.85% |

## 解读（谨慎）

- 三套都无削波、无 DC、无 NaN/Inf；这与测试一致。
- Kinetic 的 centroid 远高于其他两套，主要由 hats/噪声瞬态与高频 pluck 贡献；这不等于“更明亮/更好”，也可能是疲劳风险。
- Kinetic 峰值 0.961 接近 0 dBFS，说明限幅器在工作；没有样本超过 0.999。实际分发时建议留更多余量。
- 立体声相关 0.92–0.98 偏高：当前声像/宽度较保守，空间宽度主要来自混响而非声源分布。可作为 P1 混音改进点。
- Chamber 的 silence ratio 1.06% 与句尾呼吸/段落间隙一致；但本测量不能证明“呼吸自然”。
- Cinematic 的 crest 最高，说明瞬态/动态保留较多；这是设计意图，但未做听感验证。

## 不能从这些数字推出的结论

- 不能推出“情感表达成功”；
- 不能推出“音色自然”；
- 不能推出“混音平衡”；
- 不能推出“跨设备一致”；
- 不能替代盲听。
