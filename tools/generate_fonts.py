#!/usr/bin/env python3
"""
tools/generate_fonts.py
Generates C++ bitmap font data (Font16, Font20, Font24, Font32, Font48, Font64) for the E-Paper display.
Supports ASCII, IPA phonetic symbols, and Chinese characters.
"""

import os
import sys
import json
import math
from PIL import Image, ImageFont, ImageDraw

PROJECT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HEADER_PATH = os.path.join(PROJECT_DIR, "include", "font_data.h")
CPP_PATH = os.path.join(PROJECT_DIR, "src", "ui", "font_data.cpp")

# 开源字体, 不随仓库提交, 下载地址与许可见 tools/fonts/README.md
FONT_DIR = os.path.join(PROJECT_DIR, "tools", "fonts")
ZH_FONT_PATH = os.path.join(FONT_DIR, "NotoSansCJKsc-Medium.otf")
EN_FONT_PATH = os.path.join(FONT_DIR, "DejaVuSansCondensed.ttf")

def deck_paths():
    # 自己的词库 (sdcard_dist, 不提交) + 仓库自带的示例词库 (sample_sdcard)
    import glob
    paths = []
    for root in ("sdcard_dist", "sample_sdcard"):
        decks_dir = os.path.join(PROJECT_DIR, root, "spell", "decks")
        paths += [p for p in sorted(glob.glob(os.path.join(decks_dir, "*.json")))
                  if os.path.basename(p) != "manifest.json"]
    if not paths:
        sys.exit("No decks found in sdcard_dist/ or sample_sdcard/")
    return paths


def collect_deck_field(field):
    chars = set()
    for deck_path in deck_paths():
        with open(deck_path, "r", encoding="utf-8") as f:
            for card in json.load(f).get("cards", []):
                chars.update(card.get(field, ""))
    return chars


def collect_characters():
    chars = set(collect_ui_characters())

    # Collect all text from every deck (spbcn_r1/r2/r3, sample, ...)
    for k in ["en", "ipa", "zh", "ex", "cat", "def"]:
        chars.update(collect_deck_field(k))
    return sorted(chars)

def collect_ui_characters():
    """界面文案用到的字: 源码与头文件里的全部字符 + 下面列出的 UI 字符串"""
    chars = set()

    import glob

    # 1. Collect all text from source code & headers
    for path in glob.glob(os.path.join(PROJECT_DIR, "src", "**", "*.cpp"), recursive=True) + \
                glob.glob(os.path.join(PROJECT_DIR, "include", "**", "*.h"), recursive=True):
        if "font_data" in path:
            continue
        with open(path, "r", encoding="utf-8") as f:
            chars.update(f.read())

    # 2. Explicit UI strings & special symbols
    ui_text = (
        "SPBCN R1 单词速记 盒 Box 12345 卡片 第 张 "
        "按压中键 翻面 上拨: 记住了 (+1盒) 下拨: 没记住 (回盒1) "
        "BOOT键: 发音 播放发音 已揭晓答案 今日完成 易错点 分类 "
        "释义 英文释义 例句 长按BOOT键 全刷清残影 恭喜！所有卡片已掌握！ "
        "当前进度 词库 双击菜单 ▲▼●■□ / "
        "【中文释义】【英文定义】【例句】发音 上一张 下一张 长按 BOOT 键可消除墨水屏残影 "
        "【 Leitner 5 盒分布统计 】 重新开始复习 (已完全掌握 ★) 恭喜！ 今日卡片复习完成 "
        "◆ 中文释义 ◆ 英文释义 ◆ 典型例句 "
        "▲ 上拨 · 没记住  (重置盒1) "
        "▼ 下拨 · 记住了  (+1盒晋级) "
        "● 按中键 · 看释义 ● 按中键 · 开始新一轮复习 "
        "BOOT 发音 ★ Leitner 盒 "
        "◆ Leitner 5 盒进阶分布 ◆ ★ 掌握 "
        "Leitner 间隔复习 · 明日继续保持 "
        "共 10 张卡片已完成 Leitner 评定 --% "
        "\u2026 "  # 折行截断省略号 (源码中以 UTF-8 转义写出, 扫描不到)
    )
    chars.update(ui_text)

    # Extract unique characters (code >= 32)
    valid_chars = sorted(list(set([c for c in chars if ord(c) >= 32])))
    return valid_chars

SUPERSAMPLE = 4

def render_supersampled(ch, font, width, height):
    """英文按 4 倍尺寸灰度渲染再缩小、取 50% 阈值。单色渲染会逐字微调, n/x 比 h/l 低 1px,
    一行字高低起伏; 这里把基线固定在整像素 (ascent) 上, 所有字母同一基线。"""
    k = SUPERSAMPLE
    big = ImageFont.truetype(font.path, font.size * k)
    ascent = font.getmetrics()[0]
    img = Image.new("L", (width * k, height * k), 0)
    ImageDraw.Draw(img).text((0, ascent * k), ch, font=big, fill=255, anchor="ls")
    img = img.resize((width, height), Image.BOX)
    return img.point(lambda v: 1 if v >= 128 else 0, mode="1")

def widen_thin_stems(img):
    """16/20px 英文渲染后竖笔有 1px 也有 2px, 墨水屏上看着粗细不一。
    把每行里 1px 宽的笔画向右补 1px (右侧留得出 1px 空隙时才补, 免得糊住字怀), 统一成 2px。"""
    w, h = img.size
    src = img.load()
    out = img.copy()
    dst = out.load()
    for y in range(h):
        for x in range(w):
            if (src[x, y] and (x == 0 or not src[x - 1, y]) and x + 1 < w and not src[x + 1, y]
                    and (x + 2 >= w or not src[x + 2, y])):
                dst[x + 1, y] = 1
    return out

def render_glyph(ch, font_size, zh_font, en_font):
    code = ord(ch)
    # Choose font: use Chinese font for CJK, full-width punctuation, and special symbols
    # 间隔号 · (U+00B7) 走英文字体, 保持窄字符; emoji 两套字体都没有, 不要放进 UI 文案
    font = zh_font if (code >= 0x2000 or ch in "▲▼●■□◆★☆") else en_font

    # Calculate dimensions
    # For space
    if ch == ' ':
        width = font_size // 3
        height = font_size
        return {
            "unicode": code,
            "width": width,
            "height": height,
            "advance": width,
            "bitmap": [0] * (((width + 7) // 8) * height)
        }

    bbox = font.getbbox(ch)
    if not bbox:
        return None

    # 中文字体的 ascent 比英文字体大, 原样画会整体偏下; 上移到与英文同一基线 (但不裁掉字顶)
    dy = 0
    if font is zh_font:
        dy = max(en_font.getmetrics()[0] - zh_font.getmetrics()[0], -bbox[1])
        bbox = (bbox[0], bbox[1] + dy, bbox[2], bbox[3] + dy)

    # bbox: (x0, y0, x1, y1)
    bw = bbox[2] - bbox[0]
    bh = bbox[3] - bbox[1]
    adv = int(round(font.getlength(ch)))

    # Height is at least font_size; extend it to fit descenders (g/j/p/q/y reach
    # below font_size). The top stays at y=0, so layout coordinates are unaffected.
    width = max(adv, bw + max(0, bbox[0]))
    if width <= 0:
        width = font_size // 2
    height = max(font_size, bbox[3])

    # Create image and draw
    if font is en_font:
        img = render_supersampled(ch, font, width, height)
    else:
        img = Image.new("1", (width, height), 0)
        draw = ImageDraw.Draw(img)
        draw.text((0, dy), ch, font=font, fill=1)

    if font is en_font and font_size in (16, 20):
        img = widen_thin_stems(img)

    # Convert to 1-bit packed bytes (row-major, MSB first)
    row_bytes = (width + 7) // 8
    bitmap = []
    for y in range(height):
        for b in range(row_bytes):
            byte_val = 0
            for bit in range(8):
                px = b * 8 + bit
                if px < width:
                    if img.getpixel((px, y)):
                        byte_val |= (1 << (7 - bit))
            bitmap.append(byte_val)

    return {
        "unicode": code,
        "width": width,
        "height": height,
        "advance": adv if adv > 0 else width,
        "bitmap": bitmap
    }

def generate_font(font_name, font_size, char_list):
    print(f"Generating {font_name} (size {font_size}px)...")
    for path in (ZH_FONT_PATH, EN_FONT_PATH):
        if not os.path.exists(path):
            sys.exit(f"Missing font {path}; see tools/fonts/README.md")
    zh_font = ImageFont.truetype(ZH_FONT_PATH, font_size)
    en_font = ImageFont.truetype(EN_FONT_PATH, font_size)

    glyphs = []
    for ch in char_list:
        g = render_glyph(ch, font_size, zh_font, en_font)
        if g:
            glyphs.append(g)

    # Sort glyphs by unicode
    glyphs.sort(key=lambda x: x["unicode"])

    # A font without the character renders the same .notdef box for all of them;
    # identical non-blank, non-ASCII bitmaps therefore mean missing glyphs.
    seen = {}
    for g in glyphs:
        if g["unicode"] < 0x80 or not any(g["bitmap"]):
            continue
        key = (g["width"], g["height"], tuple(g["bitmap"]))
        seen.setdefault(key, []).append(chr(g["unicode"]))
    tofu = [chars for chars in seen.values() if len(chars) > 1]
    if tofu:
        sys.exit(f"{font_name}: characters missing from the fonts (rendered as tofu): {tofu}")
    print(f"  {font_name}: {len(glyphs)} glyphs generated.")
    return glyphs

def main():
    print("Collecting characters from datasets and UI...")
    all_chars = collect_characters()
    print(f"Total unique characters to support: {len(all_chars)}")

    # ASCII subset (32..126)
    ascii_chars = [chr(c) for c in range(32, 127)]

    # Font 16: ALL characters (all decks + UI + all code strings)
    chars_16 = all_chars

    # Font 24: ALL characters (all decks + UI + all code strings)
    chars_24 = all_chars

    # Font 48: ASCII + titles + every deck's Chinese meanings (zh->en card front)
    chars_48 = sorted(set(ascii_chars + list("恭喜！今日复习完成设置已关机未检测到卡词库读取失败") +
                          [c for c in collect_deck_field("zh") if ord(c) >= 32]))

    f16_glyphs = generate_font("Font16", 16, chars_16)
    f24_glyphs = generate_font("Font24", 24, chars_24)
    f48_glyphs = generate_font("Font48", 48, chars_48)
    # Font 64: ASCII only (en->zh card front headword)
    f64_glyphs = generate_font("Font64", 64, ascii_chars)
    # Font 32: ASCII + every deck's IPA symbols (en->zh card front phonetic)
    chars_32 = sorted(set(ascii_chars + [c for c in collect_deck_field("ipa") if ord(c) >= 32]))
    f32_glyphs = generate_font("Font32", 32, chars_32)
    # Font 20: ASCII + UI 文案 (中文小字从 16px 放大到 20px, 笔画更均匀)
    chars_20 = sorted(set(ascii_chars + collect_ui_characters()))
    f20_glyphs = generate_font("Font20", 20, chars_20)

    # Write Header
    print(f"Writing {HEADER_PATH}...")
    with open(HEADER_PATH, "w", encoding="utf-8") as f:
        f.write("""#ifndef FONT_DATA_H
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
""")

    # Write CPP
    print(f"Writing {CPP_PATH}...")
    with open(CPP_PATH, "w", encoding="utf-8") as f:
        f.write("""#include "font_data.h"
#include <stddef.h>

// ============================================================================
// 点阵数据数组
// ============================================================================
""")

        # Helper to dump bitmaps
        def dump_bitmaps(font_tag, glyphs):
            for i, g in enumerate(glyphs):
                hex_bytes = ", ".join([f"0x{b:02X}" for b in g["bitmap"]])
                f.write(f"static const uint8_t {font_tag}_bm_{i}[] = {{{hex_bytes}}};\n")

            f.write(f"\nstatic const GlyphInfo {font_tag}_glyphs[{len(glyphs)}] = {{\n")
            for i, g in enumerate(glyphs):
                f.write(f"    {{0x{g['unicode']:X}, {g['width']}, {g['height']}, {g['advance']}, {font_tag}_bm_{i}}},\n")
            f.write("};\n\n")

        dump_bitmaps("f16", f16_glyphs)
        dump_bitmaps("f24", f24_glyphs)
        dump_bitmaps("f48", f48_glyphs)
        dump_bitmaps("f64", f64_glyphs)
        dump_bitmaps("f32", f32_glyphs)
        dump_bitmaps("f20", f20_glyphs)

        f.write(f"const FontDef Font16 = {{ 16, {len(f16_glyphs)}, f16_glyphs }};\n")
        f.write(f"const FontDef Font24 = {{ 24, {len(f24_glyphs)}, f24_glyphs }};\n")
        f.write(f"const FontDef Font48 = {{ 48, {len(f48_glyphs)}, f48_glyphs }};\n")
        f.write(f"const FontDef Font32 = {{ 32, {len(f32_glyphs)}, f32_glyphs }};\n")
        f.write(f"const FontDef Font64 = {{ 64, {len(f64_glyphs)}, f64_glyphs }};\n")
        f.write(f"const FontDef Font20 = {{ 20, {len(f20_glyphs)}, f20_glyphs }};\n\n")

        f.write("""const GlyphInfo* font_find_glyph(const FontDef& font, uint32_t unicode) {
    if (font.glyphs == nullptr || font.glyph_count == 0) return nullptr;

    int low = 0;
    int high = font.glyph_count - 1;
    while (low <= high) {
        int mid = low + (high - low) / 2;
        if (font.glyphs[mid].unicode == unicode) {
            return &font.glyphs[mid];
        } else if (font.glyphs[mid].unicode < unicode) {
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }
    return nullptr;
}
""")

    print("Font generation completed successfully!")

if __name__ == "__main__":
    main()
