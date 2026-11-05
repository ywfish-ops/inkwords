#include "settings_menu.h"
#include <string.h>

void SettingsMenu::open(const UserSettings& settings, const DeckEntry* decks, int deck_count,
                        const LocalDateTime* now) {
    _settings = settings;
    _decks = decks;
    _deck_count = decks ? deck_count : 0;
    _cursor = MENU_DECK;
    _editing_volume = false;
    _editing_time = false;
    _time_field = TIME_FIELD_YEAR;
    _time_valid = now != nullptr;
    _time = now ? *now : LocalDateTime{2026, 1, 1, 0, 0, 0};
}

static int findDeck(const DeckEntry* decks, int n, const char* id) {
    for (int i = 0; i < n; i++) {
        if (strcmp(decks[i].id, id) == 0) return i;
    }
    return -1;
}

// 每日新词选项: 10 -> 20 -> 30 -> 50 -> 不限 (0) -> 10; 不在选项里的值跳到下一个更大的
static const uint8_t NEW_PER_DAY_OPTIONS[] = {10, 20, 30, 50, 0};

uint8_t next_new_per_day(uint8_t current) {
    if (current == 0) return NEW_PER_DAY_OPTIONS[0];
    for (uint8_t v : NEW_PER_DAY_OPTIONS) {
        if (v > current) return v;
    }
    return 0;
}

const char* SettingsMenu::deckLabel() const {
    int i = findDeck(_decks, _deck_count, _settings.deck_id);
    return i >= 0 ? _decks[i].label : _settings.deck_id;
}

menu_action_t SettingsMenu::handle(input_event_t evt) {
    if (evt == EVT_BOOT_LONG_PRESS) {
        _editing_volume = false;
        _editing_time = false;
        return MENU_ACT_EXIT;
    }
    if (_editing_volume) return handleEditVolume(evt);
    if (_editing_time) return handleEditTime(evt);

    switch (evt) {
        case EVT_DIAL_UP:
            if (_cursor == 0) return MENU_ACT_NONE;
            _cursor--;
            return MENU_ACT_REDRAW;
        case EVT_DIAL_DOWN:
            if (_cursor == MENU_ITEM_COUNT - 1) return MENU_ACT_NONE;
            _cursor++;
            return MENU_ACT_REDRAW;
        case EVT_DIAL_CONFIRM:
            switch (_cursor) {
                case MENU_DECK: {
                    if (_deck_count <= 1) return MENU_ACT_NONE;
                    int next = (findDeck(_decks, _deck_count, _settings.deck_id) + 1) % _deck_count;
                    strncpy(_settings.deck_id, _decks[next].id, sizeof(_settings.deck_id) - 1);
                    _settings.deck_id[sizeof(_settings.deck_id) - 1] = '\0';
                    return MENU_ACT_REDRAW;
                }
                case MENU_NEW_PER_DAY:
                    _settings.new_per_day = next_new_per_day(_settings.new_per_day);
                    return MENU_ACT_REDRAW;
                case MENU_VOLUME:
                    _editing_volume = true;
                    return MENU_ACT_REDRAW;
                case MENU_DIRECTION:
                    _settings.dir = (_settings.dir == DIR_EN_TO_ZH) ? DIR_ZH_TO_EN : DIR_EN_TO_ZH;
                    return MENU_ACT_REDRAW;
                case MENU_AUTO_SPEAK:
                    _settings.auto_speak = !_settings.auto_speak;
                    return MENU_ACT_REDRAW;
                case MENU_DATETIME:
                    _editing_time = true;
                    _time_field = TIME_FIELD_YEAR;
                    return MENU_ACT_REDRAW;
                default:
                    return MENU_ACT_EXIT;
            }
        default:
            return MENU_ACT_NONE;
    }
}

// 上拨加音量, 下拨减音量, 按 VOLUME_STEP 对齐; 中键结束调节
menu_action_t SettingsMenu::handleEditVolume(input_event_t evt) {
    int vol = _settings.volume;
    int next = vol;
    switch (evt) {
        case EVT_DIAL_UP:
            next = (vol / VOLUME_STEP + 1) * VOLUME_STEP;
            if (next > 100) next = 100;
            break;
        case EVT_DIAL_DOWN:
            next = ((vol + VOLUME_STEP - 1) / VOLUME_STEP - 1) * VOLUME_STEP;
            if (next < 0) next = 0;
            break;
        case EVT_DIAL_CONFIRM:
            _editing_volume = false;
            return MENU_ACT_REDRAW;
        default:
            return MENU_ACT_NONE;
    }
    if (next == vol) return MENU_ACT_NONE;
    _settings.volume = (uint8_t)next;
    return MENU_ACT_VOLUME_CHANGED;
}

static bool isLeap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

static int daysInMonth(int y, int m) {
    static const int DAYS[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return (m == 2 && isLeap(y)) ? 29 : DAYS[m - 1];
}

// v 加上 delta 后在 [lo, hi] 内首尾循环
static int wrap(int v, int delta, int lo, int hi) {
    int span = hi - lo + 1;
    return lo + ((v - lo + delta) % span + span) % span;
}

// 上拨加 1, 下拨减 1 (各字段首尾循环); 中键进入下一字段, 调完分钟返回 TIME_SET
menu_action_t SettingsMenu::handleEditTime(input_event_t evt) {
    int delta = 0;
    switch (evt) {
        case EVT_DIAL_UP: delta = 1; break;
        case EVT_DIAL_DOWN: delta = -1; break;
        case EVT_DIAL_CONFIRM:
            if (_time_field == TIME_FIELD_MINUTE) {
                _editing_time = false;
                _time.second = 0;
                _time_valid = true;
                return MENU_ACT_TIME_SET;
            }
            _time_field = (time_field_t)(_time_field + 1);
            return MENU_ACT_REDRAW;
        default:
            return MENU_ACT_NONE;
    }
    switch (_time_field) {
        case TIME_FIELD_YEAR: _time.year = wrap(_time.year, delta, YEAR_MIN, YEAR_MAX); break;
        case TIME_FIELD_MONTH: _time.month = wrap(_time.month, delta, 1, 12); break;
        case TIME_FIELD_DAY: _time.day = wrap(_time.day, delta, 1, daysInMonth(_time.year, _time.month)); break;
        case TIME_FIELD_HOUR: _time.hour = wrap(_time.hour, delta, 0, 23); break;
        case TIME_FIELD_MINUTE: _time.minute = wrap(_time.minute, delta, 0, 59); break;
    }
    int dim = daysInMonth(_time.year, _time.month);
    if (_time.day > dim) _time.day = dim;  // 改年/月后收紧到当月天数
    return MENU_ACT_REDRAW;
}
