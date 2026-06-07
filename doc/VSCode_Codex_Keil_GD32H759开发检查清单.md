# VS Code + Codex + Keil 开发 GD32H759 检查清单

更新时间：2026-06-06

## 推荐工作流

主流程：

```text
VS Code + Codex：编辑代码、拆模块、修编译错误、写文档
Keil uVision：编译、烧录、断点调试、查看寄存器/变量
GD-Link 或 J-Link：SWD 下载和调试
```

短期不建议把 OpenOCD 作为主烧录工具。当前已验证 OpenOCD 能通过 GD-Link 连接 GD32H759 的 Cortex-M7 内核，但标准 OpenOCD 没有可靠的 GD32H759 Flash 烧录支持。

## Codex 是否有对应 Skill

本地没有发现 Keil、GD32、EIDE、OpenOCD 专用 Codex skill。

可用的通用 skill：

- `systematic-debugging`：遇到 Keil 编译错误、烧录失败、外设不工作时，用于按证据排查。
- `project-kanban-github`：维护 `看板.md`、记录当前硬件状态、阻塞项和下一步。
- `verification-before-completion`：在声称某个模块跑通前，要求给出实际验证证据。
- `karpathy-guidelines`：写代码时避免过度抽象和不必要复杂化。

实际配合方式：

```text
你在 Keil 编译/烧录/调试
把错误日志、截图、串口输出、现象贴给 Codex
Codex 修改 .c/.h/.md 或给出下一步排查命令
```

## VS Code 写 Keil 工程代码要注意什么

### 1. Keil 工程是唯一可信构建入口

不要同时维护多套工程配置。初赛阶段以 Keil 的 `.uvprojx` 为准。

VS Code/Codex 主要改：

- `.c`
- `.h`
- `.s` 启动文件只在必要时改
- `.md`

谨慎修改：

- `.uvprojx`
- `.uvoptx`
- scatter 文件，例如 `.sct`
- Pack/RTE 生成文件
- 启动文件和系统时钟文件

### 2. 新增源文件后必须加入 Keil 工程

Codex 新建了 `radar_ld2410.c` 或 `camera_ov5640.c` 后，Keil 不会自动编译它。需要在 Keil 左侧工程树里手动 `Add Existing Files to Group`。

检查点：

- 文件是否在 Keil 工程树里
- 头文件目录是否加入 Include Paths
- 是否有重复定义或同名模块

### 3. Include Paths 要以 Keil 为准

VS Code 能跳转不代表 Keil 能编译。Keil 里要检查：

```text
Options for Target -> C/C++ -> Include Paths
```

常见路径：

- `Core/Inc`
- `App`
- `Drivers/...`
- `CMSIS/...`
- `GD32H7xx_standard_peripheral/...`

### 4. 宏定义必须同步

Keil 中的预定义宏在：

```text
Options for Target -> C/C++ -> Define
```

常见宏可能包括：

- 芯片型号宏
- 使用标准外设库或 HAL 的宏
- 外部晶振频率相关宏
- 调试日志开关

Codex 写代码时不能假设宏已经存在。若新增宏，必须记录到 Keil 配置。

### 5. 不要让 Codex 大改启动和时钟

GD32H759 的系统时钟、Cache、MPU、TCM/AXI SRAM、外设时钟比较复杂。除非明确定位到问题，不要随便重写：

- `system_gd32h7xx.c`
- `startup_gd32h7xx.s`
- linker/scatter 配置

### 6. 生成代码要保持 MCU 约束

Codex 写代码时要明确限制：

- 不用动态大内存分配
- 避免 `printf` 大量浮点输出
- DMA buffer 要考虑对齐和 cache coherency
- 中断里只做轻量处理
- 摄像头帧缓存不要放栈上
- 日志接口必须可关闭

## Keil 中编译要配置什么

### 1. Device / Pack

检查：

```text
Project -> Select Device for Target
```

应选择 GD32H759 对应具体型号。若找不到，需要安装 GigaDevice GD32H7xx DFP/Pack。

为什么重要：

- 设备包提供启动文件、SVD、Flash Algorithm、器件参数。
- CMSIS-Pack 的 DFP 通常包含 SVD、Flash Programming Algorithms、设备支持文件和示例工程。

### 2. Target

检查：

```text
Options for Target -> Target
```

重点：

- Xtal / HCLK 配置是否和板子一致
- IROM 起始地址通常是 `0x08000000`
- IRAM 区域是否和 GD32H759 SRAM 分布匹配
- 是否启用 MicroLIB，按工程需要决定

### 3. Output

检查：

```text
Options for Target -> Output
```

建议：

- 勾选生成 `.hex`
- 保留 `.axf`，调试需要符号
- 输出目录固定，方便 Codex/VS Code 找日志和产物

### 4. C/C++

检查：

```text
Options for Target -> C/C++
```

重点：

- Optimization 初期用 `-O0` 或较低优化，方便调试。
- Include Paths 完整。
- Define 宏完整。
- Warnings 不要全部忽略。

### 5. Linker / Scatter

检查：

```text
Options for Target -> Linker
```

重点：

- 使用正确 scatter 文件。
- Flash/RAM 地址和芯片一致。
- 大 buffer 是否放到合适 SRAM 区域。
- 模型数组是否占用过多 Flash。

## Keil 中烧录要配置什么

### 1. Debug Adapter

检查：

```text
Options for Target -> Debug
```

如果用 GD-Link：

- 选择 CMSIS-DAP Debugger 或 GD-Link 兼容项。
- 接口选 SWD。
- 降低初始 SWD 速度，例如 1 MHz 或 4 MHz。

如果用 J-Link：

- 选择 J-LINK / J-TRACE Cortex。
- Device 选择 GD32H759 具体型号。
- 接口选 SWD。

### 2. Flash Download

检查：

```text
Options for Target -> Utilities
Options for Target -> Debug -> Settings -> Flash Download
```

重点：

- Programming Algorithm 是否是 GD32H759/GD32H7 对应算法。
- 勾选 `Erase Sectors` 或按需全擦。
- 勾选 `Program`。
- 勾选 `Verify`。
- 勾选 `Reset and Run`，按调试需要决定。

如果 Flash Algorithm 不对，会出现：

- 下载失败
- 校验失败
- 程序烧进去了但不运行
- Flash 起始地址/扇区识别异常

### 3. GD-Link 驱动

当前电脑已识别 GD-Link：

```text
USB\VID_28E9&PID_058F
CMSIS-DAP FW Version = 2.0.0
```

如果 Keil 不识别：

- 检查设备管理器是否有 HID 设备。
- 换 USB 数据线。
- 换 USB 口。
- 降低 SWD speed。
- 确认板子供电和 BOOT 状态。

## Keil 中调试要配置什么

### 1. Debug 页

检查：

```text
Options for Target -> Debug
```

重点：

- 使用真实硬件调试器，不是 Simulator。
- Load Application at Startup：建议勾选。
- Run to main：建议初期勾选。
- Initialization File：一般先不填，除非外部 SDRAM/OSPI 需要初始化。

### 2. SVD / 寄存器视图

如果 Pack 正确，Keil 通常能显示外设寄存器。若没有：

- 检查 GD32H7 DFP 是否安装。
- 检查选择的 Device 是否正确。

### 3. Debug 初期建议

初期只做：

- 断点停在 `main`
- 单步初始化 GPIO/USART
- 看 SystemCoreClock
- 看外设寄存器时钟是否打开
- 用串口输出最小日志

不要一开始就调 DCMI/DMA/Cache/AI 模型全链路。

## 可做的 VS Code 任务

Keil 官方支持命令行：

```powershell
UV4.exe -b PROJECT.uvprojx -o build.log
UV4.exe -f PROJECT.uvprojx -o flash.log
UV4.exe -d PROJECT.uvprojx
```

其中：

- `-b`：构建工程
- `-f`：下载到 Flash
- `-d`：启动调试模式
- `-o`：输出日志文件

如果能找到 `UV4.exe` 或 `UV4.com`，可以做 VS Code `tasks.json`，让 Codex 读取 `build.log` 自动修编译错误。

注意：本机当前尚未定位到 `UV4.exe` 路径。找到后再配置。

## 参考文件

本地：

- `doc/AN264 在VS Code中使用EIDE插件开发GD32 MCU_Rev1.0 (1).pdf`
- `doc/hardware_manuals/GD32H759xx_Datasheet_Rev2.2.pdf`
- `doc/hardware_manuals/GD32H73x_75x_User_Manual_Rev1.8.pdf`
- `doc/hardware_manuals/GD32H7xx_Demo_Suites_V2.1.0.7z`
- `doc/hardware_manuals/AN126_GD32H7xx_BootLoader_Precautions_Rev1.0.pdf`

官方：

- Keil command line: https://www.keil.com/support/man/docs/uv4/uv4_commandline.asp
- Keil program flash example: https://www.keil.com/support/man/docs/uv4cl/uv4cl_cl_programflash.htm
- CMSIS-Pack DFP 内容说明: https://arm-software.github.io/CMSIS_5/5.7.0/Pack/html/createPack_DFP.html
- EIDE 导入项目: https://em-ide.com/en/docs/getting-started/import_prj
- EIDE Keil 导入限制: https://em-ide.com/en/docs/notice/keil_project_limit/

## 每次提交给 Codex 排查时最好提供

- Keil 完整 build log。
- `.uvprojx` 所在路径和 target 名称。
- 报错文件和行号。
- 当前使用的下载器：GD-Link 或 J-Link。
- Keil Debug/Utilities 里选择的调试器和 Flash Algorithm 截图。
- 串口输出。
- 板子当前 BOOT 拨码/跳线状态。

## 最小检查顺序

1. Keil 能打开官方 GD32H759 demo 工程。
2. Keil 能编译 demo。
3. Keil 能用 GD-Link 下载 demo。
4. Keil 能断点停在 `main`。
5. VS Code/Codex 修改一个 `.c` 文件。
6. Keil 重新编译确认修改生效。
7. 再开始移植项目代码。
