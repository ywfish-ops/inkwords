#include <unity.h>
#include "review_input.h"

void setUp() {}
void tearDown() {}

// 正面: 会的词直接下拨记住了, 拿不准上拨或中键看答案; 没有上一张/下一张
void test_front() {
    TEST_ASSERT_EQUAL(REVIEW_REMEMBERED, review_action(VIEW_CARD_FRONT, EVT_DIAL_DOWN));
    TEST_ASSERT_EQUAL(REVIEW_SHOW_BACK, review_action(VIEW_CARD_FRONT, EVT_DIAL_UP));
    TEST_ASSERT_EQUAL(REVIEW_SHOW_BACK, review_action(VIEW_CARD_FRONT, EVT_DIAL_CONFIRM));
}

// 背面: 上拨没记住, 下拨记住了, 中键翻回正面
void test_back() {
    TEST_ASSERT_EQUAL(REVIEW_FORGOT, review_action(VIEW_CARD_BACK, EVT_DIAL_UP));
    TEST_ASSERT_EQUAL(REVIEW_REMEMBERED, review_action(VIEW_CARD_BACK, EVT_DIAL_DOWN));
    TEST_ASSERT_EQUAL(REVIEW_SHOW_FRONT, review_action(VIEW_CARD_BACK, EVT_DIAL_CONFIRM));
}

// 下拨无论正反面都是记住了
void test_down_always_means_remembered() {
    TEST_ASSERT_EQUAL(review_action(VIEW_CARD_FRONT, EVT_DIAL_DOWN), review_action(VIEW_CARD_BACK, EVT_DIAL_DOWN));
}

void test_done_page_restarts() {
    TEST_ASSERT_EQUAL(REVIEW_RESTART, review_action(VIEW_DONE_STATE, EVT_DIAL_UP));
    TEST_ASSERT_EQUAL(REVIEW_RESTART, review_action(VIEW_DONE_STATE, EVT_DIAL_DOWN));
    TEST_ASSERT_EQUAL(REVIEW_RESTART, review_action(VIEW_DONE_STATE, EVT_DIAL_CONFIRM));
}

// BOOT 键与其他页面不属于背卡评分
void test_other_events_and_views_ignored() {
    TEST_ASSERT_EQUAL(REVIEW_NONE, review_action(VIEW_CARD_FRONT, EVT_BOOT_CLICK));
    TEST_ASSERT_EQUAL(REVIEW_NONE, review_action(VIEW_CARD_BACK, EVT_BOOT_LONG_PRESS));
    TEST_ASSERT_EQUAL(REVIEW_NONE, review_action(VIEW_MENU_MODAL, EVT_DIAL_DOWN));
    TEST_ASSERT_EQUAL(REVIEW_NONE, review_action(VIEW_ERROR, EVT_DIAL_DOWN));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_front);
    RUN_TEST(test_back);
    RUN_TEST(test_down_always_means_remembered);
    RUN_TEST(test_done_page_restarts);
    RUN_TEST(test_other_events_and_views_ignored);
    return UNITY_END();
}
