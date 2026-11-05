#include <unity.h>
#include <stdio.h>
#include <string.h>
#include "font_data.h"

void setUp() {}
void tearDown() {}

static const FontDef* const FONTS[] = {&Font16, &Font24, &Font32, &Font48, &Font64};

static bool rowHasInk(const GlyphInfo* g, int row) {
    int rb = (g->width + 7) / 8;
    for (int i = 0; i < rb; i++)
        if (g->bitmap[row * rb + i]) return true;
    return false;
}

static int bitmapBytes(const GlyphInfo* g) { return (g->width + 7) / 8 * g->height; }

static bool isBlank(const GlyphInfo* g) {
    for (int i = 0; i < bitmapBytes(g); i++)
        if (g->bitmap[i]) return false;
    return true;
}

// 下伸字母 (g j p q y) 的点阵必须延伸到字号以下并画出下伸部分; 被截断时高度恰为字号
void test_descenders_not_clipped() {
    const char* desc = "gjpqy";
    for (const FontDef* f : FONTS) {
        for (const char* c = desc; *c; c++) {
            const GlyphInfo* g = font_find_glyph(*f, (uint32_t)*c);
            TEST_ASSERT_NOT_NULL(g);
            char msg[48];
            snprintf(msg, sizeof(msg), "'%c' clipped in Font%d", *c, f->height);
            TEST_ASSERT_TRUE_MESSAGE(g->height > f->height, msg);
            bool ink_below = false;
            for (int row = f->height; row < g->height; row++) ink_below |= rowHasInk(g, row);
            TEST_ASSERT_TRUE_MESSAGE(ink_below, msg);
        }
    }
}

static bool isHanzi(uint32_t u) { return u >= 0x4E00 && u <= 0x9FFF; }

// 汉字不得出现完全相同的点阵 (相同意味着字体缺字, 渲染成了同一个方框)
// 符号里有本来就同形的 (如 ¯ 与 ˉ), 不在检查范围
void test_no_duplicate_tofu_glyphs() {
    for (const FontDef* f : FONTS) {
        for (int i = 0; i < f->glyph_count; i++) {
            const GlyphInfo* a = &f->glyphs[i];
            if (!isHanzi(a->unicode) || isBlank(a)) continue;
            for (int j = i + 1; j < f->glyph_count; j++) {
                const GlyphInfo* b = &f->glyphs[j];
                if (!isHanzi(b->unicode) || a->width != b->width || a->height != b->height) continue;
                if (memcmp(a->bitmap, b->bitmap, bitmapBytes(a)) == 0) {
                    char msg[64];
                    snprintf(msg, sizeof(msg), "Font%d: U+%04lX == U+%04lX", f->height,
                             (unsigned long)a->unicode, (unsigned long)b->unicode);
                    TEST_FAIL_MESSAGE(msg);
                }
            }
        }
    }
}

// 间隔号 · 应是窄字符, 而不是全角宽度
void test_middle_dot_is_narrow() {
    const GlyphInfo* g = font_find_glyph(Font24, 0xB7);
    TEST_ASSERT_NOT_NULL(g);
    TEST_ASSERT_TRUE(g->advance <= Font24.height / 2);
}

// Font48 收录词库中文释义用字 (中→英正面大字显示)
void test_font48_has_chinese_meaning_glyphs() {
    const uint32_t chars[] = {0x5E74 /*年*/, 0x5DE5 /*工*/, 0x4F5C /*作*/, 0x6F14 /*演*/, 0x8BB2 /*讲*/};
    for (uint32_t c : chars) TEST_ASSERT_NOT_NULL(font_find_glyph(Font48, c));
}

// 错误页标题用 Font48 显示, 缺字会画成 '?'
void test_font48_has_error_titles() {
    const char* titles[] = {"未检测到 TF 卡", "词库读取失败"};
    for (const char* t : titles) {
        const unsigned char* p = (const unsigned char*)t;
        while (*p) {
            uint32_t cp;
            if (*p < 0x80) cp = *p++;
            else { cp = ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F); p += 3; }
            if (cp == ' ') continue;
            char msg[48];
            snprintf(msg, sizeof(msg), "Font48 缺 U+%04X (%s)", (unsigned)cp, t);
            TEST_ASSERT_NOT_NULL_MESSAGE(font_find_glyph(Font48, cp), msg);
        }
    }
}

static int hanziCount(const FontDef& f) {
    int n = 0;
    for (int i = 0; i < f.glyph_count; i++) n += isHanzi(f.glyphs[i].unicode);
    return n;
}

// 内置 GB2312 全部 6763 个汉字 (首字「啊」末字「齄」): 换任意中文释义词库都不缺字
void test_builtin_gb2312_hanzi() {
    const FontDef* fonts[] = {&Font16, &Font24, &Font48};
    for (const FontDef* f : fonts) {
        TEST_ASSERT_TRUE(hanziCount(*f) >= 6763);
        TEST_ASSERT_NOT_NULL(font_find_glyph(*f, 0x554A));  // 啊
        TEST_ASSERT_NOT_NULL(font_find_glyph(*f, 0x9F44));  // 齄
        TEST_ASSERT_NOT_NULL(font_find_glyph(*f, 0x3002));  // 。
    }
}

// 内置带重音拉丁字母与音标
void test_builtin_latin_and_ipa() {
    const uint32_t latin[] = {0xE9, 0xEF, 0xE7, 0xC6, 0x153};  // é ï ç Æ œ
    for (uint32_t c : latin) {
        TEST_ASSERT_NOT_NULL(font_find_glyph(Font24, c));
        TEST_ASSERT_NOT_NULL(font_find_glyph(Font64, c));
    }
    const uint32_t ipa[] = {0x259, 0x283, 0x28A, 0x2C8, 0x2D0};  // ə ʃ ʊ ˈ ː
    for (uint32_t c : ipa) {
        TEST_ASSERT_NOT_NULL(font_find_glyph(Font24, c));
        TEST_ASSERT_NOT_NULL(font_find_glyph(Font32, c));
    }
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_descenders_not_clipped);
    RUN_TEST(test_no_duplicate_tofu_glyphs);
    RUN_TEST(test_middle_dot_is_narrow);
    RUN_TEST(test_font48_has_chinese_meaning_glyphs);
    RUN_TEST(test_font48_has_error_titles);
    RUN_TEST(test_builtin_gb2312_hanzi);
    RUN_TEST(test_builtin_latin_and_ipa);
    return UNITY_END();
}
