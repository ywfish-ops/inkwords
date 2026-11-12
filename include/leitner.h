#ifndef LEITNER_H
#define LEITNER_H

#include <stddef.h>
#include <stdint.h>
#include "deck_loader.h"

/**
 * @file leitner.h
 * @brief Leitner 5 盒调度与进度文件 (纯逻辑, 可在主机上测试), 规则:
 *        记住了升一盒 (最高盒 5), 没记住回盒 1; 下次到期 = 今天 + LEITNER_BOX_DAYS[盒]。
 *
 * 进度按单词 (去首尾空格、转小写) 在所有词库间共享, 存于 userdata/progress.json, 只记学过的词:
 *   {"day":20746,"new_today":3,"words":{"apple":{"b":2,"d":20748}, ...}}
 * 学习日与当日新词数也是全局的, 每日新词上限对所有词库一起算。
 */

// 每日新词上限: 不限
#define NEW_PER_DAY_UNLIMITED 0

// 当日状态 (全局)
struct StudyState {
    int32_t day;        // 最近一次使用的学习日
    int     new_today;  // 该学习日已学的新词数 (第一次评分才算)
};

// 单词表里一个词的进度, 按 word 排序
struct WordProgress {
    char    word[48];   // 去首尾空格、转小写
    uint8_t box;
    int32_t due_day;
};

// 全局进度表: items 由调用方分配 capacity 个
struct ProgressStore {
    WordProgress* items;
    int           count;
    int           capacity;
    StudyState    state;
};

// 单词 -> 进度表的键 (去首尾空格、ASCII 转小写)
void progress_word_key(const char* en, char* out, size_t out_len);

// 解析进度文件; json 为空或损坏时表清空、state 清零, 返回 false。超过 capacity 的词丢弃
bool store_parse(const char* json, size_t len, ProgressStore& store);

// 写成进度文件文本; 缓冲不够返回 0。缓冲大小用 store_json_capacity 估算
size_t store_serialize(const ProgressStore& store, char* out, size_t out_len);
size_t store_json_capacity(const ProgressStore& store);

// 按每张卡的单词从表里取进度到 progress[0..deck.count); 表里没有的为盒 1 未学过
void store_load_deck(const ProgressStore& store, const Deck& deck, CardProgress* progress);

// 记下一个词的进度 (已有则更新); 表满且是新词时返回 false
bool store_put(ProgressStore& store, const char* en, const CardProgress& p);

// 同一词库里重复的词只留第一张: 原地压缩 idx[0..n), 返回剩下的张数
int study_unique_words(const Deck& deck, int* idx, int n);

// 学习日变了就清零当日新词数; 返回是否换了天
bool study_roll_day(StudyState& state, int32_t today);

// 评分: 更新盒子与到期日; 第一次评分的卡计入当日新词
void study_grade(CardProgress& p, bool remembered, int32_t today, StudyState& state);

// 今天的队列: candidates (已按分类筛好的卡片下标) 中所有到期的已学卡 + 按顺序取的新词
// (最多 new_per_day - new_today 个, NEW_PER_DAY_UNLIMITED 不限), 再打乱。返回张数
int study_build_queue(const CardProgress* progress, const int* candidates, int n, int32_t today,
                      int new_per_day, const StudyState& state, uint32_t seed, int* out);

// 完成页统计: candidates 中到 day 为止到期的已学卡数 / 还没学过的卡数
int study_count_due(const CardProgress* progress, const int* candidates, int n, int32_t day);
int study_count_unseen(const CardProgress* progress, const int* candidates, int n);
// 完成页盒子分布: counts[0] 为没学过的卡, counts[1..5] 为已学卡所在盒子
void study_count_boxes(const CardProgress* progress, const int* candidates, int n, int counts[6]);

// "再过一遍全部词": candidates 全部打乱
int study_build_all(const int* candidates, int n, uint32_t seed, int* out);

#endif // LEITNER_H
