#include <unity.h>
#include <string.h>
#include "canvas_480x800.h"
#include "flashcard_ui.h"
#include "settings_menu.h"

static uint8_t s_buf[EPD_BUFFER_SIZE];
static Canvas480x800 s_canvas(s_buf, 3);
static FlashcardUI s_ui(s_canvas);

void setUp() { s_canvas.clear(true); }
void tearDown() {}

static bool rowsAreWhite(int y0, int y1, int x0 = 0, int x1 = UI_WIDTH) {
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++)
            if (s_canvas.getPixel(x, y)) return false;
    return true;
}

static void fillLong(char* dst, size_t cap, const char* unit) {
    dst[0] = '\0';
    while (strlen(dst) + strlen(unit) < cap) strcat(dst, unit);
}

static WordCard makeCard(const char* en) {
    WordCard c;
    memset(&c, 0, sizeof(c));
    strcpy(c.key, "t-0");
    strncpy(c.en, en, sizeof(c.en) - 1);
    strcpy(c.ipa, "/test/");
    strcpy(c.zh, "测试");
    strcpy(c.cat, "plain");
    strcpy(c.ex, "Short example.");
    strcpy(c.def_en, "short definition");
    return c;
}

// 背面: 超长释义 + 例句不得越过卡片框 (y=609) 压到评定按钮 (y=626)
void test_back_long_text_stays_inside_card() {
    WordCard c = makeCard("word");
    fillLong(c.def_en, sizeof(c.def_en), "a unit of time that is long ");
    fillLong(c.ex, sizeof(c.ex), "She has to give a speech again. ");
    CardProgress p = {1, 0, false};
    s_ui.renderBack(c, p, 0, 10);
    TEST_ASSERT_TRUE(rowsAreWhite(610, 626));
}

// 正面: 超长英文 (28 字母) 不得超出屏幕左右边缘
void test_front_long_word_fits_width() {
    WordCard c = makeCard("supplementary angle theorem");
    CardProgress p = {1, 0, false};
    s_ui.renderFront(c, p, 0, 10);
    // 单词区 y=200..300; 卡片双线框占 x=16,17 与 462,463, 框内两侧 16px 边距应保持空白
    TEST_ASSERT_TRUE(rowsAreWhite(200, 300, 18, 34));
    TEST_ASSERT_TRUE(rowsAreWhite(200, 300, UI_WIDTH - 34, UI_WIDTH - 18));
}

// 正面单词区 y=200..300 内墨迹的上下边界高度
static int frontWordInkHeight() {
    int top = -1, bottom = -1;
    for (int y = 200; y < 300; y++)
        if (!rowsAreWhite(y, y + 1, 24, UI_WIDTH - 24)) { if (top < 0) top = y; bottom = y; }
    return top < 0 ? -1 : bottom - top;
}

// 英→中正面: 普通单词用 Font64 大字 ("speech" 从 h 顶到 p 底, Font48 约 44px, Font64 约 59px)
void test_front_short_word_uses_large_font() {
    WordCard c = makeCard("speech");
    CardProgress p = {1, 0, false};
    s_ui.renderFront(c, p, 0, 10);
    TEST_ASSERT_TRUE(frontWordInkHeight() > 52);
}

// 英→中正面: 词库最长单词 mathematics 仍用大字一行放下, 不越出卡片内边距
void test_front_longest_deck_word_fits_large() {
    WordCard c = makeCard("mathematics");
    CardProgress p = {1, 0, false};
    s_ui.renderFront(c, p, 0, 10);
    TEST_ASSERT_TRUE(frontWordInkHeight() > 40);
    TEST_ASSERT_TRUE(rowsAreWhite(200, 300, 18, 34));
    TEST_ASSERT_TRUE(rowsAreWhite(200, 300, UI_WIDTH - 34, UI_WIDTH - 18));
}

// 英→中正面: 大字单词下伸部分 (p) 不压到音标行 (音标在 y=306)
void test_front_large_word_clear_of_ipa() {
    WordCard c = makeCard("speech");
    c.ipa[0] = '\0';
    CardProgress p = {1, 0, false};
    s_ui.renderFront(c, p, 0, 10);
    TEST_ASSERT_TRUE(rowsAreWhite(305, 340, 34, UI_WIDTH - 34));
}

// y0 起向下第一段墨迹 (连续非空白行) 的起止行; 找不到返回 false
static bool nextInkBand(int y0, int y1, int& top, int& bottom) {
    top = -1;
    for (int y = y0; y < y1; y++) {
        bool ink = !rowsAreWhite(y, y + 1, 34, UI_WIDTH - 34);
        if (ink && top < 0) top = y;
        if (!ink && top >= 0) { bottom = y - 1; return true; }
    }
    return false;
}

// 英→中正面: 单词与音标靠紧成一组, 与发音按钮拉开 (音标→按钮空白 >= 35px 且大于单词→音标空白)
void test_front_word_ipa_grouped_apart_from_button() {
    WordCard c = makeCard("speech");
    strcpy(c.ipa, "/spi:tʃ/");  // ʃ 有下伸部分, 是音标行最低的情况
    CardProgress p = {1, 0, false};
    s_ui.renderFront(c, p, 0, 10);
    int w_top, w_bot, i_top, i_bot, b_top, b_bot;
    TEST_ASSERT_TRUE(nextInkBand(200, 470, w_top, w_bot));
    TEST_ASSERT_TRUE(nextInkBand(w_bot + 1, 470, i_top, i_bot));
    TEST_ASSERT_TRUE(nextInkBand(i_bot + 1, 470, b_top, b_bot));
    int word_ipa_gap = i_top - w_bot;
    int ipa_button_gap = b_top - i_bot;
    TEST_ASSERT_GREATER_OR_EQUAL(35, ipa_button_gap);
    TEST_ASSERT_GREATER_THAN(word_ipa_gap, ipa_button_gap);
}

// 中→英正面: 中文释义仍用 Font48 (Font64 不含中文字模)
void test_front_zh_headline_stays_font48() {
    WordCard c = makeCard("speech");
    strcpy(c.zh, "演讲");
    CardProgress p = {1, 0, false};
    s_ui.setDirection(DIR_ZH_TO_EN);
    s_ui.renderFront(c, p, 0, 10);
    s_ui.setDirection(DIR_EN_TO_ZH);
    static uint8_t expect[EPD_BUFFER_SIZE];
    Canvas480x800 probe(expect, 3);
    probe.clear(true);
    probe.drawStringCentered(230, c.zh, Font48, true);
    for (int y = 220; y < 300; y++)
        for (int x = 34; x < UI_WIDTH - 34; x++)  // 避开卡片双线框
            TEST_ASSERT_EQUAL(probe.getPixel(x, y), s_canvas.getPixel(x, y));
}

// 英→中正面: 单词含 Font64 未收录的字 (é) 时整词退回 Font48, 不画出 '?'
void test_front_word_with_non_ascii_falls_back_to_font48() {
    WordCard c = makeCard("café");
    c.ipa[0] = '\0';
    CardProgress p = {1, 0, false};
    s_ui.renderFront(c, p, 0, 10);
    static uint8_t expect[EPD_BUFFER_SIZE];
    Canvas480x800 probe(expect, 3);
    probe.clear(true);
    probe.drawStringCentered(230, c.en, Font48, true);
    for (int y = 200; y < 300; y++)
        for (int x = 34; x < UI_WIDTH - 34; x++)
            TEST_ASSERT_EQUAL(probe.getPixel(x, y), s_canvas.getPixel(x, y));
}

// 英→中正面: 音标区 y=300..380 (单词下伸之下, 发音按钮之上) 内墨迹的上下边界高度
static int frontIpaInkHeight() {
    int top = -1, bottom = -1;
    for (int y = 300; y < 381; y++)
        if (!rowsAreWhite(y, y + 1, 34, UI_WIDTH - 34)) { if (top < 0) top = y; bottom = y; }
    return top < 0 ? -1 : bottom - top;
}

// 英→中正面: 音标随单词同比例放大用 Font32 ("/test/" 斜杠高度 Font24 约 18px, Font32 约 24px)
void test_front_ipa_uses_large_font() {
    WordCard c = makeCard("speech");
    CardProgress p = {1, 0, false};
    s_ui.renderFront(c, p, 0, 10);
    TEST_ASSERT_TRUE(frontIpaInkHeight() > 21);
}

// 英→中正面: 词库最长音标不越出卡片内边距, 不压到发音按钮 (y=384)
void test_front_longest_ipa_fits() {
    WordCard c = makeCard("mathematics");
    strcpy(c.ipa, "/ˌmæθəˈmætɪks/");
    CardProgress p = {1, 0, false};
    s_ui.renderFront(c, p, 0, 10);
    TEST_ASSERT_TRUE(frontIpaInkHeight() > 21);
    TEST_ASSERT_TRUE(rowsAreWhite(300, 381, 18, 34));
    TEST_ASSERT_TRUE(rowsAreWhite(300, 381, UI_WIDTH - 34, UI_WIDTH - 18));
    TEST_ASSERT_TRUE(rowsAreWhite(350, 384, 34, UI_WIDTH - 34));
}

// 电量未知 (-1): 电池图标内部不填充 (不得误显示为满电)
void test_unknown_battery_shows_empty_icon() {
    WordCard c = makeCard("word");
    CardProgress p = {1, 0, false};
    s_ui.setBattery(-1);
    s_ui.renderFront(c, p, 0, 10);
    s_ui.setBattery(100);
    // 电池图标位于 x=440, y=17 (22x12); 内部电量格 x=443..458, y=19..26
    TEST_ASSERT_TRUE(rowsAreWhite(19, 27, 443, 459));
}

// 充电中: 电池图标内画闪电, 与同电量未充电时的图标不同, 且仍在图标外壳内
void test_charging_shows_bolt_in_battery_icon() {
    WordCard c = makeCard("word");
    CardProgress p = {1, 0, false};
    static uint8_t plain[EPD_BUFFER_SIZE];
    s_ui.setBattery(60, false);
    s_ui.renderFront(c, p, 0, 10);
    memcpy(plain, s_buf, EPD_BUFFER_SIZE);
    s_canvas.clear(true);
    s_ui.setBattery(60, true);
    s_ui.renderFront(c, p, 0, 10);
    s_ui.setBattery(100, false);
    // 图标内部 x=443..458, y=19..26
    bool differs = false, has_ink = false;
    Canvas480x800 before(plain, 3);
    for (int y = 19; y < 27; y++)
        for (int x = 443; x < 459; x++) {
            differs |= before.getPixel(x, y) != s_canvas.getPixel(x, y);
            has_ink |= s_canvas.getPixel(x, y);
        }
    TEST_ASSERT_TRUE(differs);
    TEST_ASSERT_TRUE(has_ink);
    // 图标外壳之外 (正极凸起右侧) 保持空白
    TEST_ASSERT_TRUE(rowsAreWhite(15, 31, 465, 470));
}

// 按钮区域的黑色像素占比 (大面积实心黑在局刷翻转时会留下明显残影)
static float blackRatio(int x0, int y0, int x1, int y1) {
    int black = 0;
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++)
            black += s_canvas.getPixel(x, y);
    return (float)black / ((x1 - x0) * (y1 - y0));
}

void test_buttons_are_outlined_not_solid() {
    WordCard c = makeCard("word");
    CardProgress p = {1, 0, false};
    s_ui.renderFront(c, p, 0, 10);
    TEST_ASSERT_TRUE_MESSAGE(blackRatio(80, 550, 400, 606) < 0.3f, "front prompt button");
    s_canvas.clear(true);
    s_ui.renderBack(c, p, 0, 10);
    TEST_ASSERT_TRUE_MESSAGE(blackRatio(20, 690, 460, 744) < 0.3f, "back remembered button");
    s_canvas.clear(true);
    int counts[6] = {0, 1, 1, 1, 1, 1};
    s_ui.renderDone(5, counts, 3, 10);
    TEST_ASSERT_TRUE_MESSAGE(blackRatio(60, 600, 420, 658) < 0.3f, "done restart button");
}

static UserSettings menuDefaults() {
    UserSettings u;
    memset(&u, 0, sizeof(u));
    u.dir = DIR_EN_TO_ZH;
    u.volume = 60;
    return u;
}

// 菜单: 不用大面积实心黑, 且光标位置/调音量状态在画面上可区分
void test_menu_renders_distinct_states() {
    SettingsMenu m;
    m.open(menuDefaults());
    static uint8_t first[EPD_BUFFER_SIZE];
    s_ui.renderMenu(m);
    TEST_ASSERT_TRUE(blackRatio(0, 0, UI_WIDTH, UI_HEIGHT) < 0.15f);
    memcpy(first, s_buf, EPD_BUFFER_SIZE);

    m.handle(EVT_DIAL_DOWN);  // 光标移到音量
    s_canvas.clear(true);
    s_ui.renderMenu(m);
    TEST_ASSERT_TRUE(memcmp(first, s_buf, EPD_BUFFER_SIZE) != 0);

    m.open(menuDefaults());
    m.handle(EVT_DIAL_DOWN);
    s_canvas.clear(true);
    s_ui.renderMenu(m);
    memcpy(first, s_buf, EPD_BUFFER_SIZE);
    m.handle(EVT_DIAL_CONFIRM);  // 进入调音量: 线框加粗
    s_canvas.clear(true);
    s_ui.renderMenu(m);
    TEST_ASSERT_TRUE(memcmp(first, s_buf, EPD_BUFFER_SIZE) != 0);
}

// 音量条随音量变化
void test_menu_volume_bar_tracks_volume() {
    SettingsMenu m;
    UserSettings u = menuDefaults();
    u.volume = 20;
    m.open(u);
    s_ui.renderMenu(m);
    float low = blackRatio(0, 320, UI_WIDTH, 420);
    u.volume = 100;
    m.open(u);
    s_canvas.clear(true);
    s_ui.renderMenu(m);
    float high = blackRatio(0, 320, UI_WIDTH, 420);
    TEST_ASSERT_TRUE(high > low);
}

// 词库行显示当前词库标签, 切换后画面变化
void test_menu_deck_row_shows_label() {
    static const DeckEntry decks[] = {
        {"spbcn_r1", "spbcn_r1.json", "SPBCN R1"},
        {"spbcn_r3", "spbcn_r3.json", "SPBCN R3"},
    };
    SettingsMenu m;
    UserSettings u = menuDefaults();
    strcpy(u.deck_id, "spbcn_r1");
    m.open(u, decks, 2);
    s_ui.renderMenu(m);
    TEST_ASSERT_FALSE(rowsAreWhite(185, 225, UI_WIDTH / 2, UI_WIDTH - 44));  // 右侧有词库值
    static uint8_t before[EPD_BUFFER_SIZE];
    memcpy(before, s_buf, EPD_BUFFER_SIZE);
    m.handle(EVT_DIAL_CONFIRM);
    s_canvas.clear(true);
    s_ui.renderMenu(m);
    TEST_ASSERT_TRUE(memcmp(before, s_buf, EPD_BUFFER_SIZE) != 0);
}

// 中→英: 正面显示中文 (大字), 不显示英文单词与音标, 渲染结果与英→中不同
void test_front_zh_to_en_hides_english() {
    WordCard c = makeCard("speech");
    strcpy(c.zh, "演讲，言语");
    CardProgress p = {1, 0, false};
    static uint8_t en_front[EPD_BUFFER_SIZE];
    s_ui.setDirection(DIR_EN_TO_ZH);
    s_ui.renderFront(c, p, 0, 10);
    memcpy(en_front, s_buf, EPD_BUFFER_SIZE);

    s_canvas.clear(true);
    s_ui.setDirection(DIR_ZH_TO_EN);
    s_ui.renderFront(c, p, 0, 10);
    s_ui.setDirection(DIR_EN_TO_ZH);
    TEST_ASSERT_TRUE(memcmp(en_front, s_buf, EPD_BUFFER_SIZE) != 0);

    // 用同样位置单独画出英文单词, 与中→英正面的单词区域比较: 不应完全包含这些像素
    static uint8_t word_only[EPD_BUFFER_SIZE];
    memcpy(word_only, s_buf, EPD_BUFFER_SIZE);
    Canvas480x800 probe(word_only, 3);
    probe.clear(true);
    probe.drawStringCentered(230, c.en, Font48, true);
    int covered = 0, total = 0;
    for (int y = 220; y < 300; y++)
        for (int x = 0; x < UI_WIDTH; x++)
            if (probe.getPixel(x, y)) { total++; covered += s_canvas.getPixel(x, y); }
    TEST_ASSERT_TRUE(total > 0);
    TEST_ASSERT_TRUE_MESSAGE(covered < total / 2, "english headword visible on zh->en front");
}

// 关机页: 扇形叠放的单词卡 (后两张倾斜, 两侧露出卡边) + 关机文字; 整体不至于大面积发黑
void test_power_off_screen() {
    s_ui.renderPowerOff();
    TEST_ASSERT_TRUE(blackRatio(0, 0, UI_WIDTH, UI_HEIGHT) < 0.2f);
    // 最上层卡片 x = 90..389; 其左右两侧应露出倾斜卡片的边框
    TEST_ASSERT_TRUE_MESSAGE(blackRatio(60, 200, 90, 420) > 0.005f, "left tilted card edge");
    TEST_ASSERT_TRUE_MESSAGE(blackRatio(397, 200, 420, 420) > 0.005f, "right tilted card edge");
    // 最上层卡片内有文字, 下方有关机文字
    TEST_ASSERT_TRUE(blackRatio(110, 240, 370, 360) > 0.02f);
    TEST_ASSERT_TRUE(blackRatio(0, 480, UI_WIDTH, 620) > 0.01f);
}

// 错误页: 标题与说明都画出来, 且不残留卡片内容
void test_error_screen() {
    s_ui.renderError("未检测到 TF 卡", "请插入装有 spell 文件夹的 TF 卡");
    TEST_ASSERT_FALSE(rowsAreWhite(290, 360, 40, UI_WIDTH - 40));  // 标题
    TEST_ASSERT_FALSE(rowsAreWhite(405, 470, 40, UI_WIDTH - 40));  // 说明
    TEST_ASSERT_TRUE(rowsAreWhite(40, 280, 40, UI_WIDTH - 40));
}

// 状态栏词库标签随 setDeckLabel 变化
void test_deck_label_in_header() {
    WordCard c = makeCard("word");
    CardProgress p = {1, 0, false};
    s_ui.setDeckLabel("SPBCN R1");
    s_ui.renderFront(c, p, 0, 10);
    uint8_t before[EPD_BUFFER_SIZE];
    memcpy(before, s_buf, sizeof(before));
    s_canvas.clear(true);
    s_ui.setDeckLabel("SPBCN R3 LONG");
    s_ui.renderFront(c, p, 0, 10);
    TEST_ASSERT_TRUE(memcmp(before, s_buf, sizeof(before)) != 0);
    s_ui.setDeckLabel("SPBCN");
}

// 未对时: 底部提示换成对时提示, 画面与平时不同
void test_clock_warning_changes_footer() {
    WordCard c = makeCard("word");
    CardProgress p = {1, 0, false};
    s_ui.renderFront(c, p, 0, 10);
    static uint8_t normal[EPD_BUFFER_SIZE];
    memcpy(normal, s_buf, EPD_BUFFER_SIZE);
    s_canvas.clear(true);
    s_ui.setClockWarning(true);
    s_ui.renderFront(c, p, 0, 10);
    s_ui.setClockWarning(false);
    TEST_ASSERT_TRUE(memcmp(normal, s_buf, EPD_BUFFER_SIZE) != 0);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_back_long_text_stays_inside_card);
    RUN_TEST(test_front_long_word_fits_width);
    RUN_TEST(test_front_short_word_uses_large_font);
    RUN_TEST(test_front_longest_deck_word_fits_large);
    RUN_TEST(test_front_large_word_clear_of_ipa);
    RUN_TEST(test_front_word_ipa_grouped_apart_from_button);
    RUN_TEST(test_front_zh_headline_stays_font48);
    RUN_TEST(test_front_word_with_non_ascii_falls_back_to_font48);
    RUN_TEST(test_front_ipa_uses_large_font);
    RUN_TEST(test_front_longest_ipa_fits);
    RUN_TEST(test_unknown_battery_shows_empty_icon);
    RUN_TEST(test_charging_shows_bolt_in_battery_icon);
    RUN_TEST(test_buttons_are_outlined_not_solid);
    RUN_TEST(test_menu_renders_distinct_states);
    RUN_TEST(test_menu_volume_bar_tracks_volume);
    RUN_TEST(test_front_zh_to_en_hides_english);
    RUN_TEST(test_power_off_screen);
    RUN_TEST(test_clock_warning_changes_footer);
    RUN_TEST(test_menu_deck_row_shows_label);
    RUN_TEST(test_error_screen);
    RUN_TEST(test_deck_label_in_header);
    return UNITY_END();
}
