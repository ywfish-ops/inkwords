#ifndef CANVAS_480X800_H
#define CANVAS_480X800_H

#include <stdint.h>
#include <stdbool.h>
#include "board_config.h"
#include "font_data.h"

/**
 * @file canvas_480x800.h
 * @brief 墨水屏 480x800 竖屏画布 (Portrait Canvas)
 *        负责物理 800x480 与 逻辑 480x800 坐标变换、2D 几何图形与点阵文字渲染
 */

class Canvas480x800 {
public:
    Canvas480x800(uint8_t* buffer, uint8_t rotation = 1);

    void setBuffer(uint8_t* buffer) { _buffer = buffer; }
    void setRotation(uint8_t rotation) { _rotation = rotation; }
    uint8_t getRotation() const { return _rotation; }

    int getWidth() const { return UI_WIDTH; }    // 480
    int getHeight() const { return UI_HEIGHT; }  // 800

    // 清屏 (white = true 填充全白, false 填充全黑)
    void clear(bool white = true);

    // 基础画点 (逻辑坐标 0 <= x < 480, 0 <= y < 800)
    void drawPixel(int x, int y, bool black = true);

    // 读取逻辑坐标像素 (越界返回 false = 白)
    bool getPixel(int x, int y) const;

    // 几何图形图元
    void drawLine(int x0, int y0, int x1, int y1, bool black = true);
    void drawFastHLine(int x, int y, int w, bool black = true);
    void drawFastVLine(int x, int y, int h, bool black = true);
    void drawRect(int x, int y, int w, int h, bool black = true);
    void fillRect(int x, int y, int w, int h, bool black = true);
    void drawRoundRect(int x, int y, int w, int h, int r, bool black = true);
    void fillRoundRect(int x, int y, int w, int h, int r, bool black = true);

    // 多边形 (顶点数 3 ~ 16): 扫描线填充 (奇偶规则) 与描边
    void fillPolygon(const int* xs, const int* ys, int n, bool black = true);
    void drawPolygon(const int* xs, const int* ys, int n, bool black = true);

    // 虚线与装饰线
    void drawDottedHLine(int x, int y, int w, int dot_spacing = 4, bool black = true);

    // 文字排版与渲染
    // 绘制单个字符 (返回 advance 像素步进)
    int drawChar(int x, int y, uint32_t unicode, const FontDef& font, bool black = true);

    // 绘制 UTF-8 字符串 (返回累计绘制宽度)
    int drawString(int x, int y, const char* utf8_str, const FontDef& font, bool black = true);

    // 水平居中绘制字符串
    int drawStringCentered(int y, const char* utf8_str, const FontDef& font, bool black = true);

    // 测量 UTF-8 字符串像素宽度
    int measureStringWidth(const char* utf8_str, const FontDef& font);

    // 自动换行排版绘制长文本 (如英文完整定义、例句)，返回文本排版占用的最大 Y 坐标
    // max_y: 文本不得越过的底边 (不含)，放不下的部分截断并以 "…" 结尾
    int drawStringWrapped(int x, int y, int max_width, int line_spacing,
                          const char* utf8_str, const FontDef& font, bool black = true,
                          int max_y = UI_HEIGHT);

    // 单个字符的水平步进 (与 drawChar 的缺字退回规则一致)
    static int glyphAdvance(uint32_t unicode, const FontDef& font);

    // UTF-8 解码工具函数
    static uint32_t nextUtf8Char(const char*& p);

private:
    bool mapToPhysical(int x, int y, int& phys_x, int& phys_y) const;

    uint8_t* _buffer = nullptr;
    uint8_t  _rotation = 1; // 1: 90° CW, 3: 270° CW
};

#endif // CANVAS_480X800_H
