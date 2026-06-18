# EdgeCare GD32 Industrial Violation Detection Terminal

EdgeCare 是面向研电赛兆易创新 Endpoint AI 方向的工业危险区域闯入检测终端。

系统以 `GD32H759I-START` 为主控，通过 LD2410 提供低功耗人员触发，OV5640 采集固定危险区画面，在 MCU 端完成图像预处理和二分类推理，并驱动声光报警。后续计划通过 VW553 上传结构化告警，不上传视频流。

## 当前状态

截至 2026-06-16：

- Keil MDK 工程可编译，验证结果为 `0 Error(s), 0 Warning(s)`。
- USART0 日志、LD2410 GPIO 触发和低电平报警输出已验证。
- OV5640 SCCB ID 已读到 `0x56 0x40`。
- DCI/DMA 已能取得完整 `320x240 YUV422` 帧。
- 当前可用相机路径为 `OV5640_JPEG_TO_YUV_REF + DCI rising + 0x4745=0x00`。
- 已实现 `YUV422 -> 96x96 gray8` 预处理和串口样本导出，当前可开始小规模 raw `gray96` 数据集采集。
- 彩色/YUV RGB 图像仍有伪彩和噪声，当前训练数据以 `96x96 gray8` 为准，不采集彩色照片。
- `edgecare_infer()` 仍是明确标注的统计占位实现，不是真实 AI 模型。

## 系统链路

```text
LD2410 -> GD32H759 状态机
OV5640 -> SCCB 配置 -> DCI/DMA QVGA 帧
QVGA YUV422 -> gray96 -> edgecare_infer()
推理结果 -> 报警灯/串口日志 -> 后续 VW553 JSON
```

目标日志格式：

```text
[ts_ms] state=... radar=... infer_ms=... conf=... alarm=... seq=...
```

目标上传格式：

```json
{"dev":"edgecare-01","event":"zone_intrusion","conf":0.87,"lat_ms":132,"seq":42}
```

## 仓库入口

- [项目看板](看板.md)：当前进度、阻塞点和下一步。
- [固件工程说明](EdgeCare_GD32_Industrial_Violation_Terminal/README.md)：接线、Keil 下载和串口日志。
- [文档索引](doc/00_先看这里_文档索引.md)：先从这里判断该看哪份文档。
- [数据集采集全流程手册](doc/01_数据集采集全流程手册.md)：从场景搭建、固件配置、烧录、串口采集到质检的中文完整流程。
- [硬件引脚与接线说明](doc/02_硬件引脚与接线说明.md)：GD32H759、雷达、报警灯和摄像头接线。
- [快速接线表](doc/03_快速接线表.md)：按模块列出每根线的连接方式。
- [OV5640 相机调试记录](doc/04_OV5640相机调试记录.md)：OV5640、DCI/DMA 和诊断日志解释。
- [gray96 数据采集与诊断细节](doc/05_gray96数据采集与诊断细节.md)：`gray96` 样本导出和相机诊断细节。
- [开发工作流](doc/06_开发工作流_VSCode_Codex_Keil.md)：VS Code、Codex、Keil 和 Git 流程。
- [固件代码规范](CODING_STYLE.md)：模块边界和嵌入式 C 规则。

## 固件目录

```text
EdgeCare_GD32_Industrial_Violation_Terminal/
  GD32H759I_START_Demo_Suites/
    Projects/01_EdgeCare_Industrial_Violation_Terminal/
      app/        状态机和业务流程
      board/      引脚与尺寸配置
      bsp/        报警、雷达、OV5640
      model/      edgecare_infer() 边界
      platform/   日志
      vision/     YUV422 和 gray96 预处理
```

## 构建

需要：

- Keil MDK-ARM
- `GD32H7xx_DFP.1.5.0.pack`
- 兆易创新 GD32H7xx Firmware Library

下载的完整 SDK、芯片手册和 Keil 构建产物没有上传到 GitHub。将官方 `GD32H7xx_Firmware_Library` 放到：

```text
EdgeCare_GD32_Industrial_Violation_Terminal/GD32H7xx_Firmware_Library/
```

然后运行：

```powershell
cd .\EdgeCare_GD32_Industrial_Violation_Terminal
powershell -ExecutionPolicy Bypass -File .\build_keil.ps1
```

提交前完整检查：

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\verify_edgecare.ps1
```

## 协作约定

当前开发分支是：

```text
feature/firmware-module-refactor
```

队友当前只负责危险区演示场景搭建和拍摄条件记录，暂不修改固件、模型或数据采集脚本。正式训练数据使用当前 raw `gray96` 采集链路；彩色照片质量不作为本阶段阻塞项。

仓库不会提交：

- 原始数据集、模型权重和生成模型
- PDF、压缩包和完整厂商 SDK
- Keil `Objects/`、`Listings/`、AXF、HEX、MAP 和日志
- 视频、APK、密钥和本地配置
