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
 * 进度文件 userdata/progress_<deck>.json, 只记学过的卡:
 *   {"day":20746,"new_today":3,"cards":{"spbcn_r1-0":{"b":2,"d":20748}, ...}}
 */

// 每日新词上限: 不限
#define NEW_PER_DAY_UNLIMITED 0

// 每套词库的当日状态
struct StudyState {
    int32_t day;        // 最近一次使用的学习日
    int     new_today;  // 该学习日已学的新词数 (第一次评分才算)
};

// 解析进度文件到 progress[0..deck.count) (按卡片 key 对应); 未记录的卡为盒 1 未学过。
// json 为空或损坏时全部视为未学过, state 清零, 返回 false
bool progress_parse(const char* json, size_t len, const Deck& deck, CardProgress* progress, StudyState& state);

// 写成进度文件文本; 缓冲不够返回 0。缓冲大小用 progress_json_capacity 估算
size_t progress_serialize(const Deck& deck, const CardProgress* progress, const StudyState& state,
                          char* out, size_t out_len);
size_t progress_json_capacity(int card_count);

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

// "再过一遍全部词": candidates 全部打乱
int study_build_all(const int* candidates, int n, uint32_t seed, int* out);

#endif // LEITNER_H
