#include <unity.h>
#include "dial_debounce.h"

// 以 2ms 轮询周期 (与 loop() 中 delay(2) 一致) 驱动状态机, 统计按下事件次数
static int countPresses(bool (*isPressed)(uint32_t t), uint32_t from = 1000, uint32_t to = 2000) {
    DialDebounce d;
    int n = 0;
    for (uint32_t t = from; t < to; t += 2) {
        if (d.update(!isPressed(t), t)) n++;
    }
    return n;
}

void setUp() {}
void tearDown() {}

void test_clean_press_emits_once() {
    TEST_ASSERT_EQUAL(1, countPresses([](uint32_t t) { return t >= 1100 && t < 1300; }));
}

void test_press_is_reported_immediately() {
    DialDebounce d;
    for (uint32_t t = 1000; t < 1100; t += 2) d.update(true, t);
    TEST_ASSERT_TRUE(d.update(false, 1100));
}

void test_bounce_on_press_emits_once() {
    // 按下瞬间触点抖动 6ms
    TEST_ASSERT_EQUAL(1, countPresses([](uint32_t t) {
        if (t >= 1100 && t < 1106) return (t / 2) % 2 == 0;
        return t >= 1106 && t < 1300;
    }));
}

void test_bounce_on_release_emits_once() {
    // 按住 200ms, 松开后触点回弹 4ms
    TEST_ASSERT_EQUAL(1, countPresses([](uint32_t t) {
        return (t >= 1100 && t < 1300) || (t >= 1302 && t < 1306);
    }));
}

void test_glitch_while_held_emits_once() {
    // 按住期间触点短暂断开 2ms
    TEST_ASSERT_EQUAL(1, countPresses([](uint32_t t) {
        return t >= 1100 && t < 1300 && !(t >= 1140 && t < 1142);
    }));
}

void test_two_distinct_presses_emit_twice() {
    TEST_ASSERT_EQUAL(2, countPresses([](uint32_t t) {
        return (t >= 1100 && t < 1200) || (t >= 1400 && t < 1500);
    }));
}

void test_fast_repeated_flicks_each_count() {
    // 快速连拨: 按 60ms, 松 60ms, 共 3 次
    TEST_ASSERT_EQUAL(3, countPresses([](uint32_t t) {
        return (t >= 1100 && t < 1160) || (t >= 1220 && t < 1280) || (t >= 1340 && t < 1400);
    }));
}

void test_millis_wraparound() {
    DialDebounce d;
    int n = 0;
    uint32_t start = 0xFFFFFF00u;
    for (uint32_t i = 0; i < 500; i += 2) {
        uint32_t t = start + i;
        bool pressed = i >= 100 && i < 300;
        if (d.update(!pressed, t)) n++;
    }
    TEST_ASSERT_EQUAL(1, n);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_clean_press_emits_once);
    RUN_TEST(test_press_is_reported_immediately);
    RUN_TEST(test_bounce_on_press_emits_once);
    RUN_TEST(test_bounce_on_release_emits_once);
    RUN_TEST(test_glitch_while_held_emits_once);
    RUN_TEST(test_two_distinct_presses_emit_twice);
    RUN_TEST(test_fast_repeated_flicks_each_count);
    RUN_TEST(test_millis_wraparound);
    return UNITY_END();
}
