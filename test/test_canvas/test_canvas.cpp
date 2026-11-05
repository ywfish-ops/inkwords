#include <unity.h>
#include <string.h>
#include "canvas_480x800.h"

static uint8_t s_buf[EPD_BUFFER_SIZE];
static Canvas480x800 s_canvas(s_buf, 3);

void setUp() { s_canvas.clear(true); }
void tearDown() {}

// 缺字时: 测量宽度必须与实际绘制步进一致, 否则居中会偏
void test_missing_glyph_measure_matches_draw() {
    const char* s = "\xF4\x8F\xBF\xBF";  // U+10FFFF, 任何字库都不包含
    int measured = s_canvas.measureStringWidth(s, Font24);
    int drawn = s_canvas.drawString(0, 0, s, Font24, true);
    TEST_ASSERT_EQUAL(drawn, measured);
}

void test_missing_glyph_mixed_text_measure_matches_draw() {
    const char* s = "ab\xF4\x8F\xBF\xBF c";
    TEST_ASSERT_EQUAL(s_canvas.drawString(0, 0, s, Font16, true),
                      s_canvas.measureStringWidth(s, Font16));
}

// 实心圆角矩形每一行左右必须对称, 且右边缘落在 x + w - 1
void test_fill_round_rect_is_symmetric() {
    const int x = 20, y = 30, w = 41, h = 25, r = 8;
    s_canvas.fillRoundRect(x, y, w, h, r, true);
    for (int row = y; row < y + h; row++) {
        int left = -1, right = -1;
        for (int col = 0; col < UI_WIDTH; col++) {
            if (s_canvas.getPixel(col, row)) {
                if (left < 0) left = col;
                right = col;
            }
        }
        TEST_ASSERT_TRUE_MESSAGE(left >= 0, "row empty");
        TEST_ASSERT_EQUAL_MESSAGE(left - x, (x + w - 1) - right, "row not symmetric");
    }
    // 中间行应铺满整宽
    TEST_ASSERT_TRUE(s_canvas.getPixel(x, y + h / 2));
    TEST_ASSERT_TRUE(s_canvas.getPixel(x + w - 1, y + h / 2));
    TEST_ASSERT_FALSE(s_canvas.getPixel(x + w, y + h / 2));
}

// 换行文本: 给定 max_y 时不得画到 max_y 及以下, 并返回不超过 max_y 的底边
void test_wrapped_respects_max_y() {
    const char* s = "one two three four five six seven eight nine ten eleven twelve thirteen";
    int bottom = s_canvas.drawStringWrapped(10, 100, 120, 4, s, Font24, true, 160);
    TEST_ASSERT_TRUE(bottom <= 160);
    for (int y = 160; y < UI_HEIGHT; y++)
        for (int x = 0; x < UI_WIDTH; x++)
            TEST_ASSERT_FALSE(s_canvas.getPixel(x, y));
}

// 放得下时不截断: 与不限高度的结果一致
void test_wrapped_without_overflow_is_unchanged() {
    const char* s = "one two three";
    int a = s_canvas.drawStringWrapped(10, 100, 400, 4, s, Font24, true);
    s_canvas.clear(true);
    int b = s_canvas.drawStringWrapped(10, 100, 400, 4, s, Font24, true, 200);
    TEST_ASSERT_EQUAL(a, b);
}

// 凸多边形填充: 旋转 45° 的正方形 (菱形) 内部填满, 外部不涂
void test_fill_convex_polygon_diamond() {
    const int xs[4] = {100, 140, 100, 60};
    const int ys[4] = {100, 140, 180, 140};
    s_canvas.fillPolygon(xs, ys, 4, true);
    TEST_ASSERT_TRUE(s_canvas.getPixel(100, 140));   // 中心
    TEST_ASSERT_TRUE(s_canvas.getPixel(100, 105));   // 靠近上顶点
    TEST_ASSERT_TRUE(s_canvas.getPixel(135, 140));   // 靠近右顶点
    TEST_ASSERT_FALSE(s_canvas.getPixel(65, 105));   // 左上角在菱形外
    TEST_ASSERT_FALSE(s_canvas.getPixel(135, 175));  // 右下角在菱形外
    TEST_ASSERT_FALSE(s_canvas.getPixel(100, 190));  // 下方之外
}

// 多边形描边: 边上有像素, 内部保持空白
void test_draw_polygon_outline() {
    const int xs[4] = {100, 140, 100, 60};
    const int ys[4] = {100, 140, 180, 140};
    s_canvas.drawPolygon(xs, ys, 4, true);
    TEST_ASSERT_TRUE(s_canvas.getPixel(100, 100));
    TEST_ASSERT_TRUE(s_canvas.getPixel(120, 120));
    TEST_ASSERT_FALSE(s_canvas.getPixel(100, 140));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_missing_glyph_measure_matches_draw);
    RUN_TEST(test_missing_glyph_mixed_text_measure_matches_draw);
    RUN_TEST(test_fill_round_rect_is_symmetric);
    RUN_TEST(test_wrapped_respects_max_y);
    RUN_TEST(test_wrapped_without_overflow_is_unchanged);
    RUN_TEST(test_fill_convex_polygon_diamond);
    RUN_TEST(test_draw_polygon_outline);
    return UNITY_END();
}
