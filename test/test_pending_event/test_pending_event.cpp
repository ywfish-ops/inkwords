#include <unity.h>
#include <initializer_list>
#include "dial_debounce.h"
#include "pending_event.h"

// 刷屏 (waitBusy) 期间主循环不读按键. 后台任务每 2ms 采样, 识别到的按下放进只容 1 个的待处理槽,
// 主循环刷完屏再取. 这里用 t 模拟毫秒时间, isPressed 描述手指动作.
static int deliveredAfterBlock(bool (*isPressed)(uint32_t t), uint32_t block_from, uint32_t block_to,
                               PendingEvent& slot) {
    DialDebounce d;
    int taken = 0;
    for (uint32_t t = 1000; t < 2000; t += 2) {
        if (d.update(!isPressed(t), t)) slot.offer(EVT_DIAL_DOWN);
        bool main_loop_blocked = t >= block_from && t < block_to;
        if (!main_loop_blocked && slot.take() != EVT_NONE) taken++;
    }
    return taken;
}

void setUp() {}
void tearDown() {}

void test_polling_only_from_main_loop_misses_press_during_refresh() {
    // 复现: 旧做法只在主循环里采样, 刷屏 1000..1500ms 期间按下又松开, 完全看不到
    DialDebounce d;
    int n = 0;
    auto pressed = [](uint32_t t) { return t >= 1100 && t < 1160; };
    for (uint32_t t : {1000u, 1500u, 1502u}) {
        if (d.update(!pressed(t), t)) n++;
    }
    TEST_ASSERT_EQUAL(0, n);
}

void test_press_during_refresh_is_delivered_after_it() {
    PendingEvent slot;
    TEST_ASSERT_EQUAL(1, deliveredAfterBlock([](uint32_t t) { return t >= 1100 && t < 1160; }, 1000, 1500, slot));
}

void test_only_one_press_is_kept_during_refresh() {
    // 刷屏期间连按 3 下, 只保留第 1 下, 免得一口气跳过没看到的卡
    PendingEvent slot;
    TEST_ASSERT_EQUAL(1, deliveredAfterBlock([](uint32_t t) {
        return (t >= 1100 && t < 1160) || (t >= 1220 && t < 1280) || (t >= 1340 && t < 1400);
    }, 1000, 1500, slot));
}

void test_presses_without_refresh_all_delivered() {
    PendingEvent slot;
    TEST_ASSERT_EQUAL(3, deliveredAfterBlock([](uint32_t t) {
        return (t >= 1100 && t < 1160) || (t >= 1220 && t < 1280) || (t >= 1340 && t < 1400);
    }, 0, 0, slot));
}

void test_take_empties_the_slot() {
    PendingEvent slot;
    slot.offer(EVT_DIAL_CONFIRM);
    TEST_ASSERT_EQUAL(EVT_DIAL_CONFIRM, slot.take());
    TEST_ASSERT_EQUAL(EVT_NONE, slot.take());
}

void test_first_event_wins_when_slot_full() {
    PendingEvent slot;
    slot.offer(EVT_DIAL_UP);
    slot.offer(EVT_DIAL_DOWN);
    TEST_ASSERT_EQUAL(EVT_DIAL_UP, slot.take());
}

void test_power_off_replaces_pending_event() {
    // 关机不能因为槽里压着一个拨轮事件而丢掉
    PendingEvent slot;
    slot.offer(EVT_DIAL_UP);
    slot.offer(EVT_PWR_LONG_PRESS);
    TEST_ASSERT_EQUAL(EVT_PWR_LONG_PRESS, slot.take());
}

void test_power_off_is_not_replaced() {
    PendingEvent slot;
    slot.offer(EVT_PWR_LONG_PRESS);
    slot.offer(EVT_DIAL_UP);
    TEST_ASSERT_EQUAL(EVT_PWR_LONG_PRESS, slot.take());
}

void test_none_is_ignored() {
    PendingEvent slot;
    slot.offer(EVT_NONE);
    slot.offer(EVT_DIAL_DOWN);
    TEST_ASSERT_EQUAL(EVT_DIAL_DOWN, slot.take());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_polling_only_from_main_loop_misses_press_during_refresh);
    RUN_TEST(test_press_during_refresh_is_delivered_after_it);
    RUN_TEST(test_only_one_press_is_kept_during_refresh);
    RUN_TEST(test_presses_without_refresh_all_delivered);
    RUN_TEST(test_take_empties_the_slot);
    RUN_TEST(test_first_event_wins_when_slot_full);
    RUN_TEST(test_power_off_replaces_pending_event);
    RUN_TEST(test_power_off_is_not_replaced);
    RUN_TEST(test_none_is_ignored);
    return UNITY_END();
}
