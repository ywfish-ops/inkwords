#ifndef POWER_POLICY_H
#define POWER_POLICY_H

/**
 * @file power_policy.h
 * @brief 何时让 MCU 进入浅睡眠 (纯逻辑, 可在主机上测试)
 */

struct LightSleepInputs {
    bool panel_asleep;      // 墨水屏已因空闲进入深睡
    bool usb_powered;       // 插着 USB (VBUS 有电): 不缺电, 且睡眠会断开 USB 串口
    bool button_pressed;    // 有按键仍按着: 低电平唤醒会立即触发
    bool audio_playing;     // 正在发音
};

inline bool should_light_sleep(const LightSleepInputs& in) {
    return in.panel_asleep && !in.usb_powered && !in.button_pressed && !in.audio_playing;
}

#endif // POWER_POLICY_H
