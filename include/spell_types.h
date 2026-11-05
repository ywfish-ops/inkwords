#ifndef SPELL_TYPES_H
#define SPELL_TYPES_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @file spell_types.h
 * @brief SPBCN 单词卡核心数据结构定义
 */

// Leitner 5盒复习间隔 (单位: 天)
// index 1..5 对应 盒1..盒5
static const uint8_t LEITNER_BOX_DAYS[6] = {0, 1, 2, 4, 7, 14};
#define LEITNER_BOX_COUNT 5

// 背卡方向
typedef enum {
    DIR_EN_TO_ZH = 0,   // 正面英文+音标 -> 背面中文+释义+例句
    DIR_ZH_TO_EN = 1    // 正面中文+听音 -> 背面英文拼写+音标+例句
} review_dir_t;

// 界面状态机
typedef enum {
    VIEW_CARD_FRONT = 0,    // 卡片正面 (待回忆/翻面)
    VIEW_CARD_BACK  = 1,    // 卡片背面 (已揭晓答案, 待评定 没记住/记住了)
    VIEW_MENU_MODAL = 2,    // 快捷设置/词库切换弹层
    VIEW_DONE_STATE = 3,    // 今日复习完成页
    VIEW_ERROR      = 4     // 错误页 (无 TF 卡 / 词库读取失败)
} ui_view_state_t;

// 按键动作事件
typedef enum {
    EVT_NONE = 0,
    EVT_DIAL_UP,            // 拨轮上拨 (GPIO 4: 正面看答案 / 背面没记住)
    EVT_DIAL_DOWN,          // 拨轮下拨 (GPIO 6: 记住了, 正反面都可)
    EVT_DIAL_CONFIRM,       // 拨轮中键按下 (GPIO 5: 翻面 / 翻回正面)
    EVT_BOOT_CLICK,         // BOOT 单击 (GPIO 0): 重播发音
    EVT_BOOT_DOUBLE_CLICK,  // BOOT 双击 (GPIO 0): 打开/关闭设置菜单
    EVT_BOOT_LONG_PRESS,    // BOOT 长按 (GPIO 0): 打开设置菜单
    EVT_PWR_LONG_PRESS      // 电源键长按 2 秒 (GPIO 1): 关机
} input_event_t;

// 单张单词卡数据结构 (常驻 PSRAM)
struct WordCard {
    char key[32];           // 唯一键, 如 "spbcn_r1-0"
    char en[48];            // 英文单词, 如 "speech"
    char ipa[48];           // 音标, 如 "/spi:tʃ/"
    char zh[96];            // 中文释义, 如 "演讲，言语"
    char ex[256];           // 完整例句, 如 "She has to give a speech at the convention."
    char cat[32];           // 易错点/考点分类, 如 "vowel_team"
    char def_en[256];       // 英文原版释义 (最大覆盖 256 字符)
    char ex_blank[256];     // 挖空例句, 如 "She has to give a ____ at the convention."
    char audio[48];         // 发音文件名 (相对 audio/ 目录), 如 "speech.mp3"
};

// manifest.json 中的一套词库 (设置菜单的词库列表)
#define MAX_DECKS 8
struct DeckEntry {
    char id[20];        // 如 "spbcn_r1"
    char file[48];      // 如 "spbcn_r1.json"
    char label[20];     // 状态栏/菜单短标签, 如 "SPBCN R1"
};

// 单张卡片的学习进度
struct CardProgress {
    uint8_t  box;           // 当前所在 Leitner 盒子 (1 ~ 5)
    int32_t  due_day;       // 到期学习日 (见 study_day.h, 以凌晨 4 点为界)
    bool     seen;          // 是否已学习过
};

// 用户全局设置: deck_id/cat_filter 来自 TF 卡 userdata/settings.json, 其余存 NVS
struct UserSettings {
    char         deck_id[20];   // 当前词库 ID: "spbcn_r1", "spbcn_r2", "spbcn_r3", "sample"
    char         cat_filter[20];// 当前分类过滤: "all" 或具体分类名
    review_dir_t dir;           // 背卡方向: DIR_EN_TO_ZH 或 DIR_ZH_TO_EN
    bool         auto_speak;    // 切卡时是否自动播放发音 MP3
    uint8_t      volume;        // 扬声器音量 (0 ~ 100)
    uint8_t      new_per_day;   // 每日新词上限, 0 为不限 (NEW_PER_DAY_UNLIMITED)
};

#endif // SPELL_TYPES_H
