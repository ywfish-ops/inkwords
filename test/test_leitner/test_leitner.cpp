#include <unity.h>
#include <string.h>
#include <string>
#include "leitner.h"

static WordCard s_cards[6];
static Deck s_deck;
static CardProgress s_prog[6];
static StudyState s_state;
static const int ALL[6] = {0, 1, 2, 3, 4, 5};
static const int32_t TODAY = 20746;

void setUp() {
    memset(s_cards, 0, sizeof(s_cards));
    for (int i = 0; i < 6; i++) snprintf(s_cards[i].key, sizeof(s_cards[i].key), "d-%d", i);
    memset(&s_deck, 0, sizeof(s_deck));
    s_deck.count = 6;
    s_deck.cards = s_cards;
    for (int i = 0; i < 6; i++) s_prog[i] = {1, 0, false};
    s_state = {TODAY, 0};
}
void tearDown() { s_deck.cards = nullptr; }

static bool contains(const int* q, int n, int v) {
    for (int i = 0; i < n; i++)
        if (q[i] == v) return true;
    return false;
}

// 规则: 记住了升一盒, 没记住回盒 1, 到期 = 今天 + [0,1,2,4,7,14][盒]
void test_grade_moves_box_and_due() {
    CardProgress p = {1, 0, false};
    study_grade(p, true, TODAY, s_state);
    TEST_ASSERT_EQUAL(2, p.box);
    TEST_ASSERT_EQUAL(TODAY + 2, p.due_day);
    TEST_ASSERT_TRUE(p.seen);
    p.box = 5;
    study_grade(p, true, TODAY, s_state);
    TEST_ASSERT_EQUAL(5, p.box);  // 最高盒 5
    TEST_ASSERT_EQUAL(TODAY + 14, p.due_day);
    study_grade(p, false, TODAY, s_state);
    TEST_ASSERT_EQUAL(1, p.box);
    TEST_ASSERT_EQUAL(TODAY + 1, p.due_day);  // 盒 1 明天到期
}

// 只有第一次评分计入当日新词
void test_new_today_counts_first_grade_only() {
    CardProgress p = {1, 0, false};
    study_grade(p, false, TODAY, s_state);
    study_grade(p, true, TODAY, s_state);
    TEST_ASSERT_EQUAL(1, s_state.new_today);
}

void test_roll_day_resets_new_count() {
    s_state.new_today = 7;
    TEST_ASSERT_FALSE(study_roll_day(s_state, TODAY));
    TEST_ASSERT_EQUAL(7, s_state.new_today);
    TEST_ASSERT_TRUE(study_roll_day(s_state, TODAY + 1));
    TEST_ASSERT_EQUAL(0, s_state.new_today);
    TEST_ASSERT_EQUAL(TODAY + 1, s_state.day);
}

// 队列 = 到期的已学卡 (不限量) + 按词库顺序的新词 (受额度限制)
void test_queue_due_plus_limited_new() {
    s_prog[0] = {2, TODAY - 1, true};   // 已到期
    s_prog[1] = {3, TODAY, true};       // 今天到期
    s_prog[2] = {4, TODAY + 1, true};   // 未到期
    int q[6];
    int n = study_build_queue(s_prog, ALL, 6, TODAY, 2, s_state, 1, q);
    TEST_ASSERT_EQUAL(4, n);
    TEST_ASSERT_TRUE(contains(q, n, 0));
    TEST_ASSERT_TRUE(contains(q, n, 1));
    TEST_ASSERT_FALSE(contains(q, n, 2));
    TEST_ASSERT_TRUE(contains(q, n, 3));  // 新词按顺序取前 2 个
    TEST_ASSERT_TRUE(contains(q, n, 4));
    TEST_ASSERT_FALSE(contains(q, n, 5));
}

// 今天已学满额度: 只剩到期卡; 不限额度: 全部新词
void test_queue_respects_new_today_and_unlimited() {
    s_prog[0] = {2, TODAY, true};
    s_state.new_today = 2;
    int q[6];
    TEST_ASSERT_EQUAL(1, study_build_queue(s_prog, ALL, 6, TODAY, 2, s_state, 1, q));
    TEST_ASSERT_EQUAL(0, q[0]);
    TEST_ASSERT_EQUAL(6, study_build_queue(s_prog, ALL, 6, TODAY, NEW_PER_DAY_UNLIMITED, s_state, 1, q));
    s_state.new_today = 5;  // 调低额度后已超出: 不补新词, 也不出错
    TEST_ASSERT_EQUAL(1, study_build_queue(s_prog, ALL, 6, TODAY, 2, s_state, 1, q));
}

// 只在给定的分类候选中挑
void test_queue_only_uses_candidates() {
    const int cand[2] = {1, 4};
    int q[6];
    int n = study_build_queue(s_prog, cand, 2, TODAY, 10, s_state, 1, q);
    TEST_ASSERT_EQUAL(2, n);
    TEST_ASSERT_TRUE(contains(q, n, 1));
    TEST_ASSERT_TRUE(contains(q, n, 4));
}

// 打乱只改顺序, 不丢不重
void test_build_all_is_permutation() {
    int q[6];
    TEST_ASSERT_EQUAL(6, study_build_all(ALL, 6, 12345, q));
    for (int v = 0; v < 6; v++) TEST_ASSERT_TRUE(contains(q, 6, v));
}

void test_progress_roundtrip() {
    s_prog[1] = {3, TODAY + 4, true};
    s_prog[4] = {1, TODAY, true};
    s_state.new_today = 2;
    std::string buf(progress_json_capacity(6), '\0');
    size_t n = progress_serialize(s_deck, s_prog, s_state, &buf[0], buf.size());
    TEST_ASSERT_TRUE(n > 0);

    CardProgress back[6];
    StudyState st;
    TEST_ASSERT_TRUE(progress_parse(buf.c_str(), n, s_deck, back, st));
    TEST_ASSERT_EQUAL(TODAY, st.day);
    TEST_ASSERT_EQUAL(2, st.new_today);
    TEST_ASSERT_TRUE(back[1].seen);
    TEST_ASSERT_EQUAL(3, back[1].box);
    TEST_ASSERT_EQUAL(TODAY + 4, back[1].due_day);
    TEST_ASSERT_TRUE(back[4].seen);
    TEST_ASSERT_FALSE(back[0].seen);
    TEST_ASSERT_EQUAL(1, back[0].box);
}

// 损坏/缺失: 全部未学过; 未知 key 忽略; 越界盒子夹到 1..5
void test_progress_parse_robust() {
    CardProgress p[6];
    StudyState st = {99, 9};
    TEST_ASSERT_FALSE(progress_parse(nullptr, 0, s_deck, p, st));
    TEST_ASSERT_EQUAL(0, st.day);
    TEST_ASSERT_FALSE(progress_parse("{bad", 4, s_deck, p, st));
    for (int i = 0; i < 6; i++) TEST_ASSERT_FALSE(p[i].seen);
    const char* j = R"({"day":5,"cards":{"no_such_card":{"b":3,"d":1},"d-2":{"b":9,"d":7},"d-3":{"b":0}}})";
    TEST_ASSERT_TRUE(progress_parse(j, strlen(j), s_deck, p, st));
    TEST_ASSERT_EQUAL(5, p[2].box);
    TEST_ASSERT_EQUAL(7, p[2].due_day);
    TEST_ASSERT_EQUAL(1, p[3].box);
    TEST_ASSERT_EQUAL(0, st.new_today);
}

void test_serialize_too_small_buffer() {
    s_prog[0] = {2, TODAY, true};
    char tiny[16];
    TEST_ASSERT_EQUAL(0, progress_serialize(s_deck, s_prog, s_state, tiny, sizeof(tiny)));
}

void test_counts_for_done_page() {
    s_prog[0] = {2, TODAY + 1, true};
    s_prog[1] = {3, TODAY + 2, true};
    s_prog[2] = {1, TODAY, true};
    TEST_ASSERT_EQUAL(2, study_count_due(s_prog, ALL, 6, TODAY + 1));
    TEST_ASSERT_EQUAL(3, study_count_unseen(s_prog, ALL, 6));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_grade_moves_box_and_due);
    RUN_TEST(test_new_today_counts_first_grade_only);
    RUN_TEST(test_roll_day_resets_new_count);
    RUN_TEST(test_queue_due_plus_limited_new);
    RUN_TEST(test_queue_respects_new_today_and_unlimited);
    RUN_TEST(test_queue_only_uses_candidates);
    RUN_TEST(test_build_all_is_permutation);
    RUN_TEST(test_progress_roundtrip);
    RUN_TEST(test_progress_parse_robust);
    RUN_TEST(test_serialize_too_small_buffer);
    RUN_TEST(test_counts_for_done_page);
    return UNITY_END();
}
