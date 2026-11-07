# 理论与心理依据 / Theory and Psychology

Expressive Composer 的规则不是“音乐情感的科学定律”。本文件把每个设计决定分成四类证据，并明确哪些只是可调启发式：

| 标签 | 含义 |
|---|---|
| **[R]** | 相对可靠、可重复的研究结论或基础心理声学事实；仍有边界与文化差异 |
| **[C]** | 特定文化、风格或历史时期的创作惯例；不是全人类普遍规律 |
| **[H]** | 本项目的工程启发式 / 经验规则；可调、可测试、可被推翻 |
| **[?]** | 尚未验证的产品假设；不得在 UI 或文档中写成事实 |

> 重要边界：没有任何映射可以保证“让所有听众感到悲伤/快乐”。本系统只控制可观察的声音特征与结构关系，并把这些决定开放给用户修改。情绪标签不是科学证明。

---

## 1. 为什么使用四维状态，而不是情绪标签

### 1.1 效价—唤醒模型

Russell 的 circumplex 模型把情绪描述为 valence（愉悦—不悦）与 arousal（激活—平静）两维空间中的位置 [R]。Mehrabian 与 Russell 进一步把环境心理学的 pleasure–arousal–dominance 框架用于情绪描述。

本工程采用：

- **valence**：阴暗 ↔ 明亮；
- **arousal**：平静 ↔ 高能量；
- **tension**：松弛 ↔ 悬置/压迫（不是简单等同于 arousal）；
- **distance**：亲密 ↔ 遥远/宏大（属于空间—心理距离，不是效价）。

**为什么增加 tension 与 distance？** 只用 valence/arousal 会把“安静但紧张”和“安静而安全”混为一谈；也会把“宏大”误当成“快乐”。因此 tension 独立控制和声未解决程度、低音持续、节奏不稳定与粗糙度；distance 独立控制干湿比、早期反射、宽度与高频衰减。

证据边界：

- Russell 的模型是**维度描述框架**，不是音乐情绪生成公式 [R]。
- 音乐诱发情绪的机制包括脑干反射、评价性条件化、情绪感染、视觉意象、情节记忆、音乐期待等（Juslin & Västfjäll, 2008）[R]，不存在单一“音乐情绪通道”。
- 同一段音乐在不同文化、训练背景、情境下可能被归入不同情绪类别 [C/?]。

### 1.2 情绪不是标签，而是时间中的变化

指南要求“从初始状态，经过发展/转折，到高点，最后收束”。这与音乐期待理论一致：情绪体验很大程度来自对未来的预测、延迟、违反与解决（Meyer, 1956；Huron, 2006）[R/C]。

工程实现：

- 四条曲线是**分段平滑的连续函数**，不是段落标签；
- 每个段落的目标值从曲线在段落中心采样；
- 每个叙述转折点对应至少一个可定位事件（和声、音区、密度、空间、配器或结尾）；
- 用户可编辑曲线点，重新作曲时使用新曲线。

---

## 2. 期待、紧张与解决

### 2.1 调性层级与旋律期待

Krumhansl 的 probe-tone 研究显示，听者对十二平均律音级有稳定的调性层级反应；大调/小调音阶、三和弦成员音通常比非和弦音更“稳定” [R]。Lerdahl 与 Jackendoff 的生成性音调音乐理论、Narmour 的 implication–realization 模型，以及 Temperley 的偏好规则，都试图解释旋律如何产生继续、回填与终止的期待 [R/C]。

工程实现：

- 强拍优先使用和弦音；弱拍允许经过音、邻音、悬留音；
- 旋律跳进后允许级进回填，但不强制执行；
- 句尾可以解决到主音，也可以停在非主音、九音或挂留音；
- 和声进行使用功能关系、延伸音、挂留、借用与再解释，而不是只按“大调/小调”标签。

### 2.2 张力不是不协和音的堆叠

心理声学中的 roughness 与 sensory dissonance 与频率邻近、临界带宽、低频区有关（Zwicker & Fastl, 2007；Moore, 2012）[R]。但“音乐紧张”还依赖调性期待、节奏、音区、重复与结构位置 [R/C]。

工程实现：

- `chordTension()` 只作为**局部声学粗糙度的粗略指标**，不是情绪真值；
- 紧张度曲线同时改变和声节奏、低音持续、密度、切分与音区；
- 高潮后可以**减层**而不是继续加厚；这来自指南的“对比与克制”原则 [H/C]；
- 不宣称“更多不协和 = 更紧张”是普遍规律。

### 2.3 解决与未解决

Meyer/Huron 的期待理论认为，延迟解决、意外终止与未解决会改变体验 [R/C]。本工程在结尾提供：

- 解决：低音/旋律回到主音，和声稳定；
- 悬置：停在 sus、九音、非主音或弱终止；
- 消退：撤层、缩短句尾、降低亮度与空间尾音。

这些是**可编辑的结构选择**，不是自动“悲伤”。

---

## 3. 重复、记忆与变奏

Margulis 的 *On Repeat* 讨论了重复如何改变注意、记忆与意义 [R/C]；Huron 的 ITPRA 理论把重复与预测联系起来 [R]。指南要求“重复负责记忆，变奏负责发展”。

工程实现：

- 每个风格包有一个可追踪的 `Motif`（音级、节奏、重音）；
- 每次出现记录 `MotifUse`：源动机、变换类型、时间范围、轨道；
- 变换包括原形、移调、倒影、逆行、扩大、缩小、片段化、扩展、改结尾、加装饰；
- 变奏不必改变所有参数：可以只换和声背景、音区、最后一个音、配器、密度或空间。

证据边界：

- “重复越好听”不是普遍规律；重复的效果取决于风格、长度、注意与情境 [C/?]。
- 本工程通过**可追踪的变奏**来避免“不断产生无关联新材料”，但不声称已证明听众会记住它。

---

## 4. 节奏、律动与微时序

### 4.1 内部时钟与同步

Large 与 Jones 的动态注意理论把节拍感知描述为内部振荡器对时间结构的追踪 [R]。Repp 的系列综述总结了传感器运动同步的稳健现象（Repp, 2005；Repp & Su, 2013）[R]。

工程实现：

- 以 PPQ tick 为唯一编辑单位，渲染时通过 tempo map 转秒/样本；
- 微时序是**拍/秒/样本**分开的单位，不把毫秒当 tick；
- 宿主播放时按 PPQ 位置调度，速度变化不会改变音符的拍位置。

### 4.2 切分与 groove

Witek 等人的研究表明，切分与身体律动/愉悦之间可能存在倒 U 形关系：中等切分比无切分或极端切分更可能引发 groove [R/?]。该研究主要使用西方流行/电子鼓型与特定被试群体，不能外推为所有文化 [C]。

工程实现：

- 切分是风格包的参数，不是全局“越复杂越好”；
- 高潮前允许密度累积，但会在某处撤掉低频/鼓/和声层；
- 微时序成组：句首略迟、句中略前、句尾放缓；鼓组按角色（kick/snare/hat）保持不同偏移，而不是逐音随机。

### 4.3 演奏表情

Palmer 的综述与 Juslin & Laukka 的跨通道研究指出，演奏中的力度、发音、时序与音色变化可以传达情绪，且具有部分跨通道规律 [R]。Fabian, Timmers & Schubert 的编著讨论了不同风格与文化中的表达差异 [R/C]。

工程实现：

- 力度不只映射音量：同时改变起音、亮度、噪声、释放与音色；
- 乐句级渐强/渐弱、句尾收束、呼吸间隙；
- 连续发声使用状态继承与平滑漂移，不做逐音独立随机；
- 所有微时序有上限（±12 ms）并写入 MIDI tick 位置。

证据边界：

- 表演表情的“代码”不是全人类统一的；Juslin & Laukka 也强调情境与学习 [R/C]。
- 本工程没有进行听者实验，不能声称“人类听众会感知为自然”。

---

## 5. 音色：从激励—共振—辐射理解声音

### 5.1 物理声学

Fletcher & Rossing 的 *The Physics of Musical Instruments*、Giordano 的 *The Physics of the Piano* 提供了弦、板、腔体、锤击与辐射的经典模型 [R]。真实钢琴的弦具有刚度（inharmonicity）、双/三弦失谐、锤击位置梳状激励、音板多模态与踏板共鸣。

工程实现（Felt Piano）：

- 模态频率 `f_n = n f0 sqrt(1 + B n^2)`，B 随力度/音高变化；
- 锤击位置产生 comb 型模态幅度；力度改变高频倾斜与噪声；
- 高次模态衰减更快；弦组失谐产生拍频；
- 锤击噪声经高通后单独加入，不把它当成持续噪声层；
- 释放使用独立阻尼，不用一个 ADSR 包打天下。

这不是完整物理建模；它是**物理启发的混合模态合成** [H]。因此不能声称与真实钢琴录音无法区分。

### 5.2 弓弦与噪声

Bowed String 使用带限锯齿、弓噪、琴体带通和延迟颤音。真实弓弦的 Helmholtz 运动、弓压/弓速的非线性与琴体耦合远比本实现复杂 [R/H]。本实现的目标是**可演奏、可辨识、参数联动**，不是音乐会级采样替代品。

### 5.3 频谱、粗糙度与听觉场景

Bregman 的听觉场景分析说明，听者会把频率/时间上相关的成分分组为同一源 [R]。Moore 与 Zwicker/Fastl 总结了掩蔽、临界带、响度与粗糙度 [R]。

工程实现：

- 同一乐器的模态共享包络、失谐与起音，避免被听成多个独立正弦；
- 混响不掩盖干声瞬态；每个音符有独立 send；
- 关闭混响后，Felt Piano / Bowed String 的主体仍存在（测试验证非静音与有限值，不声称听感）。

### 5.4 合成方法选择

| 目标 | 方法 | 依据 |
|---|---|---|
| 毡音钢琴 | 模态 + 噪声激励 + 琴体滤波 | 弦/板模态物理 [R/H] |
| 弓弦 | 锯齿 + 弓噪 + 带通 | 弓弦激励—共振结构 [R/H] |
| 玻璃/槌击 | 非谐和模态 | 板/壳模态 [R/H] |
| 低音 | 正弦 + 锯齿 + 饱和 | 电子贝斯惯例 [C/H] |
| 鼓 | 正弦瞬态 + 滤波噪声 | 合成鼓惯例 [C/H] |

Chowning 的 FM 合成、Roads 的 *Microsound*、Cook 的 *Real Sound Synthesis* 提供了方法学背景 [R/C]；本工程没有使用 FM 作为主音色，也没有使用粒子合成。

---

## 6. 空间：心理距离不是“加混响”

### 6.1 距离线索

听觉距离依赖直达声/混响比、高频空气吸收、早期反射、响度与熟悉度等（Moore, 2012；Bregman, 1990）[R]。Moorer 与 Jot/Chaigne 讨论了人工混响的结构与参数 [R/H]。

工程实现：

- 每轨/每音有 reverb send，干声始终存在；
- 距离曲线影响 send、宽度与高频阻尼；
- 混响使用 FDN + 阻尼 + 预延迟，不把整首作品统一浸泡；
- 结尾通过撤层与缩短尾音制造“远”，而不是只加长混响。

### 6.2 动态与掩蔽

响度、掩蔽与动态余量是混音事实 [R]。工程实现：

- 主总线有低/高架 EQ、压缩、软限幅和 DC 阻断；
- 限制器只做保护，不承担“激烈”；
- 保留峰值余量，避免持续满密度。

---

## 7. 映射表：从曲线到音乐行为

下表是本工程实际使用的映射（`src/core/Expression.cpp`）。所有权重都是可调启发式 [H]。

| 输入 | 主要影响 | 次要影响 | 证据 |
|---|---|---|---|
| valence ↑ | 音色亮度、和声色彩偏好、终止解决倾向 | 噪声略降、音区略升 | [H/C]，不是“大调=快乐” |
| arousal ↑ | 音符密度、力度、起音速度、脉冲清晰度 | 音区略升、层数增加 | [R/H] |
| tension ↑ | 和声张力预算、不协和/挂留、切分、低音持续 | 音区压低、句长缩短 | [R/C/H] |
| distance ↑ | reverb send、宽度、高频阻尼 | 力度范围收窄、音域跨度增大 | [R/H] |
| restraint | 降低密度、力度范围、亮度、空间 | 句尾更收 | [H] |
| 视角/场景 | 选择风格包的配器与空间基线 | 不直接决定音符 | [C/?] |
| 文化/风格 | 规则包（音阶、进行、节奏、配器） | 不声称普遍 | [C] |

实现约束：

- 单维度不会直接切换调式；调式由风格包/用户决定；
- 所有映射经过 clamp，参数不能导致 NaN/Inf；
- 同一曲线在四个声部上的映射不同（旋律/和声/低音/打击），避免“所有东西一起变”。

---

## 8. 工程启发式与未验证假设（必须可被推翻）

| ID | 规则 | 标签 | 如何验证/推翻 |
|---|---|---|---|
| H1 | 乐句 2 小节、句尾留 0.25–0.5 拍呼吸更自然 | H | 盲听比较不同句长/呼吸 |
| H2 | 力度增加应同时增加亮度与瞬态 | R/H | 力度阶梯的听辨与频谱分析 |
| H3 | 微时序 ±5–8 ms 且成组出现比逐音随机更自然 | H | A/B 与演奏者评价 |
| H4 | 每 8 小节机械加层会导致饱和；应在高点后减层 | H/C | 对比加层 vs 减层版本 |
| H5 | 高点只出现一次或分层出现更有效 | H | 结构评价 |
| H6 | 结尾停在九音/挂留比强行主和弦更“未竟” | C/H | 风格与语境依赖 |
| H7 | 距离增加时高频衰减与干湿比同时变化更可信 | R/H | 空间听辨 |
| H8 | 两个轨道的角色分工比堆叠同类音色更清晰 | H | 混音/编曲评价 |
| H9 | 动机变奏比全新材料更容易形成记忆 | R/C | 记忆测试；风格依赖 |
| H10 | 用户锁定 + 局部重生成能提升控制感 | ? | 用户任务实验 |

---

## 9. 系统明确不声称的事情

1. 不声称“大调=快乐、小调=悲伤、慢=温柔、快=激动”。
2. 不声称任何和弦/音色/节奏能保证引发特定情绪。
3. 不声称已证明跨文化普遍性；默认规则包是西方 12-TET/功能和声 [C]。
4. 不声称“自然感音色”与真实录音无法区分；没有采样库，也没有听觉验证。
5. 不声称“AI 自动理解歌词/音频”；P0 没有模型推理，歌词字段只是元数据。
6. 不声称单插件能读取 DAW 所有轨道或做全局母带。
7. 不声称性能数字适用于所有机器；只报告本机实测。
8. 不声称已经完成盲听、A/B 或专家评审；这些是待执行计划。

---

## 10. 参考文献（真实可核查）

> 以下为本书目中的真实出版信息；未列 DOI 的条目请按题名/作者/出版社核查。没有编造文献、DOI 或实验结果。

1. Russell, J. A. (1980). A circumplex model of affect. *Journal of Personality and Social Psychology*, 39(6), 1161–1178.
2. Mehrabian, A., & Russell, J. A. (1974). *An Approach to Environmental Psychology*. MIT Press.
3. Meyer, L. B. (1956). *Emotion and Meaning in Music*. University of Chicago Press.
4. Huron, D. (2006). *Sweet Anticipation: Music and the Psychology of Expectation*. MIT Press.
5. Narmour, E. (1990). *The Analysis and Cognition of Basic Melodic Structures: The Implication–Realization Model*. University of Chicago Press.
6. Lerdahl, F., & Jackendoff, R. (1983). *A Generative Theory of Tonal Music*. MIT Press.
7. Krumhansl, C. L. (1990). *Cognitive Foundations of Musical Pitch*. Oxford University Press.
8. Temperley, D. (2001). *The Cognition of Basic Musical Structures*. MIT Press.
9. Caplin, W. E. (1998). *Classical Form: A Theory of Formal Functions for the Instrumental Music of Haydn, Mozart, and Beethoven*. Oxford University Press.
10. Aldwell, E., Schachter, C., & Cadwallader, A. (2011). *Harmony and Voice Leading* (4th ed.). Cengage Learning.
11. Margulis, E. H. (2014). *On Repeat: How Music Plays the Mind*. Oxford University Press.
12. Huron, D., & Margulis, E. H. (2010). Musical expectancy and thrills. In P. N. Juslin & J. A. Sloboda (Eds.), *Handbook of Music and Emotion: Theory, Research, Applications* (pp. 575–604). Oxford University Press.
13. Large, E. W., & Jones, M. R. (1999). The dynamics of attending: How people track time-varying events. *Psychological Review*, 106(1), 119–159.
14. Repp, B. H. (2005). Sensorimotor synchronization: A review of the tapping literature. *Psychonomic Bulletin & Review*, 12(6), 969–992.
15. Repp, B. H., & Su, Y.-H. (2013). Sensorimotor synchronization: A review of recent research (2006–2012). *Psychonomic Bulletin & Review*, 20(3), 403–452.
16. Witek, M. A. G., Clarke, E. F., Wallentin, M., Kringelbach, M. L., & Vuust, P. (2014). Syncopation, body-movement and pleasure in groove music. *PLoS ONE*, 9(4), e94446.
17. Palmer, C. (1997). Music performance. *Annual Review of Psychology*, 48, 115–138.
18. Juslin, P. N., & Laukka, P. (2003). Communication of emotions in vocal expression and music performance: Different channels, same code? *Psychological Bulletin*, 129(5), 770–814.
19. Juslin, P. N., & Västfjäll, D. (2008). Emotional responses to music: The need to consider underlying mechanisms. *Behavioral and Brain Sciences*, 31(5), 559–575.
20. Fabian, D., Timmers, R., & Schubert, E. (Eds.). (2014). *Expressiveness in Music Performance: Empirical Approaches Across Styles and Cultures*. Oxford University Press.
21. Juslin, P. N., & Sloboda, J. A. (Eds.). (2010). *Handbook of Music and Emotion: Theory, Research, Applications*. Oxford University Press.
22. Bregman, A. S. (1990). *Auditory Scene Analysis: The Perceptual Organization of Sound*. MIT Press.
23. Moore, B. C. J. (2012). *An Introduction to the Psychology of Hearing* (6th ed.). Brill.
24. Zwicker, E., & Fastl, H. (2007). *Psychoacoustics: Facts and Models* (3rd ed.). Springer.
25. Fletcher, N. H., & Rossing, T. D. (1998). *The Physics of Musical Instruments* (2nd ed.). Springer.
26. Giordano, N. J. (2010). *The Physics of the Piano*. Oxford University Press.
27. Smith, J. O. (2010). *Physical Audio Signal Processing for Virtual Musical Instruments and Audio Effects*. Online book, Stanford CCRMA.
28. Cook, P. R. (2002). *Real Sound Synthesis for Interactive Applications*. A K Peters.
29. Chowning, J. M. (1973). The synthesis of complex audio spectra by means of frequency modulation. *Journal of the Audio Engineering Society*, 21(7), 526–534.
30. Roads, C. (2001). *Microsound*. MIT Press.
31. Moorer, J. A. (1979). About this reverberation business. *Computer Music Journal*, 3(2), 13–28.
32. Jot, J.-M., & Chaigne, A. (1991). Digital delay networks for designing artificial reverberators. *AES Convention Paper*, 90th Convention, Paris.
33. Zölzer, U. (Ed.). (2011). *DAFX: Digital Audio Effects* (2nd ed.). Wiley.
34. Parncutt, R., & McPherson, G. E. (Eds.). (2002). *The Science and Psychology of Music Performance: Creative Strategies for Teaching and Learning*. Oxford University Press.
35. Sloboda, J. A. (1985). *The Musical Mind: The Cognitive Psychology of Music*. Oxford University Press.
36. Koelsch, S. (2012). *Brain and Music*. Wiley-Blackwell.
37. Salimpoor, V. N., Benovoy, M., Larcher, K., Dagher, A., & Zatorre, R. J. (2011). Anatomically distinct dopamine release during anticipation and experience of peak emotion to music. *Nature Neuroscience*, 14(2), 257–262.
38. Blood, A. J., & Zatorre, R. J. (2001). Intensely pleasurable responses to music correlate with activity in brain regions implicated in reward and emotion. *Proceedings of the National Academy of Sciences*, 98(20), 11818–11823.
39. Zatorre, R. J., & Salimpoor, V. N. (2013). From perception to pleasure: Music and its neural substrates. *Proceedings of the National Academy of Sciences*, 110(Suppl. 2), 10430–10437.
40. Bigand, E., & Poulin-Charronnat, B. (2006). Are we experienced listeners? A review of the musical capacities that do not depend on formal musical training. *Cognition*, 100(1), 100–130.
41. Deutsch, D. (Ed.). (2013). *The Psychology of Music* (3rd ed.). Academic Press.
42. Eerola, T., & Vuoskoski, J. K. (2013). A review of music and emotion studies: Approaches, emotion models, and stimuli. *Music Perception*, 30(3), 307–340.

## 11. 参考使用方式

- 具体代码权重见 `src/core/Expression.cpp` 与 `src/core/Composer.cpp`；
- 评价方法见 `ARTISTIC_EVALUATION.md`；
- 未完成的研究验证在 `ROADMAP.md` 中明确标注；
- 任何新的映射都应先加测试，再更新本文件的证据标签。
