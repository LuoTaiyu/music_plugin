# 三套可编辑示范

由 `ec_cli generate --demo all --seed 20261108` 生成。每套都包含：

- `project.json`：完整工程（音符、和弦、乐句、动机、四条曲线、自动化、轨道）；
- `<id>.mid`：Type 1 SMF，含 tempo、拍号、段落 marker、CC、以及 `ECP1` 完整工程负载；
- `<id>.wav`：44.1 kHz / 24-bit / stereo 离线渲染；
- `report.txt`：真实生成/渲染统计、决定与警告。

| id | 标题 | 调式/拍号/速度 | 小节 | 时长 |
|---|---|---|---:|---:|
| chamber | Letters in a Quiet Room | F minor / 4/4 / 68 | 48 | 172.4 s |
| cinematic | Pressure Line | D phrygian / 5/4 / 92 | 44 | 146.5 s |
| kinetic | Kinetic Bloom | E dorian / 4/4 / 122 | 36 | 73.8 s |

## 快速使用

```powershell
# 用 CLI 重新渲染（不会重新作曲）
..\..\dist\tools\ec_cli.exe render chamber\project.json --wav chamber\preview.wav --sr 48000 --bits 24

# 回导 MIDI（标准路径）
..\..\dist\tools\ec_cli.exe import-midi chamber\chamber.mid --standard --out chamber_imported.json

# 查看结构
..\..\dist\tools\ec_cli.exe inspect cinematic\project.json
```

## 注意

- 这些 WAV 是**离线渲染的合成音频**，不是采样录音，也不代表已完成听感验证。
- 工程是 seed 20261108 的确定性结果；修改 seed 会改变音符、和声与演奏细节。
- `songs/` 目录不属于本交付，是同一工作区中其他工作的文件；本目录只描述 `presets/demos/`。
