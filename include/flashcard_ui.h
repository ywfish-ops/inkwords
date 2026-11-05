#ifndef FLASHCARD_UI_H
#define FLASHCARD_UI_H

#include "canvas_480x800.h"
#include "spell_types.h"
#include "settings_menu.h"

/**
 * @file flashcard_ui.h
 * @brief SPBCN 单词卡 480x800 墨水屏视觉呈现组件
 */

class FlashcardUI {
public:
    FlashcardUI(Canvas480x800& canvas)
        : _canvas(canvas), _battery_percent(100), _charging(false), _dir(DIR_EN_TO_ZH), _clock_warning(false) {
        setDeckLabel("SPBCN");
    }

    // 状态栏左上角的词库标签, 如 "SPBCN R1"
    void setDeckLabel(const char* label);

    // 背卡方向: 英→中 正面显示英文+音标; 中→英 正面显示中文释义, 听发音回想拼写
    void setDirection(review_dir_t dir) { _dir = dir; }

    // 更新电池电量 (0 ~ 100%, 负数表示未知) 与充电状态 (充电中在电池图标内画闪电)
    void setBattery(int percent, bool charging = false) {
        _battery_percent = percent;
        _charging = charging;
    }

    // 渲染卡片正面 (英文单词、音标、分类标签、Leitner当前盒进度、翻面操作提示)
    void renderFront(const WordCard& card, const CardProgress& prog, int card_idx, int total_cards);

    // 渲染卡片背面 (揭晓答案: 英文、音标、中文释义、英文定义、例句、评定操作条)
    void renderBack(const WordCard& card, const CardProgress& prog, int card_idx, int total_cards);

    // 渲染今日完成页: 盒子分布, 以及明天到期的复习卡数 / 可学的新词数
    void renderDone(int total_cards, const int box_counts[6], int tomorrow_due, int tomorrow_new);

    // RTC 未对时: 页面底部提示连电脑对时
    void setClockWarning(bool on) { _clock_warning = on; }

    // 渲染设置菜单 (音量 / 背卡方向 / 切卡自动发音 / 退出)
    void renderMenu(const SettingsMenu& menu);

    // 渲染关机页 (断电后墨水屏保留此画面)
    void renderPowerOff();

    // 渲染错误页 (如未检测到 TF 卡 / 词库读取失败), detail 自动换行
    void renderError(const char* title, const char* detail);

private:
    Canvas480x800& _canvas;
    int            _battery_percent;
    bool           _charging;
    review_dir_t   _dir;
    char           _deck_label[20];
    bool           _clock_warning;

    // 页面最底部一行: 平时是操作提示, 未对时时换成对时提示
    void drawFooter(int y, const char* normal);

    void drawHeader(const char* deck_id, int box, int card_idx, int total_cards);
    void drawBattery(int x, int y, int percent, bool charging);
    void drawMiniBoxProgress(int x, int y, int active_box);
    void drawSpeakerIcon(int x, int y);
    void drawOutlineButton(int x, int y, int w, int h, int r, int thickness);
    void drawVolumeBar(int x, int y, int volume);
    void drawHeadline(const char* text, int y, bool large = false);
    void drawTiltedCard(int cx, int cy, int w, int h, float degrees);
};

#endif // FLASHCARD_UI_H
