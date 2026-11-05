#include "flashcard_ui.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

// 正背面最底部那行提示的位置, 两面一致, 翻面时不跳
static const int FOOTER_Y = 754;

void FlashcardUI::drawBattery(int x, int y, int percent, bool charging) {
    // 电池外壳 22 x 12, 圆角 2
    _canvas.drawRoundRect(x, y, 22, 12, 2, true);
    // 电池正极小凸起 2 x 6
    _canvas.fillRect(x + 22, y + 3, 2, 6, true);
    // 内部电量格 (长 16px, 高 8px)
    int fill_w = (percent * 16) / 100;
    if (fill_w < 2 && percent > 0) fill_w = 2;
    if (fill_w > 16) fill_w = 16;
    if (fill_w > 0) {
        _canvas.fillRect(x + 3, y + 2, fill_w, 8, true);
    }

    // 充电中: 在 16x8 电量格上反色叠加闪电, 电量填充仍可辨认
    if (charging) {
        static const char* const BOLT[8] = {
            ".........##.....",
            ".......###......",
            ".....####.......",
            "...##########...",
            ".......####.....",
            "......###.......",
            ".....##.........",
            "....#...........",
        };
        for (int row = 0; row < 8; row++) {
            for (int col = 0; col < 16; col++) {
                if (BOLT[row][col] != '#') continue;
                int px = x + 3 + col, py = y + 2 + row;
                _canvas.drawPixel(px, py, !_canvas.getPixel(px, py));
            }
        }
    }
}

// 线框圆角按钮 (避免大面积实心黑: 局刷翻转时会留下明显残影)
void FlashcardUI::drawOutlineButton(int x, int y, int w, int h, int r, int thickness) {
    for (int i = 0; i < thickness; i++) {
        _canvas.drawRoundRect(x + i, y + i, w - 2 * i, h - 2 * i, r - i, true);
    }
}

void FlashcardUI::drawSpeakerIcon(int x, int y) {
    // 16x16 线条小喇叭: 机身 + 喇叭口 + 两道声波
    _canvas.fillRect(x + 1, y + 5, 4, 6, true);
    for (int i = 0; i < 5; i++) {
        _canvas.drawFastVLine(x + 5 + i, y + 5 - i, 6 + 2 * i, true);
    }
    _canvas.drawFastVLine(x + 11, y + 6, 4, true);
    _canvas.drawPixel(x + 10, y + 5, true);
    _canvas.drawPixel(x + 10, y + 10, true);
    _canvas.drawFastVLine(x + 14, y + 4, 8, true);
    _canvas.drawPixel(x + 13, y + 3, true);
    _canvas.drawPixel(x + 13, y + 12, true);
}

void FlashcardUI::drawMiniBoxProgress(int x, int y, int active_box) {
    // 精致微缩 5 段进度条 (每个长 12, 高 7, 间距 4, 总宽 76px)
    const int seg_w = 12;
    const int seg_h = 7;
    const int spacing = 4;
    for (int i = 1; i <= 5; i++) {
        int sx = x + (i - 1) * (seg_w + spacing);
        if (i <= active_box) {
            _canvas.fillRoundRect(sx, y, seg_w, seg_h, 2, true); // 实心黑条
        } else {
            _canvas.drawRoundRect(sx, y, seg_w, seg_h, 2, true); // 线框白条
        }
    }
}

void FlashcardUI::setDeckLabel(const char* label) {
    strncpy(_deck_label, label ? label : "", sizeof(_deck_label) - 1);
    _deck_label[sizeof(_deck_label) - 1] = '\0';
}

void FlashcardUI::drawHeader(const char* deck_id, int box, int card_idx, int total_cards) {
    // 1. 词库名称 (左侧圆角胶囊徽章, 没有标签时不画)
    if (deck_id && *deck_id) {
        char deck_buf[32];
        snprintf(deck_buf, sizeof(deck_buf), " %s ", deck_id);
        int deck_w = _canvas.measureStringWidth(deck_buf, Font16);
        _canvas.drawRoundRect(16, 12, deck_w + 10, 22, 11, true);
        _canvas.drawString(21, 15, deck_buf, Font16, true);
    }

    // 2. 居中: 卡片计数与微型 5 盒进阶分段条
    char count_buf[32];
    snprintf(count_buf, sizeof(count_buf), "%d / %d", card_idx + 1, total_cards);
    int count_w = _canvas.measureStringWidth(count_buf, Font16);
    int center_total_w = count_w + 14 + 76; // 计数 + 间距 + 5盒进度条(76px)
    int center_start_x = (UI_WIDTH - center_total_w) / 2;
    _canvas.drawString(center_start_x, 15, count_buf, Font16, true);
    drawMiniBoxProgress(center_start_x + count_w + 14, 19, box);

    // 3. 右侧: 电池电量与电池图标 (电量未知时显示 "--%" 与空电池)
    int bat = _battery_percent > 100 ? 100 : _battery_percent;
    char bat_buf[16];
    if (bat < 0) {
        snprintf(bat_buf, sizeof(bat_buf), "--%%");
        bat = 0;
    } else {
        snprintf(bat_buf, sizeof(bat_buf), "%d%%", bat);
    }
    int bat_w = _canvas.measureStringWidth(bat_buf, Font16);
    int bat_icon_x = UI_WIDTH - 16 - 24;
    int bat_text_x = bat_icon_x - 6 - bat_w;
    _canvas.drawString(bat_text_x, 15, bat_buf, Font16, true);
    drawBattery(bat_icon_x, 17, bat, _charging);

    // 4. 状态栏底部分割线
    _canvas.drawFastHLine(16, 42, UI_WIDTH - 32, true);
}

// 字体是否收录了 text 的每个字 (Font32/Font64 只收部分字符, 缺字会画成 '?')
static bool fontCovers(const char* text, const FontDef& font) {
    const char* p = text;
    while (*p) {
        if (!font_find_glyph(font, Canvas480x800::nextUtf8Char(p))) return false;
    }
    return true;
}

// 正面主文字: 英文单词优先 Font64 (拉丁字母, 无汉字); 否则 Font48 居中, 放不下时降为 Font24, 仍放不下则折行
void FlashcardUI::drawHeadline(const char* text, int y, bool large) {
    const int max_w = UI_WIDTH - 68;
    if (large && fontCovers(text, Font64) && _canvas.measureStringWidth(text, Font64) <= max_w) {
        _canvas.drawStringCentered(y - 8, text, Font64, true);  // 上移 8px, 字面视觉中心与 Font48 对齐
    } else if (_canvas.measureStringWidth(text, Font48) <= max_w) {
        _canvas.drawStringCentered(y, text, Font48, true);
    } else if (_canvas.measureStringWidth(text, Font24) <= max_w) {
        _canvas.drawStringCentered(y + 12, text, Font24, true);
    } else {
        _canvas.drawStringWrapped(34, y, max_w, 4, text, Font24, true, y + 70);
    }
}

void FlashcardUI::renderFront(const WordCard& card, const CardProgress& prog, int card_idx, int total_cards) {
    _canvas.clear(true); // 纯白底色

    // 1. 顶部极简状态栏
    drawHeader(_deck_label, prog.box, card_idx, total_cards);

    // 2. 拟物实体卡片外框 (y = 54 .. 704, 高度 650px)
    _canvas.drawRoundRect(16, 54, UI_WIDTH - 32, 650, 16, true);
    _canvas.drawRoundRect(17, 55, UI_WIDTH - 34, 648, 15, true); // 双线外框增加厚度卡片质感

    // 3. 卡片内信息头部 (y = 70 .. 102)
    // 左侧: 考点/分类标签
    if (card.cat[0] != '\0') {
        char tag_buf[32];
        snprintf(tag_buf, sizeof(tag_buf), " # %s ", card.cat);
        int tag_w = _canvas.measureStringWidth(tag_buf, Font16);
        _canvas.drawRoundRect(34, 70, tag_w + 10, 24, 12, true);
        _canvas.drawString(39, 74, tag_buf, Font16, true);
    }
    // 右侧: 当前盒子等级
    char box_tag[32];
    snprintf(box_tag, sizeof(box_tag), "★ Leitner 盒 %d", prog.box);
    int btag_w = _canvas.measureStringWidth(box_tag, Font16);
    _canvas.drawString(UI_WIDTH - 34 - btag_w, 74, box_tag, Font16, true);

    // 卡片内浅点虚线
    _canvas.drawDottedHLine(34, 106, UI_WIDTH - 68, 5, true);

    // 4~5. 黄金比例居中: 英→中 显示英文单词 + 音标; 中→英 显示中文释义 + 拼写提示
    if (_dir == DIR_ZH_TO_EN) {
        drawHeadline(card.zh, 230);
        _canvas.drawStringCentered(303, "听发音 · 回想英文拼写", Font20, true);
    } else {
        drawHeadline(card.en, 230, true);
        // 音标随单词同比例放大 (24→32); 缺字或放不下时退回 Font24.
        // y=306 紧贴单词成一组, 刚好让开 Font64 的下伸部分
        if (card.ipa[0] != '\0') {
            const FontDef& ipa_font =
                fontCovers(card.ipa, Font32) && _canvas.measureStringWidth(card.ipa, Font32) <= UI_WIDTH - 68
                    ? Font32 : Font24;
            _canvas.drawStringCentered(306, card.ipa, ipa_font, true);
        }
    }

    // 6. 音频发音提示微胶囊 (y = 384, 与单词+音标组拉开 40px 以上)
    _canvas.drawRoundRect((UI_WIDTH - 172) / 2, 384, 172, 34, 17, true);
    const char* speak_label = "BOOT 发音";
    int speak_w = 16 + 6 + _canvas.measureStringWidth(speak_label, Font20);
    int speak_x = (UI_WIDTH - speak_w) / 2;
    drawSpeakerIcon(speak_x, 393);
    _canvas.drawString(speak_x + 22, 391, speak_label, Font20, true);

    // 7. 卡片下部装饰分割线 (y = 480)
    _canvas.drawDottedHLine(60, 480, UI_WIDTH - 120, 6, true);

    // 8. 翻面交互引导 (2px 线框胶囊按键, y = 550)
    drawOutlineButton(80, 550, UI_WIDTH - 160, 56, 28, 2);
    int prompt_w = _canvas.measureStringWidth("● 按中键 · 看释义", Font24);
    _canvas.drawString((UI_WIDTH - prompt_w) / 2, 566, "● 按中键 · 看释义", Font24, true);

    // 9. 底部全局操作导引 (卡片外部, y = 724 .. 776)
    _canvas.drawStringCentered(726, "▲ 看答案        ▼ 记住了", Font20, true);
    drawFooter(FOOTER_Y, "长按 BOOT 键打开设置");
}

void FlashcardUI::renderBack(const WordCard& card, const CardProgress& prog, int card_idx, int total_cards) {
    _canvas.clear(true); // 纯白底色

    // 1. 顶部极简状态栏
    drawHeader(_deck_label, prog.box, card_idx, total_cards);

    // 2. 拟物实体卡片外框 (y = 54 .. 610, 高度 556px)
    _canvas.drawRoundRect(16, 54, UI_WIDTH - 32, 556, 16, true);
    _canvas.drawRoundRect(17, 55, UI_WIDTH - 34, 554, 15, true);

    // 3. 卡片内词汇头部 (英文 + 音标 + 分类胶囊)
    int en_right = UI_WIDTH - 34;  // 英文可用的右边界 (避让分类胶囊)
    if (card.cat[0] != '\0') {
        char tag_buf[32];
        snprintf(tag_buf, sizeof(tag_buf), " # %s ", card.cat);
        int tag_w = _canvas.measureStringWidth(tag_buf, Font16);
        int tag_x = UI_WIDTH - 34 - tag_w - 10;
        _canvas.drawRoundRect(tag_x, 78, tag_w + 10, 24, 12, true);
        _canvas.drawString(tag_x + 5, 82, tag_buf, Font16, true);
        en_right = tag_x - 8;
    }
    if (_canvas.measureStringWidth(card.en, Font48) <= en_right - 34) {
        _canvas.drawString(34, 68, card.en, Font48, true);
    } else {
        _canvas.drawStringWrapped(34, 80, en_right - 34, 2, card.en, Font24, true, 120);
    }
    _canvas.drawString(34, 122, card.ipa, Font24, true);

    // 分割线
    _canvas.drawFastHLine(34, 154, UI_WIDTH - 68, true);

    // 4~6 区块文字的底线: 卡片框 (y=54+556) 内侧留白
    const int content_bottom = 600;
    // 区块标题 (Font20 + 4px) + 至少一行正文才绘制
    const int title_h = 24;
    auto sectionFits = [&](int y) { return y + title_h + Font24.height <= content_bottom; };

    // 4. 中文释义区块 (自适应高度)
    _canvas.drawString(34, 166, "◆ 中文释义", Font20, true);
    int y_after_zh = _canvas.drawStringWrapped(38, 166 + title_h, UI_WIDTH - 76, 6, card.zh, Font24, true,
                                               content_bottom);

    // 5. 英文原版释义区块 (自适应动态避让)
    int y_def = (y_after_zh > 224) ? (y_after_zh + 14) : 234;
    int y_after_def = y_def;
    if (sectionFits(y_def)) {
        _canvas.drawString(34, y_def, "◆ 英文释义", Font20, true);
        y_after_def = _canvas.drawStringWrapped(38, y_def + title_h, UI_WIDTH - 76, 4, card.def_en, Font24,
                                                true, content_bottom);
    }

    // 6. 典型例句区块 (精致引用条 | )
    int y_ex = (y_after_def > 340) ? (y_after_def + 14) : 350;
    if (sectionFits(y_ex)) {
        _canvas.drawString(34, y_ex, "◆ 典型例句", Font20, true);
        int ex_start_y = y_ex + title_h;
        int ex_end_y = _canvas.drawStringWrapped(48, ex_start_y, UI_WIDTH - 86, 4, card.ex, Font24, true,
                                                 content_bottom);
        // 左侧加粗引用竖条 (3px)
        if (ex_end_y > ex_start_y) {
            _canvas.fillRect(36, ex_start_y + 2, 3, ex_end_y - ex_start_y - 2, true);
        }
    }

    // 7. 底部 Leitner 评定操作条 (两个醒目大按键, y = 626 .. 744)
    // 按钮 1: 上拨 没记住 (双线圆角卡片)
    _canvas.drawRoundRect(20, 626, UI_WIDTH - 40, 52, 14, true);
    _canvas.drawRoundRect(21, 627, UI_WIDTH - 42, 50, 13, true);
    int t1_w = _canvas.measureStringWidth("▲ 上拨 · 没记住  (重置盒1)", Font24);
    _canvas.drawString((UI_WIDTH - t1_w) / 2, 640, "▲ 上拨 · 没记住  (重置盒1)", Font24, true);

    // 按钮 2: 下拨 记住了 (3px 粗线框, 比 "没记住" 更醒目)
    drawOutlineButton(20, 690, UI_WIDTH - 40, 54, 14, 3);
    int t2_w = _canvas.measureStringWidth("▼ 下拨 · 记住了  (+1盒晋级)", Font24);
    _canvas.drawString((UI_WIDTH - t2_w) / 2, 705, "▼ 下拨 · 记住了  (+1盒晋级)", Font24, true);

    // 底部提示: 与正面同一位置, 翻面时不跳
    drawFooter(FOOTER_Y, "长按 BOOT 键打开设置");
}

void FlashcardUI::drawFooter(int y, const char* normal) {
    _canvas.drawStringCentered(y, _clock_warning ? "时钟未校准 · 请连电脑对时" : normal, Font20, true);
}

void FlashcardUI::renderDone(int total_cards, const int box_counts[6], int tomorrow_due, int tomorrow_new) {
    _canvas.clear(true);

    // 拟物卡片外框
    _canvas.drawRoundRect(16, 30, UI_WIDTH - 32, UI_HEIGHT - 60, 18, true);
    _canvas.drawRoundRect(17, 31, UI_WIDTH - 34, UI_HEIGHT - 62, 17, true);

    _canvas.drawStringCentered(75, "今日复习完成", Font48, true);
    
    char sub_buf[64];
    snprintf(sub_buf, sizeof(sub_buf), "明天到期 %d 张 · 新词 %d 个", tomorrow_due, tomorrow_new);
    _canvas.drawStringCentered(145, sub_buf, Font24, true);

    _canvas.drawFastHLine(50, 190, UI_WIDTH - 100, true);

    _canvas.drawStringCentered(213, "◆ Leitner 5 盒进阶分布 ◆", Font20, true);

    // 绘制 5 盒统计柱状条
    int cur_y = 265;
    for (int i = 1; i <= 5; i++) {
        char label[16];
        snprintf(label, sizeof(label), "盒 %d", i);
        _canvas.drawString(50, cur_y, label, Font24, true);

        // 统计条背景框 (长 200, 高 18)
        int bar_x = 110;
        int bar_y = cur_y + 4;
        int bar_w = 200;
        int bar_h = 16;
        _canvas.drawRoundRect(bar_x, bar_y, bar_w, bar_h, 4, true);

        int count = box_counts[i];
        if (count > 0 && total_cards > 0) {
            int fill_w = (count * bar_w) / total_cards;
            if (fill_w < 6) fill_w = 6;
            if (fill_w > bar_w) fill_w = bar_w;
            _canvas.fillRoundRect(bar_x, bar_y, fill_w, bar_h, 4, true);
        }

        // 数量与掌握标记
        char count_str[32];
        if (i == 5 && count > 0) {
            snprintf(count_str, sizeof(count_str), "%d 张 ★ 掌握", count);
        } else {
            snprintf(count_str, sizeof(count_str), "%d 张", count);
        }
        _canvas.drawString(bar_x + bar_w + 12, cur_y, count_str, Font24, true);

        cur_y += 54;
    }

    _canvas.drawDottedHLine(50, 560, UI_WIDTH - 100, 6, true);

    // 重新复习按键
    drawOutlineButton(60, 600, UI_WIDTH - 120, 58, 29, 2);
    int rst_w = _canvas.measureStringWidth("● 按中键 · 再过一遍全部词", Font24);
    _canvas.drawString((UI_WIDTH - rst_w) / 2, 617, "● 按中键 · 再过一遍全部词", Font24, true);

    drawFooter(710, "Leitner 间隔复习 · 明日继续保持");
}

void FlashcardUI::drawVolumeBar(int x, int y, int volume) {
    // 10 格音量条 (每格 16x18, 间距 4), 格子小, 实心填充不会造成明显残影
    int filled = (volume + 5) / 10;
    for (int i = 0; i < 10; i++) {
        int sx = x + i * 20;
        if (i < filled) {
            _canvas.fillRect(sx, y, 16, 18, true);
        } else {
            _canvas.drawRect(sx, y, 16, 18, true);
        }
    }
}

void FlashcardUI::renderMenu(const SettingsMenu& menu) {
    _canvas.clear(true);
    const UserSettings& s = menu.settings();

    _canvas.drawStringCentered(60, "设置", Font48, true);
    _canvas.drawFastHLine(40, 130, UI_WIDTH - 80, true);

    const int row_y0 = 160;
    const int row_h = 84;
    const int label_x = 44;
    const int value_right = UI_WIDTH - 44;

    for (int i = 0; i < MENU_ITEM_COUNT; i++) {
        int ry = row_y0 + i * row_h;
        int ty = ry + (row_h - 24) / 2;  // Font24 行内垂直居中
        if (i == menu.cursor()) {
            // 选中行: 线框; 正在调音量时加粗
            drawOutlineButton(20, ry + 8, UI_WIDTH - 40, row_h - 16, 14, menu.editingVolume() ? 4 : 2);
        }

        char value[16];
        switch (i) {
            case MENU_DECK: {
                const char* deck = menu.deckLabel();
                _canvas.drawString(label_x, ty, "词库", Font24, true);
                _canvas.drawString(value_right - _canvas.measureStringWidth(deck, Font24), ty, deck, Font24, true);
                break;
            }
            case MENU_NEW_PER_DAY: {
                char n[16];
                if (s.new_per_day == 0) snprintf(n, sizeof(n), "不限");
                else snprintf(n, sizeof(n), "%d 个", s.new_per_day);
                _canvas.drawString(label_x, ty, "每日新词", Font24, true);
                _canvas.drawString(value_right - _canvas.measureStringWidth(n, Font24), ty, n, Font24, true);
                break;
            }
            case MENU_VOLUME:
                _canvas.drawString(label_x, ty, "音量", Font24, true);
                drawVolumeBar(140, ty + 3, s.volume);
                snprintf(value, sizeof(value), "%d%%", s.volume);
                _canvas.drawString(value_right - _canvas.measureStringWidth(value, Font24), ty, value, Font24, true);
                break;
            case MENU_DIRECTION: {
                const char* dir = (s.dir == DIR_EN_TO_ZH) ? "英 → 中" : "中 → 英";
                _canvas.drawString(label_x, ty, "背卡方向", Font24, true);
                _canvas.drawString(value_right - _canvas.measureStringWidth(dir, Font24), ty, dir, Font24, true);
                break;
            }
            case MENU_AUTO_SPEAK: {
                const char* on = s.auto_speak ? "开" : "关";
                _canvas.drawString(label_x, ty, "切卡自动发音", Font24, true);
                _canvas.drawString(value_right - _canvas.measureStringWidth(on, Font24), ty, on, Font24, true);
                break;
            }
            default:
                _canvas.drawString(label_x, ty, "保存并退出", Font24, true);
                break;
        }
    }

    const char* hint = menu.editingVolume() ? "▲ 上拨 加大 · ▼ 下拨 减小 · 中键 完成"
                                            : "▲▼ 选择 · 中键 确认 · 长按 BOOT 退出";
    _canvas.drawStringCentered(718, hint, Font20, true);
}

// 以 (cx, cy) 为中心旋转的圆角卡片: 白底 + 2px 边框 (圆角每角用 4 点近似)
void FlashcardUI::drawTiltedCard(int cx, int cy, int w, int h, float degrees) {
    const float rad = degrees * (float)M_PI / 180.0f;
    const float c = cosf(rad), s = sinf(rad);
    const float r = 14.0f;
    // 四个圆角的圆心 (相对卡片中心) 与各自起始角
    const float hx = w / 2.0f - r, hy = h / 2.0f - r;
    const float centers[4][2] = {{hx, -hy}, {hx, hy}, {-hx, hy}, {-hx, -hy}};
    const float start[4] = {-90.0f, 0.0f, 90.0f, 180.0f};

    for (int ring = 0; ring < 2; ring++) {  // ring 0: 外轮廓 (填白 + 描边); ring 1: 向内 1px 再描一圈
        float rr = r - ring;
        int xs[16], ys[16];
        for (int k = 0; k < 4; k++) {
            for (int i = 0; i < 4; i++) {
                float a = (start[k] + i * 30.0f) * (float)M_PI / 180.0f;
                float px = centers[k][0] + rr * cosf(a);
                float py = centers[k][1] + rr * sinf(a);
                xs[k * 4 + i] = cx + (int)lroundf(px * c - py * s);
                ys[k * 4 + i] = cy + (int)lroundf(px * s + py * c);
            }
        }
        if (ring == 0) _canvas.fillPolygon(xs, ys, 16, false);
        _canvas.drawPolygon(xs, ys, 16, true);
    }
}

void FlashcardUI::renderPowerOff() {
    _canvas.clear(true);

    // 扇形摊开的一叠单词卡: 后两张倾斜, 最上面一张摆正并带投影
    const int cw = 300, ch = 200;
    const int top_x = (UI_WIDTH - cw) / 2, top_y = 200;
    drawTiltedCard(UI_WIDTH / 2 - 10, top_y + ch / 2 + 6, cw, ch, -9.0f);
    drawTiltedCard(UI_WIDTH / 2 + 12, top_y + ch / 2 - 4, cw, ch, 7.0f);
    _canvas.fillRoundRect(top_x + 6, top_y + 6, cw, ch, 14, true);
    _canvas.fillRoundRect(top_x, top_y, cw, ch, 14, false);
    drawOutlineButton(top_x, top_y, cw, ch, 14, 2);

    // 最上面一张: 单词卡正反两面的信息
    _canvas.drawRoundRect(top_x + 16, top_y + 14, 86, 22, 11, true);
    _canvas.drawString(top_x + 22, top_y + 17, "# WORDS", Font16, true);
    drawMiniBoxProgress(top_x + cw - 16 - 76, top_y + 22, 3);
    _canvas.drawStringCentered(top_y + 52, "spell", Font48, true);
    _canvas.drawStringCentered(top_y + 118, "/spel/ · 拼写", Font24, true);
    _canvas.drawDottedHLine(top_x + 30, top_y + 166, cw - 60, 5, true);

    // 关机文字
    _canvas.drawStringCentered(490, "已关机", Font48, true);
    _canvas.drawDottedHLine(170, 560, UI_WIDTH - 340, 5, true);
    _canvas.drawStringCentered(580, "按电源键开机", Font24, true);
    _canvas.drawStringCentered(736, "墨词 InkWords", Font24, true);
}

void FlashcardUI::renderError(const char* title, const char* detail) {
    _canvas.clear(true);
    _canvas.drawRoundRect(16, 30, UI_WIDTH - 32, UI_HEIGHT - 60, 18, true);
    _canvas.drawRoundRect(17, 31, UI_WIDTH - 34, UI_HEIGHT - 62, 17, true);

    drawHeadline(title, 300);
    _canvas.drawFastHLine(50, 380, UI_WIDTH - 100, true);
    _canvas.drawStringWrapped(50, 410, UI_WIDTH - 100, 6, detail ? detail : "", Font24, true, 600);
    _canvas.drawStringCentered(708, "修复后长按电源键关机, 再开机重试", Font20, true);
}
