# EdgeCare：基于GD32H759的端侧工业危险区闯入检测终端

## 封面

第二十一届中国研究生电子设计竞赛

技术论文

论文题目：

中文：EdgeCare：基于GD32H759的端侧工业危险区闯入检测终端

英文：EdgeCare: An Edge Industrial Danger-Zone Intrusion Detection Terminal Based on GD32H759

参赛单位：【待填写】

队伍名称：【待填写】

指导老师：【待填写】

参赛队员：【待填写】

完成时间：【待填写】

## 中文摘要

工业现场的机械臂、传送带、高温设备和带电区域存在人员误入风险。传统围栏和人工巡检灵活性不足，普通视频监控又会带来隐私、带宽和集中计算压力。本文设计并实现一种面向固定危险区域的端侧闯入检测终端 EdgeCare。系统以 GD32H759I-START 为主控，使用 HLK-LD2410B 毫米波雷达作为人体存在触发前端，使用 OV5640 DVP 相机采集固定机位下的危险区画面，在 MCU 本地完成图像采集、灰度预处理、轻量模型推理、多帧投票以及声光/语音告警控制。系统不上传连续视频流，只在本地输出告警状态和语音提示，具有低成本、低带宽和隐私友好的特点。

作品的主要工程难点包括 GD32H759 与 OV5640 DVP 相机适配、小样本 gray96 数据集采集、端侧定点模型部署和雷达视觉融合误报抑制。当前相机可用链路为 `OV5640_JPEG_TO_YUV_REF + DCI rising + 0x4745=0x00 + raw gray96`，能够由 `320x240 YUV422` 生成 `96x96 gray8` 模型输入。模型采用 8x8 网格灰度统计和 Logistic Regression，共 69 维特征，导出为 Q15 定点权重在 GD32H759 上运行。最新训练集共 376 张样本，其中 empty 136 张、safe 100 张、intrusion 140 张；验证集准确率为 84.0%，全量回放准确率为 87.5%，上板推理耗时约 1 ms。系统已打通雷达触发、真实场景灰度采集、端侧推理、声光报警和 VW553 语音提示的完整闭环。受限于数据规模和调试周期，当前彩色/YUV 图像解释仍有伪彩和噪声，模型也仍需通过更多光照、边界和遮挡样本继续提升泛化能力。

关键词：端侧人工智能；GD32H759；OV5640；危险区闯入检测；工业安全

## English Abstract

Hazardous industrial zones such as robotic workspaces, conveyors, high-temperature equipment and energized areas require timely intrusion warning. Traditional fences and manual inspection are not flexible enough, while video monitoring may introduce privacy, bandwidth and centralized-computing costs. This paper presents EdgeCare, an edge danger-zone intrusion detection terminal based on GD32H759I-START. A HLK-LD2410B radar module is used as a low-power human-presence trigger, and an OV5640 DVP camera captures the fixed danger-zone scene. Image acquisition, grayscale preprocessing, lightweight inference, voting and alarm control are executed locally on the MCU. The terminal does not upload continuous video streams, reducing privacy and network risks.

The main challenges are OV5640 DVP adaptation on GD32H759, small gray96 dataset collection, fixed-point model deployment and false-alarm suppression. The validated camera path is `OV5640_JPEG_TO_YUV_REF + DCI rising + 0x4745=0x00 + raw gray96`, which converts `320x240 YUV422` frames into `96x96 gray8` model input. The deployed model is a 69-feature Logistic Regression baseline using global grayscale statistics and an 8x8 grid, exported as Q15 fixed-point weights. The latest dataset contains 376 samples: 136 empty, 100 safe and 140 intrusion. The validation accuracy is 84.0%, full replay accuracy is 87.5%, and inference time on GD32H759 is about 1 ms. The system has completed an end-to-end prototype from radar triggering and real-scene grayscale capture to local inference, sound alarm and VW553 voice prompt. Color/YUV interpretation is still artifacted, and further dataset expansion is required for stronger generalization.

Keywords: Edge AI; GD32H759; OV5640; danger-zone intrusion detection; industrial safety

## 目录

第1章 作品难点与创新

1.1 研究背景与应用意义

1.1.1 工业危险区安全需求

1.2 主要技术难点

1.2.1 相机与数据集难点

1.2.2 端侧模型与告警难点

1.3 作品创新点

1.3.1 雷达门控与视觉确认融合

第2章 方案论证与总体设计

2.1 需求分析

2.1.1 功能需求与约束

2.2 方案比较

2.2.1 纯雷达、纯视觉与融合方案

2.3 总体架构

2.3.1 感知、处理、决策与告警分层

第3章 原理分析与硬件设计

3.1 主控与外设选择

3.1.1 GD32H759 与模块选型

3.2 OV5640 图像采集链路

3.2.1 SCCB 控制面与 DVP 数据面

3.3 雷达、声光与语音链路

3.3.1 雷达门控和 UART 语音触发

第4章 软件设计与流程

4.1 固件总体流程

4.1.1 初始化与周期状态机

4.2 图像预处理与采集

4.2.1 gray96 输入生成

4.3 模型训练与端侧推理

4.3.1 69 维特征与 Q15 导出

4.4 多帧投票与告警

4.4.1 2-of-3 投票与释放迟滞

第5章 系统测试与分析

5.1 相机链路测试

5.1.1 ID、时序、DMA 与 gray96 测试

5.2 数据集与模型测试

5.2.1 类别分布与混淆矩阵

5.3 上板运行测试

5.3.1 构建、烧录和运行日志

5.4 局限性分析

5.4.1 数据、模型、相机与雷达边界

第6章 总结

参考文献

## 第1章 作品难点与创新

### 1.1 研究背景与应用意义

#### 1.1.1 工业危险区安全需求

工业危险区通常具有边界明确、风险高和误入后果严重等特点，例如机械臂工作半径、传送带入口、高温设备周边和临时检修带电区域。传统围栏对临时工位和小型实验平台不够灵活；人工巡检连续性差；普通视频监控又会引入隐私和带宽压力。本作品将目标限定为固定机位危险区二分类检测，即判断人员或目标物是否进入预设区域，并把判断过程放在 MCU 本地完成。

### 1.2 主要技术难点

#### 1.2.1 相机与数据集难点

第一项难点是 OV5640 DVP 相机适配。GD32H759I-START 不是专用相机板，EVAL 例程的部分引脚在 START 板上未引出，部分候选引脚还与板载 LED 或报警输出冲突。因此系统需要重新确定 SCCB、PCLK、HREF、VSYNC 和 D0-D7 映射，并验证 ID 读取、寄存器初始化、DCI 极性、DMA 传输和字节解释。第二项难点是小样本数据集。当前模型输入为 `96x96 gray8`，危险区边界和目标姿态会被强烈压缩。采集时不仅要拍明显闯入，还要拍 empty、safe 和 hard-negative，否则演示时容易常响误报。

#### 1.2.2 端侧模型与告警难点

第三项难点是模型部署。大型 CNN 或目标检测网络会增加训练、量化和运行时移植风险。本文采用灰度统计与网格均值 Logistic Regression，并导出 Q15 定点数组编译进固件。第四项难点是报警稳定性。LD2410B 的前向扇形检测区不等同于矩形危险区，视觉模型又会受光照、横线和小样本偏差影响，因此系统需要雷达门控、视觉确认、多帧投票和释放迟滞。

### 1.3 作品创新点

#### 1.3.1 雷达门控与视觉确认融合

本作品的创新主要体现在四方面。第一，针对 GD32H759I-START 与 OV5640 DVP 模块完成 BSP/驱动适配，实现 OV5640 ID 识别、DCI/DMA QVGA 抓帧和 gray96 输入生成。第二，采用雷达触发与视觉确认融合结构，避免把雷达扇形区域误当作危险区边界。第三，建立 MCU 串口导出样本、PC 端质检训练、Q15 权重回写固件的闭环。第四，告警逻辑采用 2-of-3 投票和安全释放迟滞，并通过 VW553 UART 命令 `DANGER\n` 在报警上升沿播报。

## 第2章 方案论证与总体设计

### 2.1 需求分析

#### 2.1.1 功能需求与约束

系统功能需求包括：固定机位下采集危险区图像；在 MCU 本地判断是否闯入；确认闯入后输出声光与语音告警；通过串口给出稳定日志。非功能需求包括低成本、低带宽、隐私保护和可复现，即使用常见开发板与模块，不上传连续视频，并记录接线、固件参数、采集流程和训练脚本。

### 2.2 方案比较

#### 2.2.1 纯雷达、纯视觉与融合方案

纯雷达方案简单、低功耗、响应快，但无法判断目标是否进入画面中的危险区。纯视觉方案能够表达区域边界，但持续运行会增加 MCU 负担。融合方案把雷达作为低功耗触发前端，把视觉作为最终判决，兼顾实时性和区域语义。模型方案上，本文暂不采用 YOLO、MobileNet 或较大 CNN，而采用灰度统计 + 8x8 网格 Logistic Regression 作为初赛 MVP 基线。

### 2.3 总体架构

#### 2.3.1 感知、处理、决策与告警分层

系统由感知层、处理层、决策层和告警层组成。感知层包括 LD2410B 和 OV5640；处理层完成相机配置、DCI/DMA 抓帧和 gray96 预处理；决策层运行 Q15 模型与多帧投票；告警层包含 PA8 声光模块和 USART1 连接的 GD32VW553。

```text
LD2410B -> PF8 雷达门控
OV5640 -> DCI/DMA -> 320x240 YUV422 -> 96x96 gray8
gray96 -> 69维特征 -> Q15 Logistic Regression -> 2-of-3投票
投票结果 -> PA8声光报警 + USART1发送DANGER\n
```

图2-1 系统总体架构

资料来源：本作品硬件设计与固件实现。

运行流程为：上电后初始化串口、雷达、相机、语音和报警模块；读取 OV5640 ID 并配置 QVGA 输出；周期循环中先读取雷达，若 `radar=0` 则关闭报警并复位投票，若 `radar=1` 则抓取一帧图像并完成视觉判断。

```text
初始化 -> 相机probe -> 周期循环
radar=0 -> IDLE, alarm=0, reset votes
radar=1 -> capture -> gray96 -> infer -> vote -> alarm/voice
```

图2-2 数据流与告警闭环

资料来源：本作品 `edgecare_app.c` 状态机设计。

## 第3章 原理分析与硬件设计

### 3.1 主控与外设选择

#### 3.1.1 GD32H759 与模块选型

主控选择 GD32H759I-START。该平台具备较高主频、丰富 GPIO、DCI 摄像头接口、DMA、USART 和足够片上 SRAM，适合相机采集和小型端侧推理。固件采用 Keil MDK 裸机工程，模块划分为 `board`、`bsp`、`vision`、`model`、`platform` 和 `app`。

相机选择 OV5640 DVP 模块，实物通过读取 `0x300A/0x300B` 得到 `0x56/0x40`。人体存在传感器选择 HLK-LD2410B，通过 `V/G/O` 三线方式接入，`O` 高电平表示有人。声光模块由 PA8 低电平有效控制，语音模块使用 GD32VW553，通过 UART 接收 `DANGER\n` 后播放提示。

### 3.2 OV5640 图像采集链路

#### 3.2.1 SCCB 控制面与 DVP 数据面

OV5640 相机链路分为控制面和数据面。控制面使用 SCCB/I2C，`PF1/PF0` 连接 `SCL/SDA`，`PD0` 控制 `RES`，`PD1` 控制 `PWON/PWDN`。数据面使用 DVP 并口，PCLK 接 `PA6`，HREF 接 `PA4`，SYNC/VSYNC 接 `PB7`，D0-D7 接 `PC6/PC7/PC8/PG11/PC11/PB6/PE5/PB9`。

图3-1 硬件连接框图

资料来源：本作品硬件接线记录。

| 模块 | 信号 | GD32H759I-START 引脚 | 说明 |
| --- | --- | --- | --- |
| USB-TTL | RXD/TXD | PF4/PF5 | USART0 日志 |
| LD2410B | O | PF8 | 有人输出高电平 |
| 声光模块 | IN | PA8 | 低电平报警 |
| OV5640 | SCL/SDA | PF1/PF0 | SCCB 控制 |
| OV5640 | RES/PWON | PD0/PD1 | 复位与电源控制 |
| OV5640 | PCLK/HREF/SYNC | PA6/PA4/PB7 | DVP 时序 |
| OV5640 | D0-D7 | PC6/PC7/PC8/PG11/PC11/PB6/PE5/PB9 | DVP 数据 |
| VW553 | UART2_RX/TX | PA2/PA3 | H759 USART1 连接 |

表3-1 关键模块与引脚连接

资料来源：本作品接线文档。

相机调试的关键结论是，旧默认 `0x4745=0x02` 会使采集字节呈现近似右移 2 位的低动态范围，而 `0x4745=0x00` 能恢复 raw gray96 的有效动态范围。因此正式数据集不再使用旧 `lshift2` 补偿样本，日志必须显示 `scale=raw`。当前可稳定用于模型输入的是真实场景灰度/Y 通道，彩色/YUV RGB 解释仍有伪彩和噪声。

### 3.3 雷达、声光与语音链路

#### 3.3.1 雷达门控和 UART 语音触发

LD2410B 的 `O` 输出接入 PF8，高电平表示检测到人体存在。它的检测范围是前向扇形，只作为视觉推理门控。PA8 连接声光模块 `IN`，低电平有效。VW553 使用 `PA2/USART1_TX -> PA7/UART2_RX`、`PA3/USART1_RX <- PA6/UART2_TX`，H759 在报警上升沿发送一次 `DANGER\n`，并设置 3000 ms 冷却时间。

## 第4章 软件设计与流程

### 4.1 固件总体流程

#### 4.1.1 初始化与周期状态机

`edgecare_app_init()` 依次初始化报警、雷达、VW553 UART、投票状态和相机，并打印设备、日志、语音 UART 和相机 SCCB 配置。若 OV5640 ID 读取成功，继续执行 QVGA 初始化和 capture probe。`edgecare_app_step()` 周期执行：雷达未触发时置为 `IDLE`、关闭报警并复位投票；雷达触发时运行 `edgecare_run_vision_frame()`，完成抓帧、预处理、推理、投票和报警输出。抓帧或推理失败时系统安全降级为不报警。

图4-1 固件软件流程

资料来源：本作品固件状态机。

### 4.2 图像预处理与采集

#### 4.2.1 gray96 输入生成

预处理把 DCI/DMA 帧缓冲中的 `320x240 YUV422` 转为 `96x96 gray8`。当前路径选择 YUV422 的 Y 分量，并按最近邻方式缩放到 96x96，同时统计 min、max、mean、center 和 checksum。正式运行时关闭 gray96 dump；采集数据集时打开 dump，PC 脚本解析后保存 `.pgm`、`.bmp` 和 `.json`。标签分为 `empty`、`safe` 和 `intrusion`，训练时将 empty 与 safe 合并为非闯入类。

图4-2 gray96 采集与模型特征流程

资料来源：本作品预处理和训练脚本。

### 4.3 模型训练与端侧推理

#### 4.3.1 69 维特征与 Q15 导出

训练脚本读取 `data/raw_gray96/<label>` 样本。每张图提取 69 维特征：bias、全局 mean/min/max/contrast，以及 8x8 网格 64 个区域均值。脚本先划分训练集和验证集输出指标，导出固件权重时再用全部样本训练，生成 `edgecare_model_baseline.h`。

固件侧使用 Q15 特征和 Q15 权重完成点积。当前模型名为 `gray_stats_grid8_logreg_baseline`，特征数为 69，阈值为 0.35，对应 `EDGECARE_BASELINE_THRESHOLD_Q15=11469`。推理结果中 `is_placeholder=0`，表示已替换早期占位模型。

### 4.4 多帧投票与告警

#### 4.4.1 2-of-3 投票与释放迟滞

单帧模型可能受噪声、阴影或边界姿态影响。固件采用 3 帧窗口中至少 2 帧闯入才置报警，解除报警需要连续 2 帧安全结果。雷达未触发时直接复位投票。周期日志保持如下格式：

```text
[ts_ms] state=... radar=... infer_ms=... conf=... alarm=... seq=...
```

运行时还输出短格式 `vision_probe`，用于确认抓帧、模型名称、均值、对比度、置信度、投票和报警状态。

## 第5章 系统测试与分析

### 5.1 相机链路测试

#### 5.1.1 ID、时序、DMA 与 gray96 测试

相机测试包括 ID、DVP 时序、DCI/DMA 完整帧和 gray96 可用性测试。ID 测试读取 `0x300A/0x300B`，期望值为 `0x56 0x40`；完整帧测试检查 `dma=done`、`words=38400`、`remain=0`，对应 `320x240x2 = 153600 bytes`；gray96 测试要求 BMP 可辨认真实场景，且日志显示 `scale=raw`。

当前结论是：真实场景灰度/Y 通道采集已可用于模型输入与数据集采集；彩色/YUV RGB 解释仍有伪彩、噪声或条纹。

### 5.2 数据集与模型测试

#### 5.2.1 类别分布与混淆矩阵

最新模型使用的数据集根为 `data/raw_gray96_aug_indoor_empty_fix_20260620`，共 376 个有效 gray96 样本，variant 为 `jpeg_to_yuv_ref_y02`，scale 为 raw，尺寸为 96x96。

| 类别 | 样本数 | 训练归类 | 说明 |
| --- | ---: | --- | --- |
| empty | 136 | 非闯入 | 危险区和附近无人体/目标物 |
| safe | 100 | 非闯入 | 目标可见但未进入危险区 |
| intrusion | 140 | 闯入 | 目标进入或压到危险区边界 |
| 合计 | 376 | 二分类 | empty 与 safe 合并为非闯入 |

表5-1 数据集类别分布

资料来源：`baseline_metrics.json` 与数据集质检记录。

模型参数为 `grid=8`、`threshold=0.35`、`epochs=200`、`learning_rate=0.05`、`shuffle_seed=7`。验证集与导出模型全量回放结果如下。

| 测试口径 | 样本数 | 准确率 | TP | TN | FP | FN |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 验证集 | 75 | 84.0% | 21 | 42 | 12 | 0 |
| 全量回放 | 376 | 87.5% | 138 | 191 | 45 | 2 |

表5-2 模型验证与全量回放结果

资料来源：`baseline_metrics.json`。

结果表明该模型已能完成第一版演示闭环，但 FP 仍偏高，全量回放仍有 2 个 FN。后续应继续补充边界 safe、不同光照 empty 和真实闯入姿态样本。

### 5.3 上板运行测试

#### 5.3.1 构建、烧录和运行日志

最新正式模型固件构建结果为 `0 Error(s), 0 Warning(s)`，程序尺寸为 `Code=24838 RO-data=7434 RW-data=8 ZI-data=167008`。通过 GD-Link/Keil 下载后，日志显示 `Erase Done.Programming Done.Verify OK.Application running`。在当前 empty 场景且雷达触发时，COM8 约 20 秒内连续输出 `vision_probe ... conf=0.00 intrusion=0 votes=0/2 alarm=0`，周期日志保持 `state=IDLE radar=1 infer_ms=1 conf=0.00 alarm=0`。

| 指标 | 当前结果 | 说明 |
| --- | --- | --- |
| Keil 构建 | 0 Error(s), 0 Warning(s) | 正式模型固件可构建 |
| 相机输入 | 320x240 YUV422 -> 96x96 gray8 | 真实场景灰度/Y 通道可用 |
| 模型名称 | gray_stats_grid8_logreg_baseline | 69 维 Q15 线性模型 |
| 推理耗时 | 约 1 ms | 固件日志 `infer_ms=1` |
| 报警策略 | 2-of-3 + release2 | 多帧投票与安全释放 |
| 语音触发 | `DANGER\n` | H759 USART1 到 VW553 UART2 |

表5-3 上板运行指标

资料来源：Keil 构建、串口日志和固件宏定义。

### 5.4 局限性分析

#### 5.4.1 数据、模型、相机与雷达边界

当前系统仍有四点局限：数据集规模较小，对新机位和新光照的泛化能力有限；模型是轻量统计基线，表达能力有限；OV5640 彩色/YUV 解释尚未作为完成成果；LD2410B 检测区域并非精确危险区边界，必须作为门控而非最终判定源。

## 第6章 总结

本文设计并实现了 EdgeCare 端侧工业危险区闯入检测终端。系统以 GD32H759I-START 为主控，融合 LD2410B 雷达与 OV5640 相机，在 MCU 本地完成 gray96 图像采集、轻量模型推理、多帧投票以及声光/语音告警。项目完成了从硬件接线、相机 BSP 调试、数据集采集、训练脚本、模型导出到固件上板验证的闭环。

后续工作包括扩大数据集规模，补充更多光照、边界、遮挡和真实人体姿态样本；在保持 MCU 实时性的前提下尝试更强的小模型；继续完善 OV5640 彩色/YUV 解释；增加网络上报或本地显示模块。

## 参考文献

[1] 第二十一届中国研究生电子设计竞赛组委会. 技术论文格式要求. 2026.

[2] GigaDevice. GD32H759I-START Board User Manual and Schematic. 2026.

[3] GigaDevice. GD32H7xx User Manual and Firmware Library. 2026.

[4] OmniVision. OV5640 Datasheet. 2011.

[5] OmniVision. OV5640 Camera Module Software Application Notes. 2011.

[6] Hi-Link. HLK-LD2410B 24G Human Presence Sensing Module Manual. 2024.

[7] EdgeCare Project. `doc/01_数据集采集全流程手册.md`, `doc/02_硬件引脚与接线说明.md`, `doc/04_OV5640相机调试记录.md`, 2026.

[8] EdgeCare Project. `tools/train_gray96_baseline.py`, `edgecare_app.c`, `edgecare_infer.c`, `edgecare_preprocess.c`, `bsp_camera_ov5640.c`, 2026.
