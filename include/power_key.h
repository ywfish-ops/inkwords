#ifndef POWER_KEY_H
#define POWER_KEY_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @file power_key.h
 * @brief 电源键长按检测 (GPIO1, 高电平有效, 已实测: 松开 0 / 按下 1)
 *        按住满 LONG_PRESS_MS 触发一次 (短按忽略, 防误触); 触发后还要画关机页并全刷 (约 2.1 秒),
 *        合计须短于 AXP2101 的 8 秒硬件关机, 以便按着不放也能先刷完关机页再软关机。
 *        开机后须先观察到按键松开才开始响应, 避免按电源键开机时被当成关机。
 */

class PowerKey {
public:
    static const uint32_t LONG_PRESS_MS = 2000;
    static const uint32_t RELEASE_STABLE_MS = 30;  // 松开持续这么久才算放手, 容忍按住时的抖动

    // pressed_level: 当前引脚是否为按下电平 (HIGH); 返回 true 表示长按达到时长
    bool update(bool pressed_level, uint32_t now) {
        if (!_armed) {
            if (!pressed_level) _armed = true;
            return false;
        }

        if (pressed_level) {
            _released_valid = false;
            if (!_holding) {
                _holding = true;
                _fired = false;
                _hold_since = now;
            } else if (!_fired && now - _hold_since >= LONG_PRESS_MS) {
                _fired = true;
                return true;
            }
        } else if (_holding) {
            if (!_released_valid) {
                _released_valid = true;
                _released_since = now;
            } else if (now - _released_since >= RELEASE_STABLE_MS) {
                _holding = false;
            }
        }
        return false;
    }

    bool armed() const { return _armed; }

private:
    bool     _armed = false;
    bool     _holding = false;
    bool     _fired = false;
    bool     _released_valid = false;
    uint32_t _hold_since = 0;
    uint32_t _released_since = 0;
};

#endif // POWER_KEY_H
