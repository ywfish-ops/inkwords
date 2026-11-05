# 第三方组件、字体与素材

本仓库自己的代码使用 [MIT 许可](LICENSE)。下列内容来自第三方或另有说明，按各自的许可使用。

| 内容 | 位置 | 来源 | 许可 |
|------|------|------|------|
| minimp3（MP3 解码） | `lib/minimp3/` | [lieff/minimp3](https://github.com/lieff/minimp3) | CC0 1.0，见 `lib/minimp3/LICENSE` |
| ArduinoJson（JSON 解析） | 构建时由 PlatformIO 下载，不随仓库分发 | [bblanchon/ArduinoJson](https://github.com/bblanchon/ArduinoJson) | MIT |
| 微雪板级参考代码 | `docs/bsp_reference/` | [78/xiaozhi-esp32](https://github.com/78/xiaozhi-esp32)（Waveshare ESP32-S3-ePaper-3.97 板级支持） | MIT，见 `LICENSES/xiaozhi-esp32-MIT.txt` |
| 点阵字库（英文、数字、音标） | `src/ui/font_data.cpp` | 由 [DejaVu Sans Condensed](https://github.com/dejavu-fonts/dejavu-fonts) 渲染生成 | Bitstream Vera 许可（DejaVu 改动为公有领域），见 `LICENSES/DejaVu-Fonts.txt` |
| 点阵字库（中文与符号） | `src/ui/font_data.cpp` | 由 [Noto Sans CJK SC](https://github.com/notofonts/noto-cjk) Medium 渲染生成 | SIL Open Font License 1.1，见 `LICENSES/Noto-Sans-CJK-OFL-1.1.txt` |
| 示例词库的发音 | `sample_sdcard/spell/audio/` | 文字转语音（TTS）生成 | — |

说明：

- 字体文件本身不随仓库分发（下载方式见 `tools/fonts/README.md`）；仓库里只有由它们渲染出的点阵数据。
- 示例词库（`sample_sdcard/`）的中文释义、英文释义和例句为本项目自行编写。
