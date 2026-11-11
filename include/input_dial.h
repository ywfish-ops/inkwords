#ifndef INPUT_DIAL_H
#define INPUT_DIAL_H

#include <stdint.h>
#include <stdbool.h>
#include <Arduino.h>
#include "board_config.h"
#include "spell_types.h"
#include "dial_debounce.h"
#include "power_key.h"
#include "pending_event.h"

/**
 * @file input_dial.h
 * @brief 微雪 Waveshare ESP32-S3-ePaper-3.97 三向滚轮按键与 BOOT 键轮询驱动
 *
 * 按键由后台任务每 2ms 采样 (刷屏时主循环被 waitBusy 卡住, 不会因此漏按), 识别出的事件放进
 * 只容 1 个的待处理槽, 主循环用 poll() 取.
 */

class InputDial {
public:
    static InputDial& getInstance() {
        static InputDial instance;
        return instance;
    }

    // 初始化按键引脚 (配置内置上拉) 并启动采样任务
    void begin();

    // 取出待处理的按键事件 (EVT_NONE 表示无按键)
    input_event_t poll() { return _pending.take(); }

private:
    InputDial() = default;

    struct ButtonState {
        int pin;
        bool last_raw_state;
        bool debounced_state;
        uint32_t last_debounce_time;
        uint32_t press_start_time;
        bool long_press_emitted;
    };

    ButtonState _btn_up;
    ButtonState _btn_down;
    ButtonState _btn_confirm;
    ButtonState _btn_boot;

    DialDebounce _deb_up;
    DialDebounce _deb_down;
    DialDebounce _deb_confirm;
    PowerKey     _pwr;

    PendingEvent _pending;

    void initButton(ButtonState& btn, int pin);
    input_event_t sample();  // 只在采样任务里调用
    static void taskEntry(void* arg);
};

#endif // INPUT_DIAL_H
