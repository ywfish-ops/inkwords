#include <unity.h>
#include "study_day.h"

void setUp() {}
void tearDown() {}

static LocalDateTime dt(int y, int mo, int d, int h = 0, int mi = 0, int s = 0) {
    LocalDateTime t = {y, mo, d, h, mi, s};
    return t;
}

void test_epoch_known_values() {
    TEST_ASSERT_EQUAL_INT64(946684800LL, datetime_to_epoch(dt(2000, 1, 1)));
    TEST_ASSERT_EQUAL_INT64(1792497600LL, datetime_to_epoch(dt(2026, 10, 20, 12)));
}

void test_epoch_roundtrip_across_leap_years() {
    const LocalDateTime cases[] = {dt(2000, 2, 29, 23, 59, 59), dt(2024, 12, 31, 0, 0, 1),
                                   dt(2026, 3, 1, 4, 0, 0), dt(2099, 12, 31, 23, 59, 59)};
    for (const LocalDateTime& c : cases) {
        LocalDateTime back;
        epoch_to_datetime(datetime_to_epoch(c), back);
        TEST_ASSERT_EQUAL(c.year, back.year);
        TEST_ASSERT_EQUAL(c.month, back.month);
        TEST_ASSERT_EQUAL(c.day, back.day);
        TEST_ASSERT_EQUAL(c.hour, back.hour);
        TEST_ASSERT_EQUAL(c.minute, back.minute);
        TEST_ASSERT_EQUAL(c.second, back.second);
    }
}

void test_validity() {
    TEST_ASSERT_TRUE(datetime_valid(dt(2024, 2, 29)));
    TEST_ASSERT_FALSE(datetime_valid(dt(2026, 2, 29)));
    TEST_ASSERT_FALSE(datetime_valid(dt(2100, 2, 28)));
    TEST_ASSERT_FALSE(datetime_valid(dt(1999, 12, 31)));
    TEST_ASSERT_FALSE(datetime_valid(dt(2026, 13, 1)));
    TEST_ASSERT_FALSE(datetime_valid(dt(2026, 4, 31)));
    TEST_ASSERT_FALSE(datetime_valid(dt(2026, 1, 1, 24)));
    TEST_ASSERT_FALSE(datetime_valid(dt(2026, 1, 1, 0, 60)));
}

// 学习日以凌晨 4 点为界: 23:30 与次日 00:30、03:59 同一天, 04:00 是新的一天
void test_study_day_boundary_at_4am() {
    int32_t d = study_day(datetime_to_epoch(dt(2026, 10, 20, 23, 30)));
    TEST_ASSERT_EQUAL(d, study_day(datetime_to_epoch(dt(2026, 10, 21, 0, 30))));
    TEST_ASSERT_EQUAL(d, study_day(datetime_to_epoch(dt(2026, 10, 21, 3, 59, 59))));
    TEST_ASSERT_EQUAL(d + 1, study_day(datetime_to_epoch(dt(2026, 10, 21, 4, 0, 0))));
    TEST_ASSERT_EQUAL(d, study_day(datetime_to_epoch(dt(2026, 10, 20, 4, 0, 0))));
    TEST_ASSERT_EQUAL(d - 1, study_day(datetime_to_epoch(dt(2026, 10, 20, 3, 59, 59))));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_epoch_known_values);
    RUN_TEST(test_epoch_roundtrip_across_leap_years);
    RUN_TEST(test_validity);
    RUN_TEST(test_study_day_boundary_at_4am);
    return UNITY_END();
}
