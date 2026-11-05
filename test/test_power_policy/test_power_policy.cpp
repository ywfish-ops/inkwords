#include <unity.h>
#include "power_policy.h"

void setUp() {}
void tearDown() {}

static LightSleepInputs idleOnBattery() { return {true, false, false, false}; }

void test_sleeps_when_idle_on_battery() {
    TEST_ASSERT_TRUE(should_light_sleep(idleOnBattery()));
}

void test_stays_awake_when_any_blocker() {
    LightSleepInputs in = idleOnBattery();
    in.panel_asleep = false;
    TEST_ASSERT_FALSE(should_light_sleep(in));
    in = idleOnBattery();
    in.usb_powered = true;
    TEST_ASSERT_FALSE(should_light_sleep(in));
    in = idleOnBattery();
    in.button_pressed = true;
    TEST_ASSERT_FALSE(should_light_sleep(in));
    in = idleOnBattery();
    in.audio_playing = true;
    TEST_ASSERT_FALSE(should_light_sleep(in));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_sleeps_when_idle_on_battery);
    RUN_TEST(test_stays_awake_when_any_blocker);
    return UNITY_END();
}
