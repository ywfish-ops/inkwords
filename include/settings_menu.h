#ifndef SETTINGS_MENU_H
#define SETTINGS_MENU_H

#include "spell_types.h"
#include "study_day.h"

/**
 * @file settings_menu.h
 * @brief 设置菜单状态机 (纯逻辑, 不依赖硬件)
 *        拨轮上下移动光标 / 调音量 / 调日期时间, 中键确认, 长按 BOOT 退出
 */

typedef enum {
    MENU_DECK = 0,
    MENU_NEW_PER_DAY,
    MENU_VOLUME,
    MENU_DIRECTION,
    MENU_AUTO_SPEAK,
    MENU_DATETIME,
    MENU_EXIT,
    MENU_ITEM_COUNT
} menu_item_t;

typedef enum {
    MENU_ACT_NONE = 0,       // 无变化
    MENU_ACT_REDRAW,         // 需要重绘菜单
    MENU_ACT_VOLUME_CHANGED, // 音量已改变: 重绘并用新音量试听
    MENU_ACT_EXIT,           // 退出菜单 (调用方保存设置)
    MENU_ACT_TIME_SET        // 日期时间调完: 调用方把 time() 写入 RTC
} menu_action_t;

// 调日期时间时依次编辑的字段
typedef enum {
    TIME_FIELD_YEAR = 0,
    TIME_FIELD_MONTH,
    TIME_FIELD_DAY,
    TIME_FIELD_HOUR,
    TIME_FIELD_MINUTE
} time_field_t;

// 每日新词的下一个选项 (10/20/30/50/不限 循环, 0 表示不限)
uint8_t next_new_per_day(uint8_t current);

class SettingsMenu {
public:
    static const uint8_t VOLUME_STEP = 10;

    static const int YEAR_MIN = 2024;
    static const int YEAR_MAX = 2099;  // RTC 只存两位年份

    // decks: 可选词库 (来自 manifest), 中键在其间循环切换并写入 settings.deck_id
    // now: RTC 当前时间, 时钟无效时传 nullptr (从 2026-01-01 00:00 开始调)
    void open(const UserSettings& settings, const DeckEntry* decks = nullptr, int deck_count = 0,
              const LocalDateTime* now = nullptr);
    menu_action_t handle(input_event_t evt);

    int  cursor() const { return _cursor; }
    bool editingVolume() const { return _editing_volume; }
    bool editingTime() const { return _editing_time; }
    time_field_t timeField() const { return _time_field; }
    const LocalDateTime& time() const { return _time; }
    bool timeValid() const { return _time_valid; }
    const UserSettings& settings() const { return _settings; }
    // 当前选中词库的短标签; 无词库列表时返回 deck_id
    const char* deckLabel() const;

private:
    menu_action_t handleEditVolume(input_event_t evt);
    menu_action_t handleEditTime(input_event_t evt);

    UserSettings _settings = {};
    const DeckEntry* _decks = nullptr;
    int  _deck_count = 0;
    int  _cursor = 0;
    bool _editing_volume = false;
    bool _editing_time = false;
    time_field_t _time_field = TIME_FIELD_YEAR;
    LocalDateTime _time = {};
    bool _time_valid = false;
};

#endif // SETTINGS_MENU_H
