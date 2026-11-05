#ifndef REVIEW_INPUT_H
#define REVIEW_INPUT_H

#include "spell_types.h"

/**
 * @file review_input.h
 * @brief 背卡时拨轮的含义 (纯逻辑, 不依赖硬件)
 *
 *          上拨           中键        下拨
 *   正面   翻面看答案     翻面        记住了 (会的词不用翻面)
 *   背面   没记住         翻回正面    记住了
 *   完成页 重新开始       重新开始    重新开始
 *
 * 不提供上一张/下一张浏览: 每张卡都要评分, 不能跳过。
 */

typedef enum {
    REVIEW_NONE = 0,
    REVIEW_SHOW_BACK,    // 翻到背面
    REVIEW_SHOW_FRONT,   // 翻回正面
    REVIEW_REMEMBERED,   // 记住了: 升一盒, 下一张
    REVIEW_FORGOT,       // 没记住: 回盒 1, 下一张
    REVIEW_RESTART       // 从第 1 张重新开始
} review_action_t;

review_action_t review_action(ui_view_state_t view, input_event_t evt);

#endif // REVIEW_INPUT_H
