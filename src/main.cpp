#include <Arduino.h>
#include <esp_log.h>
#include "board_config.h"
#include "spell_types.h"
#include "axp2101_power.h"
#include "audio_es8311.h"
#include "word_player.h"
#include "settings_menu.h"
#include "settings_store.h"
#include "power_policy.h"
#include <esp_sleep.h>
#include <driver/gpio.h>
#include "epd_3in97.h"
#include "input_dial.h"
#include "canvas_480x800.h"
#include "flashcard_ui.h"
#include "sdcard_bsp.h"
#include <soc/usb_serial_jtag_struct.h>
#include "deck_loader.h"
#include "serial_fs.h"
#include "review_input.h"
#include "rtc_pcf85063.h"
#include "leitner.h"
#include "device_log.h"
#include <esp_heap_caps.h>

static const char* TAG = "SPELL_MAIN";


// 状态与数据模型: 词库从 TF 卡加载到 PSRAM
static Deck s_deck;
static CardProgress* s_progress = nullptr;   // 按 s_deck.cards 下标, 加载词库时从全局进度表取
// 全局进度表: 按单词在各词库间共享, 连同当日状态 (学习日, 今日新词数) 存于 userdata/progress.json
#define PROGRESS_PATH SPELL_ROOT_DIR "/userdata/progress.json"
#define PROGRESS_MAX_WORDS 8192
static ProgressStore s_store = {nullptr, 0, 0, {0, 0}};
static StudyState& s_state = s_store.state;
static int* s_candidates = nullptr;          // 当前分类下的卡片下标
static int s_cand_count = 0;
static int* s_queue = nullptr;               // 本轮要过的卡片下标 (到期 + 新词, 或"再过一遍全部")
static int s_total = 0;                      // 本轮张数
static int s_current_index = 0;              // s_queue 中的位置
static int32_t s_today = 0;                  // 当前学习日 (RTC 无效时沿用上次的)
static bool s_rtc_valid = false;
static bool s_day_pending = false;           // 已换天, 评完当前这张再换成新一天的队列
static char s_error_title[32];
static char s_error_detail[160];
static ui_view_state_t s_view_state = VIEW_CARD_FRONT;
// 连续局刷达到该次数后执行一次全刷消除残影
#define FULL_REFRESH_EVERY 30
static uint32_t s_partial_count = 0;

// 用户设置与设置菜单 (长按 BOOT 打开, 退出时保存到 NVS)
static UserSettings s_settings;
static SettingsMenu s_menu;
static ui_view_state_t s_menu_return_state = VIEW_CARD_FRONT;

// 无操作超过该时长后墨水屏进入深度休眠 (画面保持, 下次刷新自动唤醒)
#define EPD_IDLE_SLEEP_MS 60000
static uint32_t s_last_activity_ms = 0;

// 定期检查电量与充电状态, 有变化才刷新屏幕
#define POWER_POLL_MS 60000
static uint32_t s_last_power_poll_ms = 0;
static bool s_usb_powered = false;   // 随电量轮询更新, 决定是否允许浅睡眠
static int  s_shown_battery = -1;
static bool s_shown_charging = false;

// 硬件与画布实例
static Epd3in97& s_epd = Epd3in97::getInstance();
static Axp2101Power& s_pmic = Axp2101Power::getInstance();
static InputDial& s_dial = InputDial::getInstance();
static AudioEs8311& s_audio = AudioEs8311::getInstance();
static WordPlayer& s_player = WordPlayer::getInstance();
static Canvas480x800* s_canvas = nullptr;
static FlashcardUI* s_ui = nullptr;

static const WordCard& currentCard() { return s_deck.cards[s_queue[s_current_index]]; }
static CardProgress& currentProgress() { return s_progress[s_queue[s_current_index]]; }

static void speakCurrentWord() {
    if (s_total == 0) return;
    if (!s_player.play(currentCard().audio)) {
        Serial.printf("  无发音音频: %s\n", currentCard().en);
    }
}

// 切到新卡正面时按设置自动发音 (先发起播放, 与刷屏并行)
static void autoSpeakNewCard() {
    if (s_settings.auto_speak && s_view_state == VIEW_CARD_FRONT) {
        speakCurrentWord();
    }
}

static void syncBatteryToUi() {
    s_shown_battery = s_pmic.getBatteryPercent();
    s_shown_charging = s_pmic.isCharging();
    s_ui->setBattery(s_shown_battery, s_shown_charging);
}

// 明天能学的新词数: 受每日额度限制
static int newWordsFor(int unseen) {
    int limit = s_settings.new_per_day;
    return (limit == NEW_PER_DAY_UNLIMITED || unseen < limit) ? unseen : limit;
}

// 重新绘制并刷新屏幕
static void updateDisplay(bool force_full_refresh = false) {
    if (!s_ui) return;

    // 同步电池电量、充电状态与背卡方向
    syncBatteryToUi();
    s_ui->setDirection(s_settings.dir);
    s_ui->setClockWarning(!s_rtc_valid && s_view_state != VIEW_ERROR);

    // 统计各盒子卡片数
    int box_counts[6] = {0};
    for (int i = 0; i < s_cand_count; i++) {
        uint8_t b = s_progress[s_candidates[i]].box;
        if (b >= 1 && b <= 5) box_counts[b]++;
    }

    // 防御性边界约束
    if (s_current_index < 0) s_current_index = 0;
    if (s_current_index >= s_total) s_current_index = s_total - 1;

    if (s_view_state == VIEW_ERROR) {
        s_ui->renderError(s_error_title, s_error_detail);
    } else if (s_view_state == VIEW_CARD_FRONT) {
        s_ui->renderFront(currentCard(), currentProgress(), s_current_index, s_total);
    } else if (s_view_state == VIEW_CARD_BACK) {
        s_ui->renderBack(currentCard(), currentProgress(), s_current_index, s_total);
    } else if (s_view_state == VIEW_DONE_STATE) {
        s_ui->renderDone(s_cand_count, box_counts, study_count_due(s_progress, s_candidates, s_cand_count, s_today + 1),
                         newWordsFor(study_count_unseen(s_progress, s_candidates, s_cand_count)));
    } else if (s_view_state == VIEW_MENU_MODAL) {
        s_ui->renderMenu(s_menu);
    }

    // 周期性全刷消除墨水屏残影 (连续局刷 FULL_REFRESH_EVERY 次后全刷一次，任何全刷都重新计数)
    if (force_full_refresh || s_partial_count >= FULL_REFRESH_EVERY) {
        Serial.printf("[UI] 执行全局全刷消除残影 (此前连续局刷 %lu 次)...\n", s_partial_count);
        s_epd.displayFull();
        s_partial_count = 0;
    } else {
        s_partial_count++;
        Serial.printf("[UI] 执行真正无闪烁局部刷新 (#%lu/%d)...\n", s_partial_count, FULL_REFRESH_EVERY);
        s_epd.displayPart();
    }
}

static void setError(const char* title, const char* detail) {
    copy_utf8(s_error_title, sizeof(s_error_title), title);
    copy_utf8(s_error_detail, sizeof(s_error_detail), detail);
    LOGE("DECK", "%s - %s", title, detail);
}

// manifest.json 中的全部词库 (设置菜单的切换列表)
static DeckEntry s_decks[MAX_DECKS];
static int s_deck_count = 0;

// 当前学习日; RTC 无效 (未对时/电池耗尽) 时沿用进度文件里上次的学习日, 日期不前进
static int32_t readToday() {
    LocalDateTime dt;
    s_rtc_valid = rtc_read(dt);
    return s_rtc_valid ? study_day(datetime_to_epoch(dt)) : s_state.day;
}

// 重建本轮队列并回到第 1 张; review_all 为"再过一遍全部词"
static void rebuildQueue(bool review_all) {
    uint32_t seed = esp_random();
    s_total = review_all ? study_build_all(s_candidates, s_cand_count, seed, s_queue)
                         : study_build_queue(s_progress, s_candidates, s_cand_count, s_today,
                                             s_settings.new_per_day, s_state, seed, s_queue);
    s_current_index = 0;
}

// 当前卡评分后记入全局进度表并整份写回 TF 卡, 返回耗时 ms
static uint32_t saveProgress() {
    uint32_t t0 = millis();
    if (!store_put(s_store, currentCard().en, currentProgress())) {
        LOGE("DECK", "进度表已满 (%d 词), %s 未记录", s_store.capacity, currentCard().en);
    }
    size_t cap = store_json_capacity(s_store);
    char* buf = (char*)heap_caps_malloc(cap, MALLOC_CAP_SPIRAM);
    if (!buf) {
        LOGE("DECK", "内存不足, 进度未写入");
        return millis() - t0;
    }
    size_t n = store_serialize(s_store, buf, cap);
    if (!n || !sdcard_write_file(PROGRESS_PATH, buf, n)) LOGE("DECK", "%s 写入失败", PROGRESS_PATH);
    free(buf);
    return millis() - t0;
}

// 开机读入全局进度表; 缺失则从空表开始, 损坏则记日志后从空表开始
static bool loadProgressStore() {
    s_store.items = (WordProgress*)heap_caps_calloc(PROGRESS_MAX_WORDS, sizeof(WordProgress), MALLOC_CAP_SPIRAM);
    if (!s_store.items) return false;
    s_store.capacity = PROGRESS_MAX_WORDS;
    size_t len = 0;
    char* buf = sdcard_read_file(PROGRESS_PATH, &len);
    if (!store_parse(buf, len, s_store) && buf) LOGE("DECK", "%s 损坏, 进度从头开始", PROGRESS_PATH);
    free(buf);
    LOGI("DECK", "进度表: %d 个词, 学习日 %ld, 今日新词 %d", s_store.count, (long)s_state.day, s_state.new_today);
    return true;
}

// 加载 s_decks[idx] 到新 Deck 并读取它的进度, 成功后替换当前词库。
// 失败时当前词库保持不变, 错误写入 title/detail
static bool loadDeckAt(int idx, const char** title, char* detail, size_t detail_len) {
    const DeckEntry& e = s_decks[idx];
    *title = "词库读取失败";
    char path[96], err[96];
    snprintf(path, sizeof(path), SPELL_ROOT_DIR "/decks/%s", e.file);
    size_t len = 0;
    char* buf = sdcard_read_file(path, &len);
    if (!buf) {
        snprintf(detail, detail_len, "decks/%s 缺失或无法读取", e.file);
        return false;
    }
    Deck deck;
    bool ok = deck_parse(buf, len, deck, err, sizeof(err));
    free(buf);
    if (!ok) {
        snprintf(detail, detail_len, "decks/%s: %s", e.file, err);
        return false;
    }
    int* cand = (int*)heap_caps_malloc(deck.count * sizeof(int), MALLOC_CAP_SPIRAM);
    int* queue = (int*)heap_caps_malloc(deck.count * sizeof(int), MALLOC_CAP_SPIRAM);
    CardProgress* progress = (CardProgress*)heap_caps_calloc(deck.count, sizeof(CardProgress), MALLOC_CAP_SPIRAM);
    if (!cand || !queue || !progress) {
        free(cand);
        free(queue);
        free(progress);
        deck_free(deck);
        snprintf(detail, detail_len, "内存不足");
        return false;
    }
    if (strcmp(deck.id, e.id) != 0) {
        LOGW("DECK", "%s 内的 id 为 \"%s\", 与 manifest 的 \"%s\" 不一致", e.file, deck.id, e.id);
    }
    store_load_deck(s_store, deck, progress);

    deck_free(s_deck);
    free(s_candidates);
    free(s_queue);
    free(s_progress);
    s_deck = deck;
    s_candidates = cand;
    s_queue = queue;
    s_progress = progress;
    int filtered = deck_filter(s_deck, s_settings.cat_filter, s_candidates);
    if (filtered == s_deck.count && strcmp(s_settings.cat_filter, "all") != 0) {
        LOGW("DECK", "分类 %s 无卡片, 显示全部", s_settings.cat_filter);
    }
    s_cand_count = study_unique_words(s_deck, s_candidates, filtered);  // 重复的词只留第一张
    copy_utf8(s_settings.deck_id, sizeof(s_settings.deck_id), e.id);
    s_ui->setDeckLabel(e.label);
    s_today = readToday();
    study_roll_day(s_state, s_today);
    s_day_pending = false;
    rebuildQueue(false);
    LOGI("DECK", "已加载 %s (%s): %d 张, 跳过 %d 张, 分类 %s -> %d 个词; 学习日 %ld, 今日新词 %d, 本轮 %d 张; PSRAM 剩余 %u KB",
                  e.id, s_deck.name, s_deck.count, s_deck.skipped, s_settings.cat_filter, s_cand_count,
                  (long)s_today, s_state.new_today, s_total,
                  (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));
    return true;
}

// 开机: 读 settings.json 选词库 -> manifest.json 列出词库 -> 加载。失败时写入错误页信息
static bool loadDeckAtBoot() {
    if (!sdcard_is_mounted()) {
        setError("未检测到 TF 卡", "请插入装有 spell 文件夹的 TF 卡");
        return false;
    }

    size_t len = 0;
    char* buf = sdcard_read_file(SPELL_ROOT_DIR "/userdata/settings.json", &len);
    if (!buf) LOGW("DECK", "userdata/settings.json 缺失, 使用默认词库");
    selection_parse(buf, len, s_settings);
    free(buf);

    buf = sdcard_read_file(SPELL_ROOT_DIR "/decks/manifest.json", &len);
    s_deck_count = buf ? manifest_list(buf, len, s_decks, MAX_DECKS) : 0;
    free(buf);
    if (s_deck_count == 0) {
        setError("词库读取失败", "decks/manifest.json 缺失或损坏");
        return false;
    }
    if (!loadProgressStore()) {
        setError("内存不足", "无法分配进度表");
        return false;
    }
    int idx = deck_index(s_decks, s_deck_count, s_settings.deck_id);
    if (s_settings.deck_id[0] && strcmp(s_decks[idx].id, s_settings.deck_id) != 0) {
        LOGW("DECK", "词库 %s 不在 manifest 中, 改用 %s", s_settings.deck_id, s_decks[idx].id);
    }

    const char* title;
    char detail[160];
    if (!loadDeckAt(idx, &title, detail, sizeof(detail))) {
        setError(title, detail);
        return false;
    }
    return true;
}

// 设置菜单里换了词库: 加载新词库并写回 settings.json; 失败则保留原词库
static bool switchDeck(const char* id) {
    int idx = deck_index(s_decks, s_deck_count, id);
    if (idx < 0) return false;
    const char* title;
    char detail[160];
    char old_cat[sizeof(s_settings.cat_filter)];
    memcpy(old_cat, s_settings.cat_filter, sizeof(old_cat));
    copy_utf8(s_settings.cat_filter, sizeof(s_settings.cat_filter), "all");  // 各词库分类不同
    if (!loadDeckAt(idx, &title, detail, sizeof(detail))) {
        memcpy(s_settings.cat_filter, old_cat, sizeof(old_cat));
        LOGE("DECK", "切换到 %s 失败, 保留原词库: %s", id, detail);
        return false;
    }
    char json[96];
    size_t n = selection_serialize(s_settings, json, sizeof(json));
    if (!n || !sdcard_write_file(SPELL_ROOT_DIR "/userdata/settings.json", json, n)) {
        LOGE("DECK", "settings.json 写入失败, 下次开机将回到原词库");
    }
    return true;
}

void setup() {
    Serial.setRxBufferSize(16384);  // tools/sdsync.py 传文件
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n=======================================================");
    Serial.println("  墨词 InkWords (480x800 竖屏墨水屏) 启动");
    Serial.println("=======================================================");

    // 1. 初始化 AXP2101 电源芯片 (开启墨水屏与外设 3.3V 供电)
    Serial.println("[1/4] 初始化 AXP2101 电源...");
    bool pmic_ok = s_pmic.begin(I2C_MASTER_SDA_PIN, I2C_MASTER_SCL_PIN);
    if (!pmic_ok) {
        Serial.println("警告: AXP2101 初始化失败，尝试继续...");
    } else {
        Serial.printf("AXP2101 启动成功! 电池电量: %d%%\n", s_pmic.getBatteryPercent());
        s_pmic.dumpStatus();
    }

    bool rtc_found = rtc_begin();

    // 1a. 挂载 TF 卡 (依赖 AXP2101 已打开外设供电)
    if (sdcard_mount()) {
        sdcard_list_dir(SPELL_ROOT_DIR "/decks");
    }
    log_begin();  // 持久日志 (TF 卡 spell/logs/device.log), 记录开机原因
    {
        LocalDateTime now;
        bool rtc_ok = rtc_read(now);
        LOGI("BOOT", "电池 %d%%, USB %s, TF 卡 %s (%u-bit, %lu MB), RTC %s", s_pmic.getBatteryPercent(),
             s_pmic.isVbusPresent() ? "插着" : "未插", sdcard_is_mounted() ? "已挂载" : "未挂载",
             sdcard_bus_width(), (unsigned long)sdcard_size_mb(), rtc_ok ? "正常" : "无效");
        if (!pmic_ok) LOGE("BOOT", "AXP2101 电源芯片初始化失败");
        if (!rtc_found) LOGE("BOOT", "PCF85063 RTC 未检测到");
    }

    // 1b. 初始化 ES8311 音频 (与 AXP2101 共用 I2C 总线)
    settings_load(s_settings);
    Serial.printf("设置: 音量 %d%%, 方向 %s, 自动发音 %s\n", s_settings.volume,
                  s_settings.dir == DIR_EN_TO_ZH ? "英->中" : "中->英", s_settings.auto_speak ? "开" : "关");
    if (s_audio.begin(AUDIO_SAMPLE_RATE) && s_player.begin(s_audio)) {
        s_audio.setVolume(s_settings.volume);
        Serial.println("ES8311 音频与发音任务初始化成功");
    } else {
        LOGE("AUDIO", "ES8311 音频初始化失败, 发音不可用");
    }

    // 2. 初始化输入按键与三向拨轮
    Serial.println("[2/4] 初始化三向拨轮与 BOOT 键...");
    s_dial.begin();

    // 3. 初始化 SSD1677 墨水屏
    Serial.println("[3/4] 初始化 3.97寸 800x480 SSD1677 墨水屏底层 SPI...");
    if (!s_epd.begin()) {
        LOGE("EPD", "墨水屏显存分配或 SPI 初始化失败");
        while (1) delay(1000);
    }

    // 4. 构建 480x800 竖屏画布与 UI 引擎
    Serial.println("[4/4] 构建 480x800 竖屏画布...");
    // 旋转度数: 3 (270° 旋转，相对原 90° 旋转 180 度)
    s_canvas = new Canvas480x800(s_epd.getBuffer(), 3);
    s_ui = new FlashcardUI(*s_canvas);
    s_ui->setDirection(s_settings.dir);

    // 5. 从 TF 卡加载词库与进度, 排出今天的队列 (没有要过的卡就直接显示完成页)
    if (!loadDeckAtBoot()) s_view_state = VIEW_ERROR;
    else if (s_total == 0) s_view_state = VIEW_DONE_STATE;

    Serial.println("\n[UI] 绘制第一张卡片正面并执行全刷...");
    s_epd.clearBuffer(0xFF);
    syncBatteryToUi();
    s_usb_powered = s_pmic.isVbusPresent();
    updateDisplay(true);
    s_last_activity_ms = millis();

    Serial.println("=======================================================");
    Serial.println("  交互指南:");
    Serial.println("   - 【拨轮中键按压】: 正面翻到背面 / 背面翻回正面");
    Serial.println("   - 【拨轮向上拨动】: 正面 看答案; 背面 没记住 (回盒 1)");
    Serial.println("   - 【拨轮向下拨动】: 记住了 (升一盒), 正面也可直接评");
    Serial.println("   - 【短按 BOOT 键】: 触发发音");
    Serial.println("   - 【长按 BOOT 键】: 打开设置 (音量 / 背卡方向 / 切卡自动发音), 退出时全刷消除残影");
    Serial.println("   - 【长按电源键 2 秒】: 关机");
    Serial.println("=======================================================\n");
}

// 评分后切到下一张, 最后一张之后进入完成页; 期间换了天就换成新一天的队列
static void advanceCard() {
    s_current_index++;
    if (s_day_pending) {
        s_day_pending = false;
        rebuildQueue(false);
        LOGI("DAY", "换成新一天的队列: 学习日 %ld, 本轮 %d 张", (long)s_today, s_total);
    }
    s_view_state = s_current_index >= s_total ? VIEW_DONE_STATE : VIEW_CARD_FRONT;
    autoSpeakNewCard();
    updateDisplay(false);
}

static void handleReviewAction(review_action_t action) {
    switch (action) {
        case REVIEW_SHOW_BACK:
            s_view_state = VIEW_CARD_BACK;
            Serial.printf("  翻面: [%s] -> 查看答案\n", currentCard().en);
            updateDisplay(false);
            break;
        case REVIEW_SHOW_FRONT:
            s_view_state = VIEW_CARD_FRONT;
            updateDisplay(false);
            break;
        case REVIEW_REMEMBERED:
        case REVIEW_FORGOT: {
            bool remembered = action == REVIEW_REMEMBERED;
            study_grade(currentProgress(), remembered, s_today, s_state);
            uint32_t save_ms = saveProgress();
            LOGI("REV", "%s %s -> 盒 %d, 学习日 %ld 到期 (%s, 写进度 %lu ms)", remembered ? "记住了" : "没记住",
                          currentCard().en, currentProgress().box, (long)currentProgress().due_day,
                          s_view_state == VIEW_CARD_FRONT ? "正面直接评" : "看过答案", (unsigned long)save_ms);
            advanceCard();
            break;
        }
        case REVIEW_RESTART:
            rebuildQueue(true);  // 再过一遍全部词, 照常评分
            s_view_state = s_total ? VIEW_CARD_FRONT : VIEW_DONE_STATE;
            autoSpeakNewCard();
            updateDisplay(true);
            break;
        default:
            break;
    }
}

// 退出菜单: 音量/方向/自动发音存 NVS; 换了词库则加载新词库并从第 1 张正面开始
static void applyMenuSettings() {
    char old_deck[sizeof(s_settings.deck_id)];
    memcpy(old_deck, s_settings.deck_id, sizeof(old_deck));
    uint8_t old_new_per_day = s_settings.new_per_day;
    s_settings = s_menu.settings();
    settings_save(s_settings);
    s_audio.setVolume(s_settings.volume);
    LOGI("MENU", "已保存: 音量 %d%%, 方向 %d, 自动发音 %d, 每日新词 %d", s_settings.volume, s_settings.dir,
         s_settings.auto_speak, s_settings.new_per_day);

    s_view_state = s_menu_return_state;
    if (s_settings.new_per_day != old_new_per_day && strcmp(old_deck, s_settings.deck_id) == 0) {
        // 改了每日新词: 按新额度重排今天的队列 (今天已学的新词不退回)
        rebuildQueue(false);
        s_view_state = s_total ? VIEW_CARD_FRONT : VIEW_DONE_STATE;
        LOGI("DECK", "每日新词 %d, 本轮 %d 张", s_settings.new_per_day, s_total);
    }
    if (strcmp(old_deck, s_settings.deck_id) != 0) {
        char wanted[sizeof(s_settings.deck_id)];
        memcpy(wanted, s_settings.deck_id, sizeof(wanted));
        memcpy(s_settings.deck_id, old_deck, sizeof(old_deck));
        if (switchDeck(wanted)) s_view_state = s_total ? VIEW_CARD_FRONT : VIEW_DONE_STATE;
    }
}

static void checkDayChange();

static void handleMenuEvent(input_event_t evt) {
    if (evt == EVT_BOOT_CLICK) {
        speakCurrentWord();
        return;
    }
    switch (s_menu.handle(evt)) {
        case MENU_ACT_REDRAW:
            updateDisplay(false);
            break;
        case MENU_ACT_VOLUME_CHANGED:
            // 先用新音量试听, 再刷新菜单 (播放在后台任务, 与刷屏并行)
            s_audio.setVolume(s_menu.settings().volume);
            speakCurrentWord();
            updateDisplay(false);
            break;
        case MENU_ACT_EXIT:
            applyMenuSettings();
            updateDisplay(true);  // 退出菜单时全刷, 兼作手动清除残影
            break;
        case MENU_ACT_TIME_SET: {
            const LocalDateTime& t = s_menu.time();
            if (rtc_write(t)) {
                LOGI("RTC", "手动设时间 %04d-%02d-%02d %02d:%02d", t.year, t.month, t.day, t.hour, t.minute);
            } else {
                LOGE("RTC", "手动设时间写入失败");
            }
            checkDayChange();  // 跨天则换队列; 同时刷新 RTC 有效标记 (底部对时提示)
            updateDisplay(false);
            break;
        }
        default:
            break;
    }
}

// 电源键长按关机: 全刷关机页 (断电后画面保留) -> 屏幕休眠 -> AXP2101 软关机
static void powerOffDevice() {
    if (s_view_state == VIEW_MENU_MODAL) applyMenuSettings();
    uint32_t t0 = millis();
    s_ui->renderPowerOff();
    uint32_t render_ms = millis() - t0;
    s_epd.displayFull();
    s_epd.sleep();
    Serial.flush();
    LOGI("PWR", "长按电源键关机 (关机页绘制 %lu ms)", (unsigned long)render_ms);
    s_pmic.powerOff();

    // 仍在运行说明未断电 (插着 USB 等外部供电时可能如此): 恢复当前页面继续使用
    delay(3000);
    LOGW("PWR", "软关机未生效 (可能有外部供电), 恢复显示");
    updateDisplay(true);
}

// ---------------------------------------------------------------------------
// MCU 浅睡眠: 屏幕已休眠且用电池时进入; 拨轮/BOOT 按下 (低电平) 或定时器唤醒
// RAM/PSRAM 保持, 醒来后从原处继续 (停在同一张卡)
// ---------------------------------------------------------------------------
static const int WAKE_PINS[] = {DIAL_UP_PIN, DIAL_CONFIRM_PIN, DIAL_DOWN_PIN, BTN_BOOT_PIN};

static bool anyButtonPressed() {
    for (int pin : WAKE_PINS) {
        if (digitalRead(pin) == LOW) return true;
    }
    return digitalRead(BTN_PWR_PIN) == HIGH;
}

// 最近一次浅睡眠的结果 (醒来瞬间 USB 串口可能尚未重连, 日志会丢, 用 'i' 命令查询)
static uint32_t s_sleep_count = 0;
static uint32_t s_last_sleep_ms = 0;
static esp_sleep_wakeup_cause_t s_last_wake_cause = ESP_SLEEP_WAKEUP_UNDEFINED;

static const char* wakeCauseName(esp_sleep_wakeup_cause_t cause) {
    switch (cause) {
        case ESP_SLEEP_WAKEUP_GPIO:  return "按键";
        case ESP_SLEEP_WAKEUP_TIMER: return "定时器";
        default:                     return "其他";
    }
}

// 每分钟随电量轮询检查学习日 (凌晨 4 点换天, 或刚对过时):
// 停在完成页就直接开始新一天; 正在背卡则评完当前这张再换队列
static void checkDayChange() {
    if (s_view_state == VIEW_ERROR) return;
    int32_t today = readToday();
    if (today == s_today) return;
    LOGI("DAY", "学习日 %ld -> %ld (%s)", (long)s_today, (long)today,
         s_view_state == VIEW_DONE_STATE ? "在完成页, 直接开始" : "评完当前这张再换");
    s_today = today;
    study_roll_day(s_state, today);
    if (s_view_state == VIEW_DONE_STATE) {
        rebuildQueue(false);
        if (s_total) {
            s_view_state = VIEW_CARD_FRONT;
            autoSpeakNewCard();
            updateDisplay(true);
        }
    } else {
        s_day_pending = true;
    }
}

// 实测: 用电池浅睡眠过之后再插 USB, 电脑不会识别出 USB 串口 (插拔也不行, 只能重启)。
// 检测到刚插上 USB 时拉低再放开 D+ 上拉, 让电脑重新枚举
static void reattachUsb() {
    USB_SERIAL_JTAG.conf0.pad_pull_override = 1;
    USB_SERIAL_JTAG.conf0.dp_pullup = 0;
    delay(20);
    USB_SERIAL_JTAG.conf0.dp_pullup = 1;
    USB_SERIAL_JTAG.conf0.pad_pull_override = 0;
    LOGI("USB", "插上 USB, 重新连接 USB 串口");
}

static void enterLightSleep(uint32_t timer_ms) {
    for (int pin : WAKE_PINS) gpio_wakeup_enable((gpio_num_t)pin, GPIO_INTR_LOW_LEVEL);
    gpio_wakeup_enable((gpio_num_t)BTN_PWR_PIN, GPIO_INTR_HIGH_LEVEL);
    esp_sleep_enable_gpio_wakeup();
    esp_sleep_enable_timer_wakeup((uint64_t)timer_ms * 1000ULL);
    Serial.println("[SLEEP] MCU 进入浅睡眠");
    Serial.flush();

    uint32_t t0 = millis();
    esp_light_sleep_start();
    s_last_wake_cause = esp_sleep_get_wakeup_cause();
    s_last_sleep_ms = millis() - t0;
    s_sleep_count++;

    for (int pin : WAKE_PINS) gpio_wakeup_disable((gpio_num_t)pin);
    gpio_wakeup_disable((gpio_num_t)BTN_PWR_PIN);
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    Serial.printf("[SLEEP] 唤醒: %s, 睡了 %lu ms\n", wakeCauseName(s_last_wake_cause),
                  (unsigned long)s_last_sleep_ms);
}

static input_event_t s_serial_evt = EVT_NONE;  // 串口模拟的按键

void loop() {
    // 串口调试命令: 'p' 电源寄存器; 't' 1kHz 测试音; 'w' 播放当前单词; 's' 浅睡 5 秒; 'i' 睡眠统计; 'd' TF 卡状态; 'U/C/D/B/L' 模拟按键; '#...' TF 卡文件传输
    if (Serial.available()) {
        int cmd = Serial.read();
        // 'U' 'C' 'D' 'B' 'L' 模拟拨轮上拨/中键/下拨/BOOT 单击/BOOT 长按 (上板自动测试用)
        if (cmd == 'U') s_serial_evt = EVT_DIAL_UP;
        if (cmd == 'C') s_serial_evt = EVT_DIAL_CONFIRM;
        if (cmd == 'D') s_serial_evt = EVT_DIAL_DOWN;
        if (cmd == 'B') s_serial_evt = EVT_BOOT_CLICK;
        if (cmd == 'L') s_serial_evt = EVT_BOOT_LONG_PRESS;
        if (cmd == '#') serial_fs_handle_command();  // TF 卡文件传输, 见 serial_fs.h
        if (cmd == 'p') s_pmic.dumpStatus();
        if (cmd == 't') s_audio.playTone(1000, 500);
        if (cmd == 'w') speakCurrentWord();
        if (cmd == 's') enterLightSleep(5000);  // 调试: 强制浅睡眠 5 秒 (插 USB 时也执行)
        if (cmd == 'g') {  // 调试: 只在画布上画关机页并计时, 不关机, 然后全刷恢复当前页面
            uint32_t t0 = millis();
            s_ui->renderPowerOff();
            Serial.printf("[UI] 关机页绘制 %lu ms\n", (unsigned long)(millis() - t0));
            updateDisplay(true);
        }
        if (cmd == 'd') {
            Serial.printf("[SD] %s, %u-bit, %lu MB\n", sdcard_is_mounted() ? "已挂载" : "未挂载",
                          sdcard_bus_width(), (unsigned long)sdcard_size_mb());
            sdcard_list_dir(SPELL_ROOT_DIR);
            sdcard_list_dir(SPELL_ROOT_DIR "/decks");
            LocalDateTime now;
            if (rtc_read(now)) {
                Serial.printf("[RTC] %04d-%02d-%02d %02d:%02d:%02d, 学习日 %ld\n", now.year, now.month, now.day,
                              now.hour, now.minute, now.second, (long)study_day(datetime_to_epoch(now)));
            } else {
                Serial.println("[RTC] 时间无效 (未对时)");
            }
            size_t sl = 0;
            char* sj = sdcard_read_file(SPELL_ROOT_DIR "/userdata/settings.json", &sl);
            Serial.printf("[SD] settings.json: %s\n", sj ? sj : "(无)");
            free(sj);
            if (s_view_state == VIEW_ERROR) {
                Serial.printf("[DECK] 错误: %s - %s\n", s_error_title, s_error_detail);
            } else {
                Serial.printf("[DECK] %s (%s): %d 张, 分类 %s -> %d 张; 学习日 %ld%s, 今日新词 %d/%d, 本轮第 %d/%d 张 %s%s\n",
                              s_deck.id, s_deck.name, s_deck.count, s_settings.cat_filter, s_cand_count,
                              (long)s_today, s_rtc_valid ? "" : " (RTC 无效)", s_state.new_today,
                              s_settings.new_per_day, s_current_index + 1, s_total,
                              s_total && s_current_index < s_total ? currentCard().en : "-",
                              s_day_pending ? ", 待换天" : "");
            }
        }
        if (cmd == 'i') {
            Serial.printf("[SLEEP] 共 %lu 次, 最近一次: %s 唤醒, 睡了 %lu ms\n", (unsigned long)s_sleep_count,
                          wakeCauseName(s_last_wake_cause), (unsigned long)s_last_sleep_ms);
        }
    }

    input_event_t evt = s_dial.poll();
    if (s_serial_evt != EVT_NONE) {
        evt = s_serial_evt;
        s_serial_evt = EVT_NONE;
    }

    if (evt != EVT_NONE) {
        s_last_activity_ms = millis();
        if (evt == EVT_PWR_LONG_PRESS) {
            powerOffDevice();
        } else if (s_view_state == VIEW_ERROR) {
            // 错误页只响应电源键
        } else if (s_view_state == VIEW_MENU_MODAL) {
            handleMenuEvent(evt);
        } else switch (evt) {
            case EVT_DIAL_UP:
            case EVT_DIAL_CONFIRM:
            case EVT_DIAL_DOWN:
                handleReviewAction(review_action(s_view_state, evt));
                break;

            case EVT_BOOT_CLICK:
                Serial.printf("[EVENT] 短按 BOOT 键 -> 发音: %s (%s)\n",
                              currentCard().en, currentCard().ipa);
                speakCurrentWord();
                break;

            case EVT_BOOT_LONG_PRESS: {
                Serial.println("[EVENT] 长按 BOOT 键 -> 打开设置");
                LocalDateTime now;
                bool clock_ok = rtc_read(now);
                s_menu.open(s_settings, s_decks, s_deck_count, clock_ok ? &now : nullptr);
                s_menu_return_state = s_view_state;
                s_view_state = VIEW_MENU_MODAL;
                updateDisplay(false);
                break;
            }

            default:
                break;
        }
    }

    if (millis() - s_last_power_poll_ms >= POWER_POLL_MS) {
        s_last_power_poll_ms = millis();
        bool was_usb = s_usb_powered;
        s_usb_powered = s_pmic.isVbusPresent();
        if (s_usb_powered && !was_usb) reattachUsb();
        checkDayChange();
        int pct = s_pmic.getBatteryPercent();
        bool charging = s_pmic.isCharging();
        if (pct != s_shown_battery || charging != s_shown_charging) {
            LOGI("PWR", "电量 %d%% -> %d%%, 充电 %d -> %d", s_shown_battery, pct, s_shown_charging, charging);
            updateDisplay(false);
        }
    }

    if (!s_epd.isAsleep() && millis() - s_last_activity_ms > EPD_IDLE_SLEEP_MS) {
        s_epd.sleep();
    }

    LightSleepInputs sleep_in = {s_epd.isAsleep(), s_usb_powered, anyButtonPressed(), s_player.isPlaying()};
    if (should_light_sleep(sleep_in)) {
        // 定时醒来一次, 让电量轮询照常执行 (状态变化时刷新状态栏)
        enterLightSleep(POWER_POLL_MS);
        return;
    }

    delay(2);
}
