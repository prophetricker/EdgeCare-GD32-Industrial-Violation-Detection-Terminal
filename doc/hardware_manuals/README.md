# 工厂违规检测项目硬件手册索引

更新时间：2026-06-03

本目录用于集中保存 EdgeCare 工厂违规检测项目的硬件资料。优先级按初赛必需程度排序。

## 主控与无线

| 硬件 | 本地文件 | 用途 | 来源 |
| --- | --- | --- | --- |
| GD32H759 / GD32H7 主控 | `GD32H759xx_Datasheet_Rev2.2.pdf` | 芯片参数、封装、外设、电气特性 | https://download.gigadevice.com/Datasheet/GD32H759xx%20Datasheet_Rev2.2.pdf |
| GD32H73x/75x 用户手册 | `GD32H73x_75x_User_Manual_Rev1.8.pdf` | 寄存器、外设、DCMI、DMA、UART、SPI、I2C 等 | https://download.gigadevice.com/User_Manual/GD32H73x_75x_User_Manual_Rev1.8.pdf |
| GD32H7 官方 Demo 包 | `GD32H7xx_Demo_Suites_V2.1.0.7z` | GD32H7 评估板例程、板级资料、外设 demo | https://www.gd32mcu.com/data/documents/evaluationBoard/GD32H7xx_Demo_Suites_V2.1.0.7z |
| GD32H7 BootLoader 注意事项 | `AN126_GD32H7xx_BootLoader_Precautions_Rev1.0.pdf` | 系统 BootLoader、USART/DFU/SDIO 烧录入口和注意事项 | https://www.gd32mcu.com/data/documents/applicationNote/AN126_GD32H7xx_BootLoadercaozuozhuyishixiang_Rev1.0.pdf |
| GD32VW553-MD1 | `GD32VW553-MD1_Datasheet_Rev1.0.pdf` | Wi-Fi/BLE 模组硬件参数、引脚、供电 | https://www.gd32mcu.com/data/documents/datasheet/GD32VW553-MD1%20Datasheet%20Rev1.0.pdf |
| GD32VW553 快速开发 | `AN154_GD32VW553_Quick_Development_Guide.pdf` | VW553 快速上手、环境、基础例程 | https://www.gd32mcu.com/data/documents/applicationNote/AN154%20GD32VW553%20Quick%20Development%20Guide.pdf |
| GD32VW553 Wi-Fi 开发 | `AN158_GD32VW553_WiFi_Development_Guide.pdf` | Wi-Fi 开发、联网、协议栈使用 | https://www.gd32mcu.com/data/documents/applicationNote/AN158%20GD32VW553%20Wi-Fi%20Development%20Guide.pdf |

## 感知输入

| 硬件 | 本地文件 | 用途 | 来源 |
| --- | --- | --- | --- |
| OV5640 摄像头 | `OV5640_Datasheet.pdf` | 摄像头寄存器、时序、接口、电气特性 | https://cdn.sparkfun.com/datasheets/Sensors/LightImaging/OV5640_datasheet.pdf |
| HLK-LD2410B 官方资料包 | `HLK_LD2410B_official/` | 官方说明书、串口协议、天线罩设计、上位机工具、APP 下载说明、Arduino 库参考 | https://h.hlktech.com/Mobile/download/FDetail/204.html |
| HLK-LD2410 毫米波雷达 | `HLK-LD2410_User_Manual_usermanual_wiki.pdf` | 雷达 UART 协议、参数配置、检测逻辑 | https://usermanual.wiki/HI-LINK-ELECTRONIC/HLK-LD2410-P-6619974.pdf |
| INMP441 I2S 麦克风 | 未能稳定下载到本地 | 初赛不作为主线；如后续做音频模型，需要看 I2S 时序和供电去耦 | https://product.tdk.com.cn/system/files/dam/doc/product/sw_piezo/mic/mems-mic/data_sheet/inmp441.pdf |

## 显示、执行与电源

| 硬件 | 本地文件 | 用途 | 来源 |
| --- | --- | --- | --- |
| SSD1306 OLED | `SSD1306_Datasheet.pdf` | OLED 控制器命令、I2C/SPI 接口 | https://cdn-shop.adafruit.com/datasheets/SSD1306.pdf |
| SRD-05VDC-SL-C 继电器 | `SRD-05VDC-SL-C_Relay_Datasheet_Seeed.pdf` | 继电器线圈、电气参数、触点容量 | https://files.seeedstudio.com/wiki/Grove-2-Channel_SPDT_Relay/res/SRD_05VDC-SL-C.pdf |
| LM2596 降压芯片 | `LM2596_Datasheet_TI.pdf` | 12V/24V 转 5V 电源模块参考 | https://www.ti.com/lit/ds/symlink/lm2596.pdf |

## 使用建议

- 先看 `GD32H73x_75x_User_Manual_Rev1.8.pdf` 的 DCMI、DMA、GPIO、UART、SPI、I2C 章节。
- 摄像头调试优先看 OV5640 的 SCCB/I2C 配置、输出格式和时序。
- 雷达调试优先看 `HLK_LD2410B_official/LD2410B 串口通信协议 V1.08.pdf` 和官方上位机工具；初赛 Day 2 仍先用 `O` 数字输出做存在触发。
- 初赛阶段 INMP441 音频链路不要作为主线，避免 I2S 采样、底噪和模型训练拖慢进度。
- `GD32H7xx_Demo_Suites_V2.1.0.7z` 里可能包含板级例程、原理图或工程模板，建议解压到临时目录后只拷贝需要的例程，不要直接把整个包提交到 Git。
