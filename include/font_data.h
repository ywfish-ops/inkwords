#ifndef FONT_DATA_H
#define FONT_DATA_H

#include <stdint.h>
#include <stddef.h>

/**
 * @file font_data.h
 * @brief 墨水屏 1-bit 点阵字体定义 (Font16, Font20, Font24, Font32, Font48, Font64)
 *        支持完整 ASCII, 国际音标 IPA, 以及 SPBCN 词库与 UI 中文
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
extern const FontDef Font64;  // 仅 ASCII
extern const FontDef Font32;  // 仅 ASCII + 词库音标
extern const FontDef Font20;  // 仅 ASCII + UI 文案

// 高效二分查找字符点阵
const GlyphInfo* font_find_glyph(const FontDef& font, uint32_t unicode);

#endif // FONT_DATA_H
