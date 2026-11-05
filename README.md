# epaper-word-cards · 墨水屏背单词卡

一台只能背单词的小机器：微雪 ESP32-S3 3.97 寸墨水屏开发板 + TF 卡词库 + 发音，按 Leitner 间隔复习自动安排每天要背的词。没网、没游戏、没推送，全程离线。

这个项目的代码全部由 AI 编程工具（主要是 Claude Code）编写，作者负责提需求、上板测试和拍照验收。

[English summary](#english-summary)

## 能做什么

- 翻卡背单词：正面英文 + 音标（或中文释义），背面中文释义、英文释义和例句
- 两个背卡方向：英 → 中，中 → 英（听发音想拼写）
- 发音：每个词一个 MP3，存在 TF 卡里；可设置切卡自动念
- Leitner 5 盒间隔复习：记住了升一盒，没记住回盒 1，下次到期 = 今天 + [0, 1, 2, 4, 7, 14] 天（按盒）
- 每日新词上限、多套词库切换、按易错类型筛选（在 `userdata/settings.json` 里设置）
- 学习进度存在 TF 卡上，拔卡换机也不丢
- 电池供电时 MCU 自动浅睡眠；长按电源键关机，墨水屏停在关机画面

## 硬件

| 物品 | 说明 |
|------|------|
| 开发板 | [微雪 ESP32-S3-ePaper-3.97](https://www.waveshare.net/shop/ESP32-S3-ePaper-3.97.htm)：ESP32-S3R8（16MB Flash / 8MB PSRAM），3.97 寸 800×480 墨水屏，三向拨轮，喇叭，TF 卡槽，RTC，电源管理芯片 |
| 电池 | 板子配套的锂电池接口（作者用 1500mAh） |
| TF 卡 | 8G～32G 即可，格式化为 **FAT32**；词库加发音只有十几 MB |
| 数据线 | 能传数据的 USB-C 线（只能充电的线电脑认不出板子） |

### 外壳

| 项目 | 说明 |
|------|------|
| 模型 | MakerWorld 上专为这块板设计的开源壳 [FolloUp](https://makerworld.com/zh/models/3180226-folloup)（作者 alxvtoronto） |
| 螺丝 | M3×10，2 颗 |

打印文件请到模型页下载，本仓库不重复分发。

## 操作

| 按键 | 正面 | 背面 |
|------|------|------|
| 拨轮上拨 | 看答案 | 没记住（回盒 1） |
| 拨轮按下 | 翻面 | 翻回正面 |
| 拨轮下拨 | 记住了（升一盒） | 记住了（升一盒） |
| BOOT 短按 | 播放发音 | 播放发音 |
| BOOT 长按 | 打开 / 退出设置 | 打开 / 退出设置 |
| 电源键长按 2 秒 | 关机 | 关机 |

设置菜单：词库、每日新词（10 / 20 / 30 / 50 / 不限）、音量、背卡方向、切卡自动发音。

## 快速开始

### 不会编程？让 AI 帮你

用一个**能在电脑上执行命令**的 AI 编程工具（Claude Code 等，也叫 Agent），把这个仓库地址发给它，说「帮我把这个项目烧录到插在电脑上的微雪 ESP32-S3-ePaper-3.97 板子上」。编译、烧录、看日志它都能自己完成。

### 自己动手

1. 安装 [PlatformIO](https://platformio.org/)（命令行版即可）。
2. 编译并烧录（串口自动查找；插着多块板子时用 `--upload-port` 指定）：
   ```bash
   pio run -t upload
   ```
3. 准备 TF 卡：格式化为 FAT32，把 `sample_sdcard/spell` 整个文件夹拷到卡的根目录，插回板子开机。
4. 串口日志（可选）：
   ```bash
   pio device monitor -b 115200
   ```

烧录前可以先用 `./backup_firmware.sh` 备份板子的原厂固件（整片 16MB）。

## TF 卡与词库格式

```text
/spell/
├── decks/
│   ├── manifest.json      # 词库列表
│   └── <id>.json          # 每套词库一个文件
├── audio/
│   └── <word>.mp3         # 每个词一个发音文件
├── userdata/              # 进度与当前词库选择（板子自动生成）
└── logs/                  # 设备日志（板子自动生成）
```

`decks/manifest.json`：

```json
[
  {"id": "sample", "name": "Sample 示例词库", "sub": "5 个词", "count": 5, "file": "sample.json"}
]
```

`decks/<id>.json`：

```json
{
  "id": "sample",
  "name": "Sample 示例词库",
  "cards": [
    {
      "key": "sample-0",
      "en": "wrong",
      "ipa": "/rɔ:ŋ/",
      "zh": "错误的；不对的",
      "def": "not correct",
      "ex": "I wrote the wrong answer on the test.",
      "ex_blank": "I wrote the ____ answer on the test.",
      "cat": "silent",
      "audio": "wrong.mp3"
    }
  ]
}
```

- `key`（全局唯一，进度按它记录）和 `en` 必填，其余字段可以省略。
- `cat` 是易错类型标签，显示在卡片左上角，可在 `userdata/settings.json` 的 `cat` 里按它筛选；示例用的是 `silent` / `vowel_team` / `double` / `schwa` / `ending`。
- MP3 建议 24kHz 单声道，每个词念一遍。

### 用自己的词库

把自己的词库放到 `sdcard_dist/spell/`（与上面同样的结构，此目录不入库），然后重新生成字库再烧录。点阵字库只收录用到的字，新词库里的中文、音标要靠这一步加进去：

```bash
python3 tools/generate_fonts.py   # 先按 tools/fonts/README.md 下载两个开源字体
pio run -t upload
```

## 工具

| 命令 | 作用 |
|------|------|
| `python3 tools/sdsync.py ls / get / put / push / rm` | 不拔卡，通过 USB 串口读写 TF 卡 |
| `python3 tools/sdsync.py log` | 查看设备日志 |
| `python3 tools/sdsync.py time` | 用电脑时间给板子对时（每次连接也会自动对时） |
| `python3 tools/generate_fonts.py` | 从源码和词库生成点阵字库 `src/ui/font_data.cpp` |
| `pio test -e native` | 在电脑上跑单元测试（不需要板子） |

`sdsync.py` 需要 `pip install pyserial`；`generate_fonts.py` 需要 `pip install pillow`。

## 目录

```text
src/bsp/        板级驱动：墨水屏 SSD1677、电源 AXP2101、音频 ES8311、TF 卡、RTC、按键
src/core/       词库解析、Leitner 调度、设置菜单、日志等（纯逻辑，可在电脑上测试）
src/ui/         480×800 竖屏画布、卡片界面、点阵字库
include/        头文件；引脚定义见 include/board_config.h
test/           单元测试（pio test -e native）
tools/          字库生成、TF 卡读写、数据打包脚本
sample_sdcard/  示例 TF 卡（5 个词）
docs/           板子官方 BSP 参考
AGENTS.md       给 AI 编程工具看的协作说明
MANUAL.md       详细设计文档
```

## 许可

代码使用 [MIT 许可](LICENSE)。第三方组件、字体和示例发音的来源与许可见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。

---

## English summary

A single-purpose flashcard device for spelling-bee vocabulary, built on the Waveshare ESP32-S3-ePaper-3.97 board (3.97" 800×480 e-paper, ESP32-S3, TF card, speaker). Word lists and per-word MP3 pronunciations live on a FAT32 TF card; reviews are scheduled with a 5-box Leitner system and progress is saved back to the card. Fully offline.

- Build & flash with PlatformIO: `pio run -t upload` (serial port auto-detected).
- Copy `sample_sdcard/spell` to the root of a FAT32 TF card to try the 5-word sample deck.
- Deck format, controls and tools are described above; host-side unit tests run with `pio test -e native`.
- All code in this repository was written by AI coding agents (mainly Claude Code), with the author directing and testing on hardware.

Licensed under MIT; see THIRD_PARTY_NOTICES.md for bundled components, fonts and sample audio.
