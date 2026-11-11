#ifndef PENDING_EVENT_H
#define PENDING_EVENT_H

#include <atomic>
#include "spell_types.h"

/**
 * @file pending_event.h
 * @brief 按键采样任务与主循环之间只容 1 个事件的待处理槽 (不依赖 Arduino, 可在主机上单元测试)
 *
 * 刷屏期间主循环不读按键, 采样任务照常识别并放进槽里, 刷完屏主循环再取.
 * 槽满时后来的按键丢弃, 免得连按几下一口气跳过还没看到的卡; 只有关机长按会顶掉槽里的事件.
 */

class PendingEvent {
public:
    // 采样任务调用
    void offer(input_event_t evt) {
        if (evt == EVT_NONE) return;
        if (evt == EVT_PWR_LONG_PRESS) {
            _slot.store(evt);
            return;
        }
        int expected = EVT_NONE;
        _slot.compare_exchange_strong(expected, evt);
    }

    // 主循环调用, 取出并清空; 槽空时返回 EVT_NONE
    input_event_t take() { return (input_event_t)_slot.exchange(EVT_NONE); }

private:
    std::atomic<int> _slot{EVT_NONE};
};

#endif // PENDING_EVENT_H
