#ifndef FONT_DATA_H
#define FONT_DATA_H

#include <stdint.h>
#include <stddef.h>

/**
 * @file font_data.h
 * @brief 墨水屏 1-bit 点阵字体定义 (Font16, Font20, Font24, Font32, Font48, Font64)
 *        内置 GB2312 全部 6763 个汉字、中文标点、带重音拉丁字母与 IPA 音标, 外加词库与 UI 用到的字
 */

struct GlyphInfo {
    uint32_t unicode;        // Unicode 字符码
    uint8_t  width;          // 点阵宽度
    uint8_t  height;         // 点阵高度
    int8_t   advance;        // 水平步进
    const uint8_t* bitmap;   // 1-bit 点阵数据 ((width + 7) / 8 * height 字节)
};

struct FontDef {
    uint8_t          height;
    uint16_t         glyph_count;
    const GlyphInfo* glyphs; // 按 unicode 升序排列，支持 O(log N) 二分查找
};

extern const FontDef Font16;
extern const FontDef Font24;
extern const FontDef Font48;
extern const FontDef Font64;  // 拉丁字母 (含重音), 无汉字
extern const FontDef Font32;  // 拉丁字母 + 音标, 无汉字
extern const FontDef Font20;  // 仅 ASCII + UI 文案

// 高效二分查找字符点阵
const GlyphInfo* font_find_glyph(const FontDef& font, uint32_t unicode);

#endif // FONT_DATA_H
