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

// 进度按单词存在全局表里 (userdata/progress.json), 各词库共享
static WordProgress s_items[8];
static ProgressStore s_store;

static void resetStore() {
    memset(s_items, 0, sizeof(s_items));
    s_store = {s_items, 0, 8, {TODAY, 0}};
}

static void setWords(const char* const* words) {
    for (int i = 0; i < 6; i++) snprintf(s_cards[i].en, sizeof(s_cards[i].en), "%s", words[i]);
}

void test_word_key_trims_and_lowercases() {
    char k[48];
    progress_word_key("  Apple ", k, sizeof(k));
    TEST_ASSERT_EQUAL_STRING("apple", k);
    progress_word_key("Good morning!", k, sizeof(k));
    TEST_ASSERT_EQUAL_STRING("good morning!", k);
}

void test_store_roundtrip() {
    resetStore();
    TEST_ASSERT_TRUE(store_put(s_store, "pear", {3, TODAY + 4, true}));
    TEST_ASSERT_TRUE(store_put(s_store, "Apple", {1, TODAY, true}));
    TEST_ASSERT_TRUE(store_put(s_store, "say \"hi\"", {2, TODAY + 1, true}));
    s_store.state.new_today = 2;
    std::string buf(store_json_capacity(s_store), '\0');
    size_t n = store_serialize(s_store, &buf[0], buf.size());
    TEST_ASSERT_TRUE(n > 0);

    WordProgress items[8];
    ProgressStore back = {items, 0, 8, {0, 0}};
    TEST_ASSERT_TRUE(store_parse(buf.c_str(), n, back));
    TEST_ASSERT_EQUAL(3, back.count);
    TEST_ASSERT_EQUAL(TODAY, back.state.day);
    TEST_ASSERT_EQUAL(2, back.state.new_today);

    const char* words[6] = {"apple", "banana", "pear", "say \"hi\"", "kiwi", "APPLE"};
    setWords(words);
    CardProgress p[6];
    store_load_deck(back, s_deck, p);
    TEST_ASSERT_TRUE(p[0].seen);
    TEST_ASSERT_EQUAL(1, p[0].box);
    TEST_ASSERT_FALSE(p[1].seen);
    TEST_ASSERT_EQUAL(1, p[1].box);
    TEST_ASSERT_EQUAL(3, p[2].box);
    TEST_ASSERT_EQUAL(TODAY + 4, p[2].due_day);
    TEST_ASSERT_EQUAL(2, p[3].box);
    TEST_ASSERT_TRUE(p[5].seen);  // 大小写不同也是同一个词
}

// 在一个词库里评过的词, 换到别的词库就是已学过的
void test_progress_shared_across_decks() {
    resetStore();
    const char* a[6] = {"cat", "dog", "pig", "cow", "hen", "fox"};
    setWords(a);
    CardProgress pa[6];
    store_load_deck(s_store, s_deck, pa);
    study_grade(pa[1], true, TODAY, s_store.state);
    TEST_ASSERT_TRUE(store_put(s_store, s_cards[1].en, pa[1]));

    const char* b[6] = {"bird", "Dog", "fish", "duck", "goat", "frog"};
    setWords(b);
    CardProgress pb[6];
    store_load_deck(s_store, s_deck, pb);
    TEST_ASSERT_TRUE(pb[1].seen);
    TEST_ASSERT_EQUAL(2, pb[1].box);
    TEST_ASSERT_EQUAL(1, s_store.state.new_today);
}

// 每日新词上限是全局的: 换词库后不重新给名额
void test_new_word_limit_is_global() {
    resetStore();
    s_store.state.new_today = 9;
    const char* w[6] = {"a1", "a2", "a3", "a4", "a5", "a6"};
    setWords(w);
    CardProgress p[6];
    store_load_deck(s_store, s_deck, p);
    int q[6];
    TEST_ASSERT_EQUAL(1, study_build_queue(p, ALL, 6, TODAY, 10, s_store.state, 1, q));
}

void test_store_put_updates_existing_and_keeps_order() {
    resetStore();
    store_put(s_store, "b", {2, 5, true});
    store_put(s_store, "a", {1, 3, true});
    store_put(s_store, "c", {1, 3, true});
    store_put(s_store, "B", {4, 9, true});
    TEST_ASSERT_EQUAL(3, s_store.count);
    TEST_ASSERT_EQUAL_STRING("a", s_store.items[0].word);
    TEST_ASSERT_EQUAL_STRING("b", s_store.items[1].word);
    TEST_ASSERT_EQUAL(4, s_store.items[1].box);
    TEST_ASSERT_EQUAL(9, s_store.items[1].due_day);
}

void test_store_full_rejects_new_word() {
    resetStore();
    char w[8];
    for (int i = 0; i < 8; i++) {
        snprintf(w, sizeof(w), "w%d", i);
        TEST_ASSERT_TRUE(store_put(s_store, w, {1, 0, true}));
    }
    TEST_ASSERT_FALSE(store_put(s_store, "extra", {1, 0, true}));
    TEST_ASSERT_TRUE(store_put(s_store, "w3", {2, 0, true}));  // 已有的词照样能更新
}

// 损坏/缺失: 空表; 越界盒子夹到 1..5; 超长单词跳过
void test_store_parse_robust() {
    resetStore();
    store_put(s_store, "old", {2, 1, true});
    TEST_ASSERT_FALSE(store_parse(nullptr, 0, s_store));
    TEST_ASSERT_EQUAL(0, s_store.count);
    TEST_ASSERT_EQUAL(0, s_store.state.day);
    TEST_ASSERT_FALSE(store_parse("{bad", 4, s_store));
    const char* j = R"({"day":5,"words":{"x":{"b":9,"d":7},"y":{"b":0},"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa":{"b":2}}})";
    TEST_ASSERT_TRUE(store_parse(j, strlen(j), s_store));
    TEST_ASSERT_EQUAL(2, s_store.count);
    TEST_ASSERT_EQUAL(5, s_store.items[0].box);
    TEST_ASSERT_EQUAL(7, s_store.items[0].due_day);
    TEST_ASSERT_EQUAL(1, s_store.items[1].box);
    TEST_ASSERT_EQUAL(0, s_store.state.new_today);
}

void test_serialize_too_small_buffer() {
    resetStore();
    store_put(s_store, "word", {2, TODAY, true});
    char tiny[16];
    TEST_ASSERT_EQUAL(0, store_serialize(s_store, tiny, sizeof(tiny)));
}

// 同一词库里重复的词只留第一张, 一天只出一次
void test_unique_words_keeps_first_card() {
    const char* w[6] = {"note", "pal", "Note", "speed", "pal", "note "};
    setWords(w);
    int idx[6] = {0, 1, 2, 3, 4, 5};
    TEST_ASSERT_EQUAL(3, study_unique_words(s_deck, idx, 6));
    TEST_ASSERT_EQUAL(0, idx[0]);
    TEST_ASSERT_EQUAL(1, idx[1]);
    TEST_ASSERT_EQUAL(3, idx[2]);
}

void test_counts_for_done_page() {
    s_prog[0] = {2, TODAY + 1, true};
    s_prog[1] = {3, TODAY + 2, true};
    s_prog[2] = {1, TODAY, true};
    TEST_ASSERT_EQUAL(2, study_count_due(s_prog, ALL, 6, TODAY + 1));
    TEST_ASSERT_EQUAL(3, study_count_unseen(s_prog, ALL, 6));
}

// 完成页盒子分布: 没学过的卡 (默认 box 1) 单独计入 [0], 不算进盒 1
void test_box_counts_keep_unseen_out_of_box1() {
    s_prog[0] = {1, TODAY, true};
    s_prog[1] = {2, TODAY + 1, true};
    s_prog[2] = {5, TODAY + 14, true};
    int counts[6];
    study_count_boxes(s_prog, ALL, 6, counts);
    TEST_ASSERT_EQUAL(3, counts[0]);
    TEST_ASSERT_EQUAL(1, counts[1]);
    TEST_ASSERT_EQUAL(1, counts[2]);
    TEST_ASSERT_EQUAL(0, counts[3]);
    TEST_ASSERT_EQUAL(0, counts[4]);
    TEST_ASSERT_EQUAL(1, counts[5]);
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
    RUN_TEST(test_word_key_trims_and_lowercases);
    RUN_TEST(test_store_roundtrip);
    RUN_TEST(test_progress_shared_across_decks);
    RUN_TEST(test_new_word_limit_is_global);
    RUN_TEST(test_store_put_updates_existing_and_keeps_order);
    RUN_TEST(test_store_full_rejects_new_word);
    RUN_TEST(test_store_parse_robust);
    RUN_TEST(test_serialize_too_small_buffer);
    RUN_TEST(test_unique_words_keeps_first_card);
    RUN_TEST(test_counts_for_done_page);
    RUN_TEST(test_box_counts_keep_unseen_out_of_box1);
    return UNITY_END();
}
