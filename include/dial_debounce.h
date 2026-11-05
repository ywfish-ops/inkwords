#ifndef DIAL_DEBOUNCE_H
#define DIAL_DEBOUNCE_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @file dial_debounce.h
 * @brief 拨轮单方向防抖状态机 (Active LOW, 不依赖 Arduino, 可在主机上单元测试)
 *
 * 按下沿立即触发 (零延迟); 之后必须连续保持 HIGH 满 RELEASE_STABLE_MS 才视为松开,
 * 期间任何 LOW 抖动都会重新计时, 因此松开回弹与按住期间的接触抖动都不会重复触发。
 */

class DialDebounce {
public:
    static const uint32_t RELEASE_STABLE_MS = 30;

    // raw: 当前引脚电平 (true = HIGH 释放, false = LOW 按下)
    // 返回 true 表示确认一次新的按下事件
    bool update(bool raw, uint32_t now) {
        if (_released) {
            if (!raw) {
                _released = false;
                _high_since_valid = false;
                return true;
            }
        } else if (!raw) {
            _high_since_valid = false;
        } else if (!_high_since_valid) {
            _high_since = now;
            _high_since_valid = true;
        } else if (now - _high_since >= RELEASE_STABLE_MS) {
            _released = true;
        }
        return false;
    }

private:
    bool     _released = true;
    bool     _high_since_valid = false;
    uint32_t _high_since = 0;
};

#endif // DIAL_DEBOUNCE_H
