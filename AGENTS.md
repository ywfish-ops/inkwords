# AGENTS.md — SPBCN 单词卡 (ePaper 版) 多 AI 协同开发指南

本项目把 SPBCN 词表、412 个发音 MP3 与双面翻转单词闪卡 + Leitner 5 盒间隔复习做到 **微雪 ESP32-S3-ePaper-3.97** 开发板。  
详细的架构与 UI 规范请参见同目录下的 [`PLAN.md`](PLAN.md)。

---

## 1. 硬件规格与已验证引脚速查表 (Verified Pinout)

> [!IMPORTANT]
> 以下引脚已通过原厂 16MB 固件二进制机器码逆向与微雪官方 BSP 双重验证，所有 Agent 编写驱动时**必须严格引用 [`include/board_config.h`](include/board_config.h)**，严禁自行猜测引脚！

- **MCU**: ESP32-S3R8 (双核 240MHz, **16MB Flash**, **8MB Octal PSRAM**)
- **墨水屏 (3.97寸 800×480 SSD1677, SPI3)**：
  - `DC = GPIO 9`, `CS = GPIO 10`, `SCK = GPIO 11`, `MOSI = GPIO 12`, `RST = GPIO 46`, `BUSY = GPIO 3`
  - **注意**：物理屏幕为 `800×480`（`BUSY == 1` 为忙，等待变为 `0`），UI 层统一使用 **竖屏 `480×800`** 逻辑坐标！
- **三向拨轮与侧边按键 (Active LOW, 需启用内部上拉)**：
  - `DIAL_UP = GPIO 4`（物理上拨：正面看答案 / 背面“没记住” / 菜单上移）
  - `DIAL_CONFIRM = GPIO 5`（按压中键：正面翻面 / 菜单确认）
  - `DIAL_DOWN = GPIO 6`（物理下拨：正反面都是“记住了” / 菜单下移）
  - `BTN_BOOT = GPIO 0`（单击：重播发音；双击：打开/关闭设置菜单；长按：全局全刷清残影）
  - `BTN_PWR = GPIO 1`（电源管理键）
- **TF / MicroSD 卡槽 (SDMMC 4-bit / 1-bit)**：
  - `CLK = GPIO 16`, `CMD = GPIO 17`, `D0 = GPIO 15`, `D1 = GPIO 7`, `D2 = GPIO 8`, `D3 = GPIO 18`
  - 挂载路径：`/sdcard`，项目数据根目录：`/sdcard/spell`
- **I2C 总线 (`I2C_NUM_0`)**：
  - `SDA = GPIO 41`, `SCL = GPIO 42`
  - 挂载设备：`AXP2101 PMIC (0x34)`、`ES8311 Audio (0x18)`、`PCF85063 RTC (0x51)`、`QMI8658 IMU (0x6B)`、`SHTC3 (0x70)`
- **I2S 音频 (`ES8311` + 功放控制)**：
  - `MCLK = GPIO 13`, `BCLK = GPIO 14`, `WS = GPIO 47`, `DOUT = GPIO 48`, `DIN = GPIO 21`
  - `PA_CTRL = GPIO 39`（高电平开启扬声器功放）

---

## 2. 多 AI 角色分工与模块边界 (Agent Roles)

| 角色 (Agent Role) | 负责目录与文件 | 核心职责 |
| :--- | :--- | :--- |
| **BSP & Storage Agent (底层驱动与存储)** | `src/bsp/pmic_*`, `src/bsp/sdcard_*`, `src/bsp/input_*`, `src/bsp/rtc_*`, `tools/prepare_sdcard.py` | 负责 AXP2101 供电初始化、SDMMC TF 卡挂载、拨轮按键防抖状态机、以及打包 TF 卡数据的脚本 |
| **Display & UI Agent (墨水屏与竖屏界面)** | `src/bsp/epd_3in97.*`, `src/ui/*` | 负责 SSD1677 全刷/局刷驱动、480×800 竖屏画布旋转映射、中英音标渲染、卡片正反面与 5 盒状态栏视觉 |
| **Core & SRS Agent (词库与复习算法)** | `src/core/*`, `include/spell_types.h` | 负责将 `/sdcard/spell/decks/*.json` 加载到 PSRAM、实现 Leitner 5盒间隔调度算法 (`[0,1,2,4,7,14]`天)、分类过滤与进度 JSON 持久化 |
| **Audio Agent (MP3 语音播放)** | `src/bsp/audio_es8311.*` | 负责 ES8311 编解码器初始化、I2S 音频流输出、读取 `/sdcard/spell/audio/{slug}.mp3` 解码播放（非阻塞 FreeRTOS 音频任务） |

---

## 3. 关键协同与防踩坑准则 (Collaboration Rules)

1. **物理串口独占锁 (`/dev/cu.usbmodem*`)**：
   - 任何时刻只能有一个任务打开板子的串口 (插不同 USB 口时编号会变, 先 `ls /dev/cu.usbmodem*`)。执行烧录或读取日志前先用 `lsof` 检查是否被占用。
2. **PSRAM 内存优先原则**：
   - 墨水屏帧缓冲 (`48,000` 字节 × 双缓冲)、词表数组（100~400词）、MP3 解码缓冲必须分配在 **8MB PSRAM**（`ps_malloc()` 或 `heap_caps_malloc(..., MALLOC_CAP_SPIRAM)`），严禁挤占宝贵的片内 SRAM。
3. **墨水屏刷新健康度**：
   - 翻面揭晓答案与切换卡片默认使用 **局部刷新 (`EPD_DisplayPart`)** 以获得 ~300ms 的瞬时响应；每连续局刷 30 次或退出设置菜单 (长按 `BOOT` 打开) 时执行一次 **全局全刷 (`EPD_Display`)**。
4. **原厂固件与备份保护**：
   - 本地的原厂固件备份 (`factory_*.bin`, 由 `backup_firmware.sh` 生成) 与 `xiaozhi/` 为重要备份资产，不入库、严禁删除。

---

## 4. 常用开发命令

```bash
# 1. 生成 TF 卡数据包 (词表源文件不随仓库提供; 没有的话直接用 sample_sdcard/ 示例)
python3 tools/prepare_sdcard.py --full <完整词表目录> --blank <挖空例句与音频目录>

# 2. 编译固件
~/.venv-esp/bin/pio run

# 3. 烧录固件至开发板
~/.venv-esp/bin/pio run -t upload

# 4. 查看串口日志
~/.venv-esp/bin/pio device monitor -b 115200
```
