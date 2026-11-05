#include "canvas_480x800.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

Canvas480x800::Canvas480x800(uint8_t* buffer, uint8_t rotation)
    : _buffer(buffer), _rotation(rotation) {}

void Canvas480x800::clear(bool white) {
    if (_buffer) {
        memset(_buffer, white ? 0xFF : 0x00, EPD_BUFFER_SIZE);
    }
}

bool Canvas480x800::mapToPhysical(int x, int y, int& phys_x, int& phys_y) const {
    if (x < 0 || x >= UI_WIDTH || y < 0 || y >= UI_HEIGHT || !_buffer) {
        return false;
    }
    if (_rotation == 3) {
        // 270° 旋转: logical (0,0) 对应物理 (0, 479)
        phys_x = y;
        phys_y = (EPD_NATIVE_HEIGHT - 1) - x;
    } else {
        // 默认 90° 旋转: logical (0,0) 对应物理 (799, 0)
        phys_x = (EPD_NATIVE_WIDTH - 1) - y;
        phys_y = x;
    }
    return true;
}

void Canvas480x800::drawPixel(int x, int y, bool black) {
    int phys_x, phys_y;
    if (!mapToPhysical(x, y, phys_x, phys_y)) return;

    uint32_t index = (uint32_t)phys_y * (EPD_NATIVE_WIDTH / 8) + (phys_x >> 3);
    uint8_t bit = 7 - (phys_x & 0x07);

    if (!black) {
        _buffer[index] |= (1 << bit);   // 白 (1)
    } else {
        _buffer[index] &= ~(1 << bit);  // 黑 (0)
    }
}

bool Canvas480x800::getPixel(int x, int y) const {
    int phys_x, phys_y;
    if (!mapToPhysical(x, y, phys_x, phys_y)) return false;
    uint32_t index = (uint32_t)phys_y * (EPD_NATIVE_WIDTH / 8) + (phys_x >> 3);
    return (_buffer[index] & (1 << (7 - (phys_x & 0x07)))) == 0;
}

void Canvas480x800::drawFastHLine(int x, int y, int w, bool black) {
    if (y < 0 || y >= UI_HEIGHT || w <= 0) return;
    for (int i = 0; i < w; i++) {
        drawPixel(x + i, y, black);
    }
}

void Canvas480x800::drawFastVLine(int x, int y, int h, bool black) {
    if (x < 0 || x >= UI_WIDTH || h <= 0) return;
    for (int i = 0; i < h; i++) {
        drawPixel(x, y + i, black);
    }
}

void Canvas480x800::drawLine(int x0, int y0, int x1, int y1, bool black) {
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    while (true) {
        drawPixel(x0, y0, black);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void Canvas480x800::drawRect(int x, int y, int w, int h, bool black) {
    drawFastHLine(x, y, w, black);
    drawFastHLine(x, y + h - 1, w, black);
    drawFastVLine(x, y, h, black);
    drawFastVLine(x + w - 1, y, h, black);
}

void Canvas480x800::fillRect(int x, int y, int w, int h, bool black) {
    for (int i = 0; i < h; i++) {
        drawFastHLine(x, y + i, w, black);
    }
}

void Canvas480x800::drawRoundRect(int x, int y, int w, int h, int r, bool black) {
    if (r <= 0) {
        drawRect(x, y, w, h, black);
        return;
    }
    drawFastHLine(x + r, y, w - 2 * r, black);
    drawFastHLine(x + r, y + h - 1, w - 2 * r, black);
    drawFastVLine(x, y + r, h - 2 * r, black);
    drawFastVLine(x + w - 1, y + r, h - 2 * r, black);

    // 四角圆弧
    int f = 1 - r;
    int ddF_x = 1;
    int ddF_y = -2 * r;
    int cx = 0;
    int cy = r;

    while (cx < cy) {
        if (f >= 0) {
            cy--;
            ddF_y += 2;
            f += ddF_y;
        }
        cx++;
        ddF_x += 2;
        f += ddF_x;

        drawPixel(x + r - cx, y + r - cy, black);
        drawPixel(x + r - cy, y + r - cx, black);
        drawPixel(x + w - 1 - r + cx, y + r - cy, black);
        drawPixel(x + w - 1 - r + cy, y + r - cx, black);

        drawPixel(x + r - cx, y + h - 1 - r + cy, black);
        drawPixel(x + r - cy, y + h - 1 - r + cx, black);
        drawPixel(x + w - 1 - r + cx, y + h - 1 - r + cy, black);
        drawPixel(x + w - 1 - r + cy, y + h - 1 - r + cx, black);
    }
}

void Canvas480x800::fillRoundRect(int x, int y, int w, int h, int r, bool black) {
    fillRect(x + r, y, w - 2 * r, h, black);
    for (int i = 0; i <= r; i++) {
        int w_cut = (int)sqrtf((float)(r * r - (r - i) * (r - i)));
        drawFastHLine(x + r - w_cut, y + i, w_cut, black);
        drawFastHLine(x + w - r, y + i, w_cut, black);
        drawFastHLine(x + r - w_cut, y + h - 1 - i, w_cut, black);
        drawFastHLine(x + w - r, y + h - 1 - i, w_cut, black);
    }
    fillRect(x, y + r, w, h - 2 * r, black);
}

void Canvas480x800::fillPolygon(const int* xs, const int* ys, int n, bool black) {
    if (n < 3 || n > 16) return;
    int min_y = ys[0], max_y = ys[0];
    for (int i = 1; i < n; i++) {
        if (ys[i] < min_y) min_y = ys[i];
        if (ys[i] > max_y) max_y = ys[i];
    }
    for (int y = min_y; y <= max_y; y++) {
        // 在像素中心高度 (y + 0.5) 求与各边的交点
        float cross[16];
        int count = 0;
        float sy = y + 0.5f;
        for (int i = 0; i < n; i++) {
            int j = (i + 1) % n;
            float y0 = ys[i], y1 = ys[j];
            if ((y0 <= sy && y1 > sy) || (y1 <= sy && y0 > sy)) {
                cross[count++] = xs[i] + (sy - y0) * (xs[j] - xs[i]) / (y1 - y0);
            }
        }
        for (int a = 1; a < count; a++) {  // 插入排序, 交点很少
            float v = cross[a];
            int b = a - 1;
            while (b >= 0 && cross[b] > v) { cross[b + 1] = cross[b]; b--; }
            cross[b + 1] = v;
        }
        for (int k = 0; k + 1 < count; k += 2) {
            int x0 = (int)ceilf(cross[k] - 0.5f);
            int x1 = (int)floorf(cross[k + 1] - 0.5f);
            drawFastHLine(x0, y, x1 - x0 + 1, black);
        }
    }
}

void Canvas480x800::drawPolygon(const int* xs, const int* ys, int n, bool black) {
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        drawLine(xs[i], ys[i], xs[j], ys[j], black);
    }
}

void Canvas480x800::drawDottedHLine(int x, int y, int w, int dot_spacing, bool black) {
    for (int i = 0; i < w; i += dot_spacing) {
        drawPixel(x + i, y, black);
    }
}

uint32_t Canvas480x800::nextUtf8Char(const char*& p) {
    if (!p || *p == '\0') return 0;
    uint8_t c = (uint8_t)*p++;
    if (c < 0x80) {
        return c;
    } else if ((c & 0xE0) == 0xC0) {
        if (*p == '\0') return c;
        uint8_t c1 = (uint8_t)*p++;
        return ((c & 0x1F) << 6) | (c1 & 0x3F);
    } else if ((c & 0xF0) == 0xE0) {
        if (*p == '\0') return c;
        uint8_t c1 = (uint8_t)*p++;
        if (*p == '\0') return c;
        uint8_t c2 = (uint8_t)*p++;
        return ((c & 0x0F) << 12) | ((c1 & 0x3F) << 6) | (c2 & 0x3F);
    } else if ((c & 0xF8) == 0xF0) {
        if (*p == '\0') return c;
        uint8_t c1 = (uint8_t)*p++;
        if (*p == '\0') return c;
        uint8_t c2 = (uint8_t)*p++;
        if (*p == '\0') return c;
        uint8_t c3 = (uint8_t)*p++;
        return ((c & 0x07) << 18) | ((c1 & 0x3F) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F);
    }
    return c;
}

// 查找字模; 缺字时退回 '?' (空格除外)。返回 nullptr 时 fallback_advance 为应前进的宽度
static const GlyphInfo* resolveGlyph(uint32_t unicode, const FontDef& font, int& fallback_advance) {
    const GlyphInfo* g = font_find_glyph(font, unicode);
    if (g) return g;
    if (unicode == ' ') {
        fallback_advance = font.height / 3;
        return nullptr;
    }
    g = font_find_glyph(font, '?');
    fallback_advance = font.height / 2;
    return g;
}

int Canvas480x800::glyphAdvance(uint32_t unicode, const FontDef& font) {
    int fallback = 0;
    const GlyphInfo* g = resolveGlyph(unicode, font, fallback);
    return g ? g->advance : fallback;
}

int Canvas480x800::drawChar(int x, int y, uint32_t unicode, const FontDef& font, bool black) {
    int fallback = 0;
    const GlyphInfo* g = resolveGlyph(unicode, font, fallback);
    if (!g) return fallback;

    int row_bytes = (g->width + 7) / 8;
    for (int gy = 0; gy < g->height; gy++) {
        for (int gx = 0; gx < g->width; gx++) {
            int byte_idx = gy * row_bytes + (gx / 8);
            int bit_idx = 7 - (gx % 8);
            if (g->bitmap[byte_idx] & (1 << bit_idx)) {
                drawPixel(x + gx, y + gy, black);
            }
        }
    }
    return g->advance;
}

int Canvas480x800::measureStringWidth(const char* utf8_str, const FontDef& font) {
    if (!utf8_str) return 0;
    const char* p = utf8_str;
    int total_width = 0;
    while (*p) {
        uint32_t cp = nextUtf8Char(p);
        if (cp == '\n' || cp == '\r') continue;
        total_width += glyphAdvance(cp, font);
    }
    return total_width;
}

int Canvas480x800::drawString(int x, int y, const char* utf8_str, const FontDef& font, bool black) {
    if (!utf8_str) return 0;
    const char* p = utf8_str;
    int cur_x = x;
    while (*p) {
        uint32_t cp = nextUtf8Char(p);
        if (cp == '\n' || cp == '\r') continue;
        cur_x += drawChar(cur_x, y, cp, font, black);
    }
    return cur_x - x;
}

int Canvas480x800::drawStringCentered(int y, const char* utf8_str, const FontDef& font, bool black) {
    int str_w = measureStringWidth(utf8_str, font);
    int x = (UI_WIDTH - str_w) / 2;
    if (x < 0) x = 0;
    return drawString(x, y, utf8_str, font, black);
}

int Canvas480x800::drawStringWrapped(int x, int y, int max_width, int line_spacing,
                                     const char* utf8_str, const FontDef& font, bool black,
                                     int max_y) {
    if (!utf8_str) return y;
    if (y + font.height > max_y) return y;
    const char* p = utf8_str;
    int cur_x = x;
    int cur_y = y;

    // 换到下一行; 若下一行放不下则在本行末尾画 "…" 并返回 false
    auto newLine = [&]() -> bool {
        if (cur_y + 2 * font.height + line_spacing > max_y) {
            const char* ellipsis = "\xE2\x80\xA6";  // U+2026
            int ell_w = measureStringWidth(ellipsis, font);
            int ell_x = cur_x;
            if (ell_x + ell_w > x + max_width) {
                ell_x = x + max_width - ell_w;
                fillRect(ell_x, cur_y, x + max_width - ell_x, font.height, !black);
            }
            drawString(ell_x, cur_y, ellipsis, font, black);
            return false;
        }
        cur_x = x;
        cur_y += font.height + line_spacing;
        return true;
    };

    while (*p) {
        uint32_t cp = nextUtf8Char(p);

        if (cp == '\n') {
            if (!newLine()) break;
            continue;
        }

        // 单词前瞻折行 (针对英文单词): 若遇到空格后紧接一整词，检测该词能否放下
        if (cp == ' ') {
            const char* lookahead = p;
            int word_w = 0;
            while (*lookahead && *lookahead != ' ' && *lookahead != '\n') {
                word_w += glyphAdvance(nextUtf8Char(lookahead), font);
            }
            if ((cur_x + glyphAdvance(' ', font) + word_w) > (x + max_width)) {
                if (!newLine()) break;
                continue; // 跳过当前空格直接换行
            }
        }

        int adv = glyphAdvance(cp, font);

        // 如果超出最大宽度则换行
        if (cur_x + adv > x + max_width) {
            if (!newLine()) break;
        }

        cur_x += drawChar(cur_x, cur_y, cp, font, black);
    }

    return cur_y + font.height;
}
