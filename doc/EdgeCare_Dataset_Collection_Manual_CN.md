# EdgeCare 数据集采集全流程手册

更新日期：2026-06-16

本手册用于采集 EdgeCare 初赛模型训练数据。当前目标不是采集彩色照片，而是采集和 GD32H759 端侧模型输入完全一致的 `96x96 gray8` 灰度图。

当前相机结论：

- 可用路径：`OV5640_JPEG_TO_YUV_REF + DCI rising + 0x4745=0x00`。
- 采集样本：`jpeg_to_yuv_ref_y02` 变体的 raw `gray96`。
- 不再使用旧的 `lshift2` 补偿样本。
- 彩色/YUV RGB 图像仍有伪彩和噪声，暂不作为训练数据来源。
- `edgecare_infer()` 仍是 `stat_placeholder`，采集数据时不要使用它的置信度当标签。

## 1. 采集目标

本阶段先做固定机位危险区二分类数据集：

| 标签 | 含义 | 后续训练归类 |
| --- | --- | --- |
| `empty` | 危险区和附近没有人或目标物 | 非闯入 |
| `safe` | 人或目标物可见，但没有进入危险区 | 非闯入 |
| `intrusion` | 人或目标物进入、跨入或明显压到危险区边界 | 闯入 |

建议先保留三个文件夹，训练时再把 `empty` 和 `safe` 合并为非闯入类。这样后续分析误报时能看清问题来自空场景还是边界外干扰。

## 2. 硬件准备

必需设备：

- `GD32H759I-START` 开发板。
- OV5640 DVP 并口相机模块。
- USB-TTL/CH340 串口线，当前日志口为 `COM8`。
- GD-Link、Keil 下载，或外接 J-Link 命令行下载。
- 稳定的板卡供电和公共地。

当前 OV5640 接线必须按下面这版：

| Camera | GD32H759I-START |
| --- | --- |
| `PCLK` | `PA6` |
| `HREF` | `PA4` |
| `SYNC` / `VSYNC` | `PB7` |
| `D0` | `PC6` |
| `D1` | `PC7` |
| `D2` | `PC8` |
| `D3` | `PG11` |
| `D4` | `PC11` |
| `D5` | `PB6` |
| `D6` | `PE5` |
| `D7` | `PB9` |
| `SCL` | `PF1` |
| `SDA` | `PF0` |
| `RES` | `PD0` |
| `PWON` | `PD1` |
| `3V3` | `3V3` |
| `GND` | `GND` |

串口接线：

| USB-TTL | GD32H759I-START |
| --- | --- |
| RXD | `PF4 / USART0_TX` |
| TXD | `PF5 / USART0_RX`，当前可不接 |
| GND | `GND` |

采集前检查：

- 相机固定，采集中不能移动。
- 危险区边界固定，最好用胶带或明显标记。
- 镜头能同时看到危险区、边界、背景参照物。
- 光照稳定，避免强背光和频闪。
- 串口工具、Keil Serial、PuTTY、MobaXterm 等不要占用 `COM8`。

## 3. 固件配置

检查文件：

```text
EdgeCare_GD32_Industrial_Violation_Terminal/GD32H759I_START_Demo_Suites/Projects/01_EdgeCare_Industrial_Violation_Terminal/board/board_config.h
```

采集固件应保持：

```c
#define EDGECARE_CAMERA_NORMAL_OUTPUT_JPEG_TO_YUV_REF 1U
#define EDGECARE_CAMERA_JPEG_TO_YUV_DCI_RISING 1U
#define EDGECARE_CAMERA_DATA_ORDER_DEFAULT 0x00U
#define EDGECARE_ENABLE_GRAY96_LSHIFT2_COMPENSATION 0U
#define EDGECARE_ENABLE_GRAY96_DUMP 1U
#define EDGECARE_ENABLE_GRAY96_DUMP_VARIANTS 1U
#define EDGECARE_ENABLE_CAMERA_BYTE_PLANE_DUMP 0U
```

关键判断：

- 启动日志必须出现 `data_order_default=0x00` 或 `data_order=0x00/read0x00`。
- `preprocess_gray96` 必须显示 `scale=raw`。
- 如果看到 `scale=lshift2`，说明烧录的是旧固件，不要继续采集。
- 如果 `EDGECARE_ENABLE_GRAY96_DUMP=0U`，串口不会输出图像样本。

## 4. 编译和烧录

从项目根目录运行：

```powershell
cd "D:\MyProject\GRADUATE ELECTONICS DESIGN CONTEST\EdgeCare_GD32_Industrial_Violation_Terminal"
powershell -ExecutionPolicy Bypass -File .\build_keil.ps1
```

构建成功标准：

```text
0 Error(s), 0 Warning(s)
```

下载方式二选一：

- Keil/GD-Link：打开 Keil 工程后 Download，再按板子 RESET。
- J-Link 命令行：确认 J-Link/SWD 可用后再执行。

J-Link 示例：

```powershell
cd "D:\MyProject\GRADUATE ELECTONICS DESIGN CONTEST"
powershell -ExecutionPolicy Bypass -File .\tools\verify_edgecare.ps1 -SkipSafetyScan
powershell -ExecutionPolicy Bypass -File .\tools\jlink_flash_edgecare.ps1 -Flash -JLinkSerial 150710308 -SpeedKHz 100
```

如果 J-Link 不稳定，直接退回 Keil/GD-Link 手动下载，不要在采集当天继续消耗时间排查下载器。

## 5. 首次自检

关闭所有占用 `COM8` 的串口窗口，然后运行一个小样本：

```powershell
cd "D:\MyProject\GRADUATE ELECTONICS DESIGN CONTEST"
py .\tools\collect_gray96_serial.py --label empty --count 2 --variant jpeg_to_yuv_ref_y02 --kind gray96 --port COM8
```

脚本启动后，每采一张按一次板子 RESET。看到 `saved ...bmp` 后再按下一次 RESET。

输出位置：

```text
data/raw_gray96/empty/
```

打开保存的 `.bmp`，必须能辨认真实场景的大体轮廓，例如桌面、显示器、危险区标记、人体或目标物。第一次自检不合格时，不要开始批量采集。

## 6. 正式采集流程

建议先采小规模平衡集：

```powershell
py .\tools\collect_gray96_serial.py --label empty --count 20 --variant jpeg_to_yuv_ref_y02 --kind gray96 --port COM8
py .\tools\collect_gray96_serial.py --label safe --count 20 --variant jpeg_to_yuv_ref_y02 --kind gray96 --port COM8
py .\tools\collect_gray96_serial.py --label intrusion --count 20 --variant jpeg_to_yuv_ref_y02 --kind gray96 --port COM8
```

每条命令运行期间：

1. 摆好当前标签对应的场景。
2. 按 RESET。
3. 等脚本打印 `saved`。
4. 轻微改变人或目标物位置、姿态、距离或边界附近状态。
5. 继续按 RESET，直到达到 `--count`。

第一批建议数量：

| 阶段 | 数量 | 目标 |
| --- | --- | --- |
| 干跑 | 每类 2 张 | 验证固件、串口、图片可视性 |
| 小平衡集 | 每类 20 张 | 验证标签定义和训练闭环 |
| 初版训练集 | 每类 50-100 张 | 训练第一版模型 |
| 增强集 | 按误报/漏报补采 | 提升边界场景表现 |

## 7. 每类场景怎么拍

`empty`：

- 固定机位下的空危险区。
- 不同光照：正常、偏暗、偏亮。
- 背景中允许有不会进入危险区的固定物体。
- 不要把手、线、影子临时伸进危险区。

`safe`：

- 人在危险区外经过。
- 人靠近边界但没有压线。
- 手、工具或目标物在画面里但不进入危险区。
- 包含容易误报的边界外近距离样本。

`intrusion`：

- 人或目标物明显进入危险区。
- 只进入一部分、压线、从不同方向进入。
- 靠近边界的轻微闯入。
- 不同距离、姿态、遮挡和亮度。

重要原则：

- 标签按真实危险区规则定，不按图像好不好看定。
- 边界模糊样本先少拍，等第一版模型跑通后再补。
- 一次采集过程中不要改变相机位置；换机位要作为新 session 记录。

## 8. 输出文件说明

脚本会保存到：

```text
data/raw_gray96/<label>/
```

每个样本包含三类文件：

| 文件 | 用途 |
| --- | --- |
| `.pgm` | 训练程序优先读取，二进制 P5 gray8 |
| `.bmp` | Windows 直接预览，人工质检用 |
| `.json` | 标签、尺寸、校验和、采集时间等元数据 |

示例文件名包含：

```text
时间戳_标签_类型_变体_尺寸_校验和
```

`data/` 已被 `.gitignore` 排除。不要把数据集、原始样张、模型权重上传 GitHub。

## 9. 质量检查

每采完一个标签，至少检查前 3 张和最后 3 张 BMP：

```powershell
explorer .\data\raw_gray96\empty
explorer .\data\raw_gray96\safe
explorer .\data\raw_gray96\intrusion
```

合格样本：

- 能辨认真实场景结构。
- 标签和画面内容一致。
- 危险区边界或参照物仍在画面中。
- 没有明显串口丢包、纯黑、纯白、重复条纹或旧补偿特征。

应剔除样本：

- `scale=lshift2` 旧固件采到的样本。
- `variant` 不是 `jpeg_to_yuv_ref_y02` 的样本。
- 图片看不出真实场景。
- 拍摄时相机被碰动。
- 标签无法判断。
- 人还没摆好就按 RESET。

如果一批里连续多张质量异常，停止采集，回到“首次自检”重新确认固件和相机。

## 10. 故障排查

`could not open COM8`：

- 关闭 VS Code Serial Monitor、Keil 串口窗口、PuTTY、MobaXterm、HLK 工具。
- 拔插 CH340 后确认设备管理器里的 COM 号。
- 必要时把命令里的 `--port COM8` 改成实际 COM 号。

脚本一直等待，没有保存：

- 确认烧录的是 `EDGECARE_ENABLE_GRAY96_DUMP=1U` 固件。
- 按一次板子 RESET。
- 确认 CH340 RXD 接 `PF4 / USART0_TX`，GND 共地。
- 串口参数必须是 `115200 8N1`。

日志里有 `scale=lshift2`：

- 这是旧采集路径。重新编译烧录当前固件。
- 当前训练数据只接受 `scale=raw`。

日志里 `data_order=0x02`：

- 这是旧默认。重新确认 `EDGECARE_CAMERA_DATA_ORDER_DEFAULT=0x00U` 并重新烧录。

BMP 有横线但能看出真实场景：

- 当前可以先用于小规模 MVP 数据集。
- 采集后训练时以模型效果判断是否需要继续修相机。
- 不要把它称为彩色照片成功。

BMP 完全看不出真实场景：

- 不要批量采集。
- 检查 `0x4745=0x00`、`scale=raw`、相机接线、供电和是否烧录到正确板子。
- 必要时运行 `doc/EdgeCare_Data_Collection.md` 中的 byte-plane 诊断。

## 11. 采集记录模板

每次采集建议在本地另存一份记录，不需要提交 Git：

```text
session_id:
date:
firmware_commit:
camera_position:
danger_zone_description:
lighting:
labels_and_counts:
operator:
notes:
```

`firmware_commit` 可用下面命令查询：

```powershell
git rev-parse --short HEAD
```

## 12. 数据集完成标准

第一版可训练数据集至少满足：

- `empty/safe/intrusion` 每类至少 20 张。
- 三类样本数量接近平衡。
- 全部来自当前 raw `gray96` 固件，不混入旧 `gray96_lshift2_mvp`。
- 人工检查 BMP 后确认可辨认真实场景。
- 采集时机位固定，危险区规则一致。

第一版训练跑通后，再根据误报/漏报补采：

- 漏报多：补 `intrusion` 的边界、遮挡、远距离样本。
- 误报多：补 `safe` 的边界外、路过、背景干扰样本。
- 光照敏感：补不同亮度下的三类样本。

## 13. GitHub 提交规则

可以提交：

- 固件源码。
- 采集脚本。
- 数据采集手册。
- 相机 bring-up 记录。
- 看板和 README。

禁止提交：

- `data/` 下采到的图片。
- `.embeddedskills/logs/` 串口日志。
- Keil `Objects/`、`Listings/`、`.axf`、`.hex`、`.map`。
- 模型权重、训练输出、视频、PDF、SDK 压缩包、密钥。

提交前运行：

```powershell
git diff --check
powershell -ExecutionPolicy Bypass -File .\tools\verify_edgecare.ps1 -SkipSafetyScan
```
