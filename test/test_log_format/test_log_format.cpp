#include <unity.h>
#include <string.h>
#include "log_format.h"

void setUp() {}
void tearDown() {}

void test_line_with_time() {
    LocalDateTime dt = {2026, 10, 21, 21, 30, 5};
    char buf[128];
    size_t n = log_format_line(buf, sizeof(buf), &dt, 12345, 'E', "SD", "写入失败");
    TEST_ASSERT_EQUAL_STRING("2026-10-21 21:30:05 +12345 E SD    写入失败\n", buf);
    TEST_ASSERT_EQUAL(strlen(buf), n);
}

void test_line_without_time() {
    char buf[128];
    log_format_line(buf, sizeof(buf), nullptr, 7, 'I', "BOOT", "开机");
    TEST_ASSERT_EQUAL_STRING("---------- --:--:-- +7 I BOOT  开机\n", buf);
}

// 内容里的换行变成空格, 一条记录始终只占一行
void test_newlines_in_message_flattened() {
    char buf[128];
    log_format_line(buf, sizeof(buf), nullptr, 1, 'W', "DECK", "a\nb\rc");
    TEST_ASSERT_EQUAL_STRING("---------- --:--:-- +1 W DECK  a b c\n", buf);
}

// 过长截断但仍以换行结尾
void test_truncated_line_ends_with_newline() {
    char buf[32];
    size_t n = log_format_line(buf, sizeof(buf), nullptr, 1, 'I', "X", "0123456789012345678901234567890123456789");
    TEST_ASSERT_EQUAL(31, n);
    TEST_ASSERT_EQUAL('\n', buf[30]);
    TEST_ASSERT_EQUAL('\0', buf[31]);
}

// 截断不切断汉字
void test_truncation_keeps_utf8_whole() {
    char buf[40];
    log_format_line(buf, sizeof(buf), nullptr, 1, 'I', "X", "汉汉汉汉汉汉汉汉汉汉汉汉");
    size_t n = strlen(buf);
    TEST_ASSERT_EQUAL('\n', buf[n - 1]);
    // "---------- --:--:-- +1 I X     " 共 31 字节, 剩下的只能放完整的汉字 (3 字节一个)
    TEST_ASSERT_EQUAL(0, (n - 1 - 31) % 3);
}

void test_rotate_decision() {
    TEST_ASSERT_FALSE(log_needs_rotate(100, 50, 200));
    TEST_ASSERT_FALSE(log_needs_rotate(150, 50, 200));
    TEST_ASSERT_TRUE(log_needs_rotate(151, 50, 200));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_line_with_time);
    RUN_TEST(test_line_without_time);
    RUN_TEST(test_newlines_in_message_flattened);
    RUN_TEST(test_truncated_line_ends_with_newline);
    RUN_TEST(test_truncation_keeps_utf8_whole);
    RUN_TEST(test_rotate_decision);
    return UNITY_END();
}
