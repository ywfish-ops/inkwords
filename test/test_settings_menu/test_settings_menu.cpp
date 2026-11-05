#include <unity.h>
#include <string.h>
#include "settings_menu.h"

static SettingsMenu s_menu;

static UserSettings defaults() {
    UserSettings s;
    memset(&s, 0, sizeof(s));
    s.dir = DIR_EN_TO_ZH;
    s.auto_speak = false;
    s.volume = 100;
    return s;
}

void setUp() { s_menu.open(defaults()); }

// 打开菜单并把光标移到音量行 (前两行是词库、每日新词)
static void openAtVolume(const UserSettings& s) {
    s_menu.open(s);
    s_menu.handle(EVT_DIAL_DOWN);
    s_menu.handle(EVT_DIAL_DOWN);
}
void tearDown() {}

void test_open_starts_at_first_item() {
    TEST_ASSERT_EQUAL(MENU_DECK, s_menu.cursor());
    TEST_ASSERT_FALSE(s_menu.editingVolume());
}

void test_cursor_moves_and_clamps() {
    TEST_ASSERT_EQUAL(MENU_ACT_NONE, s_menu.handle(EVT_DIAL_UP));  // 已在顶部
    TEST_ASSERT_EQUAL(MENU_ACT_REDRAW, s_menu.handle(EVT_DIAL_DOWN));
    TEST_ASSERT_EQUAL(MENU_NEW_PER_DAY, s_menu.cursor());
    s_menu.handle(EVT_DIAL_DOWN);
    TEST_ASSERT_EQUAL(MENU_VOLUME, s_menu.cursor());
    s_menu.handle(EVT_DIAL_DOWN);
    TEST_ASSERT_EQUAL(MENU_DIRECTION, s_menu.cursor());
    s_menu.handle(EVT_DIAL_DOWN);
    TEST_ASSERT_EQUAL(MENU_AUTO_SPEAK, s_menu.cursor());
    s_menu.handle(EVT_DIAL_DOWN);
    TEST_ASSERT_EQUAL(MENU_DATETIME, s_menu.cursor());
    s_menu.handle(EVT_DIAL_DOWN);
    TEST_ASSERT_EQUAL(MENU_EXIT, s_menu.cursor());
    TEST_ASSERT_EQUAL(MENU_ACT_NONE, s_menu.handle(EVT_DIAL_DOWN));  // 已在底部
    TEST_ASSERT_EQUAL(MENU_EXIT, s_menu.cursor());
    TEST_ASSERT_EQUAL(MENU_ACT_REDRAW, s_menu.handle(EVT_DIAL_UP));
    TEST_ASSERT_EQUAL(MENU_DATETIME, s_menu.cursor());
}

void test_volume_edit_steps_and_clamps() {
    s_menu.handle(EVT_DIAL_DOWN);
    s_menu.handle(EVT_DIAL_DOWN);
    TEST_ASSERT_EQUAL(MENU_ACT_REDRAW, s_menu.handle(EVT_DIAL_CONFIRM));
    TEST_ASSERT_TRUE(s_menu.editingVolume());
    TEST_ASSERT_EQUAL(MENU_ACT_NONE, s_menu.handle(EVT_DIAL_UP));  // 已是 100
    TEST_ASSERT_EQUAL(MENU_ACT_VOLUME_CHANGED, s_menu.handle(EVT_DIAL_DOWN));
    TEST_ASSERT_EQUAL(90, s_menu.settings().volume);
    for (int i = 0; i < 12; i++) s_menu.handle(EVT_DIAL_DOWN);
    TEST_ASSERT_EQUAL(0, s_menu.settings().volume);
    TEST_ASSERT_EQUAL(MENU_ACT_VOLUME_CHANGED, s_menu.handle(EVT_DIAL_UP));
    TEST_ASSERT_EQUAL(10, s_menu.settings().volume);
    // 调音量时光标不动
    TEST_ASSERT_EQUAL(MENU_VOLUME, s_menu.cursor());
    TEST_ASSERT_EQUAL(MENU_ACT_REDRAW, s_menu.handle(EVT_DIAL_CONFIRM));
    TEST_ASSERT_FALSE(s_menu.editingVolume());
}

// 非 10 的倍数 (如旧设置的 75) 调节后对齐到 10 的倍数
void test_volume_snaps_to_step() {
    UserSettings s = defaults();
    s.volume = 75;
    openAtVolume(s);
    s_menu.handle(EVT_DIAL_CONFIRM);
    s_menu.handle(EVT_DIAL_UP);
    TEST_ASSERT_EQUAL(80, s_menu.settings().volume);
    openAtVolume(s);
    s_menu.handle(EVT_DIAL_CONFIRM);
    s_menu.handle(EVT_DIAL_DOWN);
    TEST_ASSERT_EQUAL(70, s_menu.settings().volume);
}

void test_toggle_direction_and_auto_speak() {
    for (int i = 0; i < 3; i++) s_menu.handle(EVT_DIAL_DOWN);
    TEST_ASSERT_EQUAL(MENU_ACT_REDRAW, s_menu.handle(EVT_DIAL_CONFIRM));
    TEST_ASSERT_EQUAL(DIR_ZH_TO_EN, s_menu.settings().dir);
    s_menu.handle(EVT_DIAL_CONFIRM);
    TEST_ASSERT_EQUAL(DIR_EN_TO_ZH, s_menu.settings().dir);
    s_menu.handle(EVT_DIAL_DOWN);
    s_menu.handle(EVT_DIAL_CONFIRM);
    TEST_ASSERT_TRUE(s_menu.settings().auto_speak);
}

void test_exit_item_and_long_press_exit() {
    for (int i = 0; i < MENU_EXIT; i++) s_menu.handle(EVT_DIAL_DOWN);
    TEST_ASSERT_EQUAL(MENU_ACT_EXIT, s_menu.handle(EVT_DIAL_CONFIRM));
    s_menu.open(defaults());
    TEST_ASSERT_EQUAL(MENU_ACT_EXIT, s_menu.handle(EVT_BOOT_LONG_PRESS));
    // 调音量途中长按也能退出, 且保留已调的音量
    openAtVolume(defaults());
    s_menu.handle(EVT_DIAL_CONFIRM);
    s_menu.handle(EVT_DIAL_DOWN);
    TEST_ASSERT_EQUAL(MENU_ACT_EXIT, s_menu.handle(EVT_BOOT_LONG_PRESS));
    TEST_ASSERT_EQUAL(90, s_menu.settings().volume);
}

// 打开菜单会复位状态 (上次退出时停在调音量也不影响)
void test_reopen_resets_state() {
    openAtVolume(defaults());
    s_menu.handle(EVT_DIAL_CONFIRM);
    s_menu.open(defaults());
    TEST_ASSERT_FALSE(s_menu.editingVolume());
    TEST_ASSERT_EQUAL(MENU_DECK, s_menu.cursor());
}

void test_boot_click_is_ignored() {
    TEST_ASSERT_EQUAL(MENU_ACT_NONE, s_menu.handle(EVT_BOOT_CLICK));
}

static const DeckEntry DECKS[] = {
    {"spbcn_r1", "spbcn_r1.json", "SPBCN R1"},
    {"spbcn_r2", "spbcn_r2.json", "SPBCN R2"},
    {"spbcn_r3", "spbcn_r3.json", "SPBCN R3"},
};

static UserSettings withDeck(const char* id) {
    UserSettings s = defaults();
    strcpy(s.deck_id, id);
    return s;
}

void test_deck_cycles_through_list() {
    s_menu.open(withDeck("spbcn_r2"), DECKS, 3);
    TEST_ASSERT_EQUAL_STRING("SPBCN R2", s_menu.deckLabel());
    TEST_ASSERT_EQUAL(MENU_ACT_REDRAW, s_menu.handle(EVT_DIAL_CONFIRM));
    TEST_ASSERT_EQUAL_STRING("spbcn_r3", s_menu.settings().deck_id);
    s_menu.handle(EVT_DIAL_CONFIRM);
    TEST_ASSERT_EQUAL_STRING("spbcn_r1", s_menu.settings().deck_id);  // 循环回第一套
    TEST_ASSERT_EQUAL_STRING("SPBCN R1", s_menu.deckLabel());
    TEST_ASSERT_EQUAL(MENU_DECK, s_menu.cursor());
}

// 当前词库不在列表里 (manifest 改过): 中键从第一套开始
void test_deck_unknown_current_starts_at_first() {
    s_menu.open(withDeck("no_such_deck"), DECKS, 3);
    TEST_ASSERT_EQUAL_STRING("no_such_deck", s_menu.deckLabel());
    s_menu.handle(EVT_DIAL_CONFIRM);
    TEST_ASSERT_EQUAL_STRING("spbcn_r1", s_menu.settings().deck_id);
}

// 只有一套或没有词库列表: 中键无效果
void test_deck_single_or_none_does_nothing() {
    s_menu.open(withDeck("spbcn_r1"), DECKS, 1);
    TEST_ASSERT_EQUAL(MENU_ACT_NONE, s_menu.handle(EVT_DIAL_CONFIRM));
    TEST_ASSERT_EQUAL_STRING("spbcn_r1", s_menu.settings().deck_id);
    s_menu.open(withDeck("spbcn_r1"));
    TEST_ASSERT_EQUAL(MENU_ACT_NONE, s_menu.handle(EVT_DIAL_CONFIRM));
    TEST_ASSERT_EQUAL_STRING("spbcn_r1", s_menu.deckLabel());
}

// 每日新词: 10 -> 20 -> 30 -> 50 -> 不限 -> 10
void test_new_per_day_cycles() {
    UserSettings s = defaults();
    s.new_per_day = 10;
    s_menu.open(s);
    s_menu.handle(EVT_DIAL_DOWN);
    const uint8_t expect[] = {20, 30, 50, 0, 10};
    for (uint8_t v : expect) {
        TEST_ASSERT_EQUAL(MENU_ACT_REDRAW, s_menu.handle(EVT_DIAL_CONFIRM));
        TEST_ASSERT_EQUAL(v, s_menu.settings().new_per_day);
    }
}

// 不在选项里的值 (如旧设置 15) 跳到下一个更大的选项, 超过 50 跳到不限
void test_next_new_per_day_from_odd_values() {
    TEST_ASSERT_EQUAL(20, next_new_per_day(15));
    TEST_ASSERT_EQUAL(10, next_new_per_day(5));
    TEST_ASSERT_EQUAL(0, next_new_per_day(80));
}

// ---- 日期时间 ----
static const LocalDateTime NOW = {2026, 1, 31, 23, 59, 30};

// 打开菜单 (带当前时间), 光标移到日期时间行并进入编辑
static void openEditTime(const LocalDateTime* now) {
    s_menu.open(defaults(), nullptr, 0, now);
    while (s_menu.cursor() != MENU_DATETIME) s_menu.handle(EVT_DIAL_DOWN);
    s_menu.handle(EVT_DIAL_CONFIRM);
}

void test_time_edit_starts_at_year_with_current_time() {
    openEditTime(&NOW);
    TEST_ASSERT_TRUE(s_menu.editingTime());
    TEST_ASSERT_EQUAL(TIME_FIELD_YEAR, s_menu.timeField());
    TEST_ASSERT_TRUE(s_menu.timeValid());
    TEST_ASSERT_EQUAL(2026, s_menu.time().year);
    TEST_ASSERT_EQUAL(31, s_menu.time().day);
    TEST_ASSERT_EQUAL(MENU_DATETIME, s_menu.cursor());
}

// 时钟无效时从 2026-01-01 00:00 开始调
void test_time_edit_without_valid_clock() {
    openEditTime(nullptr);
    TEST_ASSERT_FALSE(s_menu.timeValid());
    TEST_ASSERT_EQUAL(2026, s_menu.time().year);
    TEST_ASSERT_EQUAL(1, s_menu.time().month);
    TEST_ASSERT_EQUAL(1, s_menu.time().day);
    TEST_ASSERT_EQUAL(0, s_menu.time().hour);
}

// 年份 2024~2099 首尾循环
void test_time_year_wraps() {
    openEditTime(&NOW);
    for (int i = 0; i < 2099 - 2026; i++) s_menu.handle(EVT_DIAL_UP);
    TEST_ASSERT_EQUAL(2099, s_menu.time().year);
    TEST_ASSERT_EQUAL(MENU_ACT_REDRAW, s_menu.handle(EVT_DIAL_UP));
    TEST_ASSERT_EQUAL(2024, s_menu.time().year);
    s_menu.handle(EVT_DIAL_DOWN);
    TEST_ASSERT_EQUAL(2099, s_menu.time().year);
}

// 改月份时日期按当月天数收紧: 1/31 -> 2 月 28 日; 闰年 2028 -> 29 日
void test_time_day_clamped_to_month_and_leap_year() {
    openEditTime(&NOW);
    s_menu.handle(EVT_DIAL_CONFIRM);  // -> 月
    TEST_ASSERT_EQUAL(TIME_FIELD_MONTH, s_menu.timeField());
    s_menu.handle(EVT_DIAL_UP);
    TEST_ASSERT_EQUAL(2, s_menu.time().month);
    TEST_ASSERT_EQUAL(28, s_menu.time().day);

    openEditTime(&NOW);
    s_menu.handle(EVT_DIAL_UP);
    s_menu.handle(EVT_DIAL_UP);       // 2028
    s_menu.handle(EVT_DIAL_CONFIRM);
    s_menu.handle(EVT_DIAL_UP);       // 2 月
    TEST_ASSERT_EQUAL(29, s_menu.time().day);
}

// 日、时、分在各自范围内循环
void test_time_fields_wrap() {
    openEditTime(&NOW);
    s_menu.handle(EVT_DIAL_CONFIRM);
    s_menu.handle(EVT_DIAL_CONFIRM);  // -> 日 (1 月 31 日)
    TEST_ASSERT_EQUAL(TIME_FIELD_DAY, s_menu.timeField());
    s_menu.handle(EVT_DIAL_UP);
    TEST_ASSERT_EQUAL(1, s_menu.time().day);
    s_menu.handle(EVT_DIAL_DOWN);
    TEST_ASSERT_EQUAL(31, s_menu.time().day);
    s_menu.handle(EVT_DIAL_CONFIRM);  // -> 时 (23)
    s_menu.handle(EVT_DIAL_UP);
    TEST_ASSERT_EQUAL(0, s_menu.time().hour);
    s_menu.handle(EVT_DIAL_CONFIRM);  // -> 分 (59)
    s_menu.handle(EVT_DIAL_UP);
    TEST_ASSERT_EQUAL(0, s_menu.time().minute);
    s_menu.handle(EVT_DIAL_DOWN);
    TEST_ASSERT_EQUAL(59, s_menu.time().minute);
}

// 调完分钟再按中键: 返回 MENU_ACT_TIME_SET, 退出编辑, 秒归零, 时间变为有效
void test_time_set_after_minute() {
    openEditTime(nullptr);
    for (int f = 0; f < TIME_FIELD_MINUTE; f++) {
        TEST_ASSERT_EQUAL(MENU_ACT_REDRAW, s_menu.handle(EVT_DIAL_CONFIRM));
    }
    s_menu.handle(EVT_DIAL_UP);
    TEST_ASSERT_EQUAL(MENU_ACT_TIME_SET, s_menu.handle(EVT_DIAL_CONFIRM));
    TEST_ASSERT_FALSE(s_menu.editingTime());
    TEST_ASSERT_TRUE(s_menu.timeValid());
    TEST_ASSERT_EQUAL(1, s_menu.time().minute);
    TEST_ASSERT_EQUAL(0, s_menu.time().second);
    TEST_ASSERT_EQUAL(MENU_DATETIME, s_menu.cursor());
}

// 调时间途中长按 BOOT 退出菜单, 不算设置时间
void test_time_edit_long_press_exits_without_setting() {
    openEditTime(&NOW);
    s_menu.handle(EVT_DIAL_UP);
    TEST_ASSERT_EQUAL(MENU_ACT_EXIT, s_menu.handle(EVT_BOOT_LONG_PRESS));
    TEST_ASSERT_FALSE(s_menu.editingTime());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_time_edit_starts_at_year_with_current_time);
    RUN_TEST(test_time_edit_without_valid_clock);
    RUN_TEST(test_time_year_wraps);
    RUN_TEST(test_time_day_clamped_to_month_and_leap_year);
    RUN_TEST(test_time_fields_wrap);
    RUN_TEST(test_time_set_after_minute);
    RUN_TEST(test_time_edit_long_press_exits_without_setting);
    RUN_TEST(test_open_starts_at_first_item);
    RUN_TEST(test_cursor_moves_and_clamps);
    RUN_TEST(test_volume_edit_steps_and_clamps);
    RUN_TEST(test_volume_snaps_to_step);
    RUN_TEST(test_toggle_direction_and_auto_speak);
    RUN_TEST(test_exit_item_and_long_press_exit);
    RUN_TEST(test_reopen_resets_state);
    RUN_TEST(test_boot_click_is_ignored);
    RUN_TEST(test_deck_cycles_through_list);
    RUN_TEST(test_deck_unknown_current_starts_at_first);
    RUN_TEST(test_deck_single_or_none_does_nothing);
    RUN_TEST(test_new_per_day_cycles);
    RUN_TEST(test_next_new_per_day_from_odd_values);
    return UNITY_END();
}
