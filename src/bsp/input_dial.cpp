#include "input_dial.h"

static const uint32_t DEBOUNCE_DELAY_MS = 25;
static const uint32_t LONG_PRESS_MS     = 1200;

void InputDial::initButton(ButtonState& btn, int pin) {
    btn.pin = pin;
    btn.last_raw_state = true;    // Pull-up: HIGH by default
    btn.debounced_state = true;
    btn.last_debounce_time = 0;
    btn.press_start_time = 0;
    btn.long_press_emitted = false;
    pinMode(pin, INPUT_PULLUP);
}

void InputDial::begin() {
    initButton(_btn_up, DIAL_UP_PIN);
    initButton(_btn_confirm, DIAL_CONFIRM_PIN);
    initButton(_btn_down, DIAL_DOWN_PIN);
    initButton(_btn_boot, BTN_BOOT_PIN);
    pinMode(BTN_PWR_PIN, INPUT);  // 板上已有外部电路, 不加内部上下拉
}

input_event_t InputDial::poll() {
    uint32_t now = millis();
    input_event_t event = EVT_NONE;

    // 三个拨轮方向并发更新, 避免单向阻塞 (防抖逻辑见 dial_debounce.h)
    bool trig_confirm = _deb_confirm.update(digitalRead(_btn_confirm.pin), now);
    bool trig_up      = _deb_up.update(digitalRead(_btn_up.pin), now);
    bool trig_down    = _deb_down.update(digitalRead(_btn_down.pin), now);

    if (_pwr.update(digitalRead(BTN_PWR_PIN) == HIGH, now)) return EVT_PWR_LONG_PRESS;
    if (trig_confirm) return EVT_DIAL_CONFIRM;
    if (trig_up)      return EVT_DIAL_UP;
    if (trig_down)    return EVT_DIAL_DOWN;

    // 检测 BOOT 键 (支持短按发音 与 长按1.2s全局全刷消除残影)
    bool boot_raw = digitalRead(_btn_boot.pin);
    if (boot_raw != _btn_boot.last_raw_state) {
        _btn_boot.last_debounce_time = now;
        _btn_boot.last_raw_state = boot_raw;
    }
    if ((now - _btn_boot.last_debounce_time) > DEBOUNCE_DELAY_MS) {
        if (boot_raw != _btn_boot.debounced_state) {
            _btn_boot.debounced_state = boot_raw;
            if (_btn_boot.debounced_state == LOW) {
                // 刚刚按下
                _btn_boot.press_start_time = now;
                _btn_boot.long_press_emitted = false;
            } else {
                // 释放按键
                if (!_btn_boot.long_press_emitted && (now - _btn_boot.press_start_time >= 40)) {
                    return EVT_BOOT_CLICK;
                }
            }
        } else if (_btn_boot.debounced_state == LOW) {
            // 处于长按中
            if (!_btn_boot.long_press_emitted && (now - _btn_boot.press_start_time >= LONG_PRESS_MS)) {
                _btn_boot.long_press_emitted = true;
                return EVT_BOOT_LONG_PRESS;
            }
        }
    }

    return EVT_NONE;
}
