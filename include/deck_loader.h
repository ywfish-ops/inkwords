#ifndef DECK_LOADER_H
#define DECK_LOADER_H

#include <stddef.h>
#include "spell_types.h"

/**
 * @file deck_loader.h
 * @brief 解析 TF 卡上的词库 / manifest / settings JSON (纯逻辑, 不读文件, 可在主机上测试)
 *        卡片数组在设备上分配于 PSRAM
 */

struct Deck {
    char      id[20];
    char      name[48];
    int       count;    // 有效卡片数
    int       skipped;  // 缺 en 或 key 被跳过的卡片数
    WordCard* cards;
};

// 解析一套词库 JSON。成功返回 true, out.cards 需用 deck_free 释放;
// 失败返回 false 并把原因写进 err (JSON 损坏 / 无 cards 数组 / 没有有效卡片 / 内存不足)
bool deck_parse(const char* json, size_t len, Deck& out, char* err, size_t err_len);

void deck_free(Deck& deck);

// 读出 manifest.json 中全部有效词库 (缺 id 或 file 的跳过), 最多 max 套; 返回套数, 损坏返回 0
int manifest_list(const char* json, size_t len, DeckEntry* out, int max);

// 在列表中找 id, 找不到返回 0 (第一套); n 为 0 时返回 -1
int deck_index(const DeckEntry* decks, int n, const char* id);

// 把 deck_id / cat_filter 写成 settings.json 文本; 返回长度, 缓冲不够返回 0
size_t selection_serialize(const UserSettings& settings, char* out, size_t out_len);

// 用 settings.json 覆盖 settings.deck_id / cat_filter; json 为空或损坏时保持原值
void selection_parse(const char* json, size_t len, UserSettings& settings);

// 按分类筛选卡片, 把卡片下标写进 out_idx (容量至少 deck.count), 返回张数;
// cat 为 "all"/空, 或筛出 0 张时返回全部
int deck_filter(const Deck& deck, const char* cat, int* out_idx);

// 从词库名取状态栏短标签: "SPBCN R1 单音节" -> "SPBCN R1" (取第一个非 ASCII 字符之前, 去尾空格)
void deck_short_label(const char* name, char* out, size_t out_len);

// 按 UTF-8 字符边界截断复制, 保证 dst 以 '\0' 结尾且不切断多字节字符
void copy_utf8(char* dst, size_t cap, const char* src);

#endif // DECK_LOADER_H
