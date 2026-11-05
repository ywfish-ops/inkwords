#include <unity.h>
#include "power_key.h"

void setUp() {}
void tearDown() {}

static int run(PowerKey& k, bool (*pressed)(uint32_t), uint32_t from, uint32_t to) {
    int n = 0;
    for (uint32_t t = from; t < to; t += 2) n += k.update(pressed(t), t);
    return n;
}

// 短按 (实测约 200ms) 不触发, 防误触
void test_short_press_is_ignored() {
    PowerKey k;
    TEST_ASSERT_EQUAL(0, run(k, [](uint32_t t) {
        return (t >= 1000 && t < 1200) || (t >= 2000 && t < 2900);
    }, 0, 4000));
}

// 按住满 LONG_PRESS_MS 触发一次, 继续按住不重复触发
void test_long_press_fires_once() {
    PowerKey k;
    int fired_at = -1;
    for (uint32_t t = 0; t < 8000; t += 2) {
        bool pressed = t >= 1000 && t < 7000;
        if (k.update(pressed, t)) {
            TEST_ASSERT_EQUAL(-1, fired_at);
            fired_at = (int)t;
        }
    }
    TEST_ASSERT_EQUAL(1000 + (int)PowerKey::LONG_PRESS_MS, fired_at);
}

// 按住期间的短暂抖动不重新计时
void test_glitch_during_hold_keeps_timer() {
    PowerKey k;
    TEST_ASSERT_EQUAL(1, run(k, [](uint32_t t) {
        return t >= 1000 && t < 3500 && !(t >= 1500 && t < 1504);
    }, 0, 4000));
}

// 松开后重新按住要重新计满时长
void test_release_resets_timer() {
    PowerKey k;
    TEST_ASSERT_EQUAL(0, run(k, [](uint32_t t) {
        return (t >= 1000 && t < 2500) || (t >= 2700 && t < 4000);
    }, 0, 4500));
}

// 开机时按键仍按着 (按电源键开机): 即使按满时长也不触发; 松开后再长按才触发
void test_held_at_boot_is_ignored() {
    PowerKey k;
    TEST_ASSERT_EQUAL(0, run(k, [](uint32_t t) { return t < 3000; }, 0, 3500));
    TEST_ASSERT_EQUAL(1, run(k, [](uint32_t t) { return t >= 4000 && t < 6500; }, 3500, 7000));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_short_press_is_ignored);
    RUN_TEST(test_long_press_fires_once);
    RUN_TEST(test_glitch_during_hold_keeps_timer);
    RUN_TEST(test_release_resets_timer);
    RUN_TEST(test_held_at_boot_is_ignored);
    return UNITY_END();
}
