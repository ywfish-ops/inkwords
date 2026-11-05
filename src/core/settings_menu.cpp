#include "settings_menu.h"
#include <string.h>

void SettingsMenu::open(const UserSettings& settings, const DeckEntry* decks, int deck_count) {
    _settings = settings;
    _decks = decks;
    _deck_count = decks ? deck_count : 0;
    _cursor = MENU_DECK;
    _editing_volume = false;
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
        return MENU_ACT_EXIT;
    }
    if (_editing_volume) return handleEditVolume(evt);

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
