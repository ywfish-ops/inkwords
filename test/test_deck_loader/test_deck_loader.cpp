#include <unity.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include "deck_loader.h"

static Deck s_deck;
static char s_err[96];

void setUp() {
    memset(&s_deck, 0, sizeof(s_deck));
    s_err[0] = '\0';
}
void tearDown() { deck_free(s_deck); }

static bool parse(const std::string& json) {
    return deck_parse(json.c_str(), json.size(), s_deck, s_err, sizeof(s_err));
}

static std::string read_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return "";
    std::string s;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) s.append(buf, n);
    fclose(f);
    return s;
}

void test_parses_all_fields() {
    TEST_ASSERT_TRUE(parse(R"({"id":"spbcn_r1","name":"SPBCN R1","count":1,"cards":[
        {"key":"spbcn_r1-0","en":"speech","ipa":"/spi:tʃ/","zh":"演讲","ex":"Give a speech.",
         "ex_blank":"Give a ____.","cat":"vowel_team","def":"a talk","audio":"speech.mp3"}]})"));
    TEST_ASSERT_EQUAL(1, s_deck.count);
    TEST_ASSERT_EQUAL(0, s_deck.skipped);
    TEST_ASSERT_EQUAL_STRING("spbcn_r1", s_deck.id);
    TEST_ASSERT_EQUAL_STRING("SPBCN R1", s_deck.name);
    const WordCard& c = s_deck.cards[0];
    TEST_ASSERT_EQUAL_STRING("spbcn_r1-0", c.key);
    TEST_ASSERT_EQUAL_STRING("speech", c.en);
    TEST_ASSERT_EQUAL_STRING("/spi:tʃ/", c.ipa);
    TEST_ASSERT_EQUAL_STRING("演讲", c.zh);
    TEST_ASSERT_EQUAL_STRING("Give a speech.", c.ex);
    TEST_ASSERT_EQUAL_STRING("Give a ____.", c.ex_blank);
    TEST_ASSERT_EQUAL_STRING("vowel_team", c.cat);
    TEST_ASSERT_EQUAL_STRING("a talk", c.def_en);
    TEST_ASSERT_EQUAL_STRING("speech.mp3", c.audio);
}

void test_missing_optional_fields_become_empty() {
    TEST_ASSERT_TRUE(parse(R"({"id":"sample","cards":[{"key":"sample-0","en":"wrong"}]})"));
    const WordCard& c = s_deck.cards[0];
    TEST_ASSERT_EQUAL_STRING("", c.ipa);
    TEST_ASSERT_EQUAL_STRING("", c.ex_blank);
    TEST_ASSERT_EQUAL_STRING("", c.audio);
    TEST_ASSERT_EQUAL_STRING("", s_deck.name);
}

void test_skips_cards_without_key_or_en() {
    TEST_ASSERT_TRUE(parse(R"({"cards":[{"key":"a-0","en":"one"},{"key":"a-1"},
        {"en":"three"},{"key":"a-3","en":""},{"key":"a-4","en":"five"}]})"));
    TEST_ASSERT_EQUAL(2, s_deck.count);
    TEST_ASSERT_EQUAL(3, s_deck.skipped);
    TEST_ASSERT_EQUAL_STRING("one", s_deck.cards[0].en);
    TEST_ASSERT_EQUAL_STRING("five", s_deck.cards[1].en);
}

void test_wrong_types_are_treated_as_missing() {
    TEST_ASSERT_TRUE(parse(R"({"cards":[{"key":"a-0","en":"one","ipa":5,"zh":null}]})"));
    TEST_ASSERT_EQUAL_STRING("", s_deck.cards[0].ipa);
    TEST_ASSERT_EQUAL_STRING("", s_deck.cards[0].zh);
}

void test_broken_json_fails_with_reason() {
    TEST_ASSERT_FALSE(parse(R"({"cards":[{"key":"a-0",)"));
    TEST_ASSERT_NOT_NULL(strstr(s_err, "JSON"));
    TEST_ASSERT_NULL(s_deck.cards);
}

void test_missing_or_empty_cards_fails() {
    TEST_ASSERT_FALSE(parse(R"({"id":"x"})"));
    TEST_ASSERT_EQUAL_STRING("缺少 cards 数组", s_err);
    TEST_ASSERT_FALSE(parse(R"({"cards":[]})"));
    TEST_ASSERT_EQUAL_STRING("词库没有卡片", s_err);
    TEST_ASSERT_FALSE(parse(R"({"cards":[{"key":"a-0"}]})"));
    TEST_ASSERT_EQUAL_STRING("没有有效卡片", s_err);
    TEST_ASSERT_FALSE(parse(""));
}

void test_long_fields_truncate_on_utf8_boundary() {
    std::string zh;
    for (int i = 0; i < 40; i++) zh += "汉";  // 120 字节 > zh[96]
    TEST_ASSERT_TRUE(parse(R"({"cards":[{"key":"a-0","en":"x","zh":")" + zh + R"("}]})"));
    const char* got = s_deck.cards[0].zh;
    TEST_ASSERT_EQUAL(93, strlen(got));  // 31 个完整汉字, 第 32 个放不下
    TEST_ASSERT_EQUAL(0, strncmp(got, zh.c_str(), 93));
}

void test_copy_utf8_edges() {
    char buf[4];
    copy_utf8(buf, sizeof(buf), "abc");
    TEST_ASSERT_EQUAL_STRING("abc", buf);
    copy_utf8(buf, sizeof(buf), "abcd");
    TEST_ASSERT_EQUAL_STRING("abc", buf);
    copy_utf8(buf, sizeof(buf), "a汉");  // 4 字节放不下 -> 只留 a
    TEST_ASSERT_EQUAL_STRING("a", buf);
    copy_utf8(buf, sizeof(buf), nullptr);
    TEST_ASSERT_EQUAL_STRING("", buf);
}

static const char* MANIFEST = R"([
    {"id":"spbcn_r1","name":"SPBCN R1 单音节","file":"spbcn_r1.json"},
    {"id":"broken"},
    {"id":"sample","file":"sample.json"}])";

void test_manifest_list_skips_invalid_and_makes_labels() {
    DeckEntry d[MAX_DECKS];
    TEST_ASSERT_EQUAL(2, manifest_list(MANIFEST, strlen(MANIFEST), d, MAX_DECKS));
    TEST_ASSERT_EQUAL_STRING("spbcn_r1", d[0].id);
    TEST_ASSERT_EQUAL_STRING("spbcn_r1.json", d[0].file);
    TEST_ASSERT_EQUAL_STRING("SPBCN R1", d[0].label);
    TEST_ASSERT_EQUAL_STRING("sample", d[1].id);
    TEST_ASSERT_EQUAL_STRING("sample", d[1].label);  // 无 name 时用 id
    TEST_ASSERT_EQUAL(1, manifest_list(MANIFEST, strlen(MANIFEST), d, 1));
}

void test_manifest_list_broken_or_empty() {
    DeckEntry d[MAX_DECKS];
    TEST_ASSERT_EQUAL(0, manifest_list("[", 1, d, MAX_DECKS));
    TEST_ASSERT_EQUAL(0, manifest_list("[]", 2, d, MAX_DECKS));
    TEST_ASSERT_EQUAL(0, manifest_list("{}", 2, d, MAX_DECKS));
}

void test_deck_index_finds_or_falls_back() {
    DeckEntry d[MAX_DECKS];
    int n = manifest_list(MANIFEST, strlen(MANIFEST), d, MAX_DECKS);
    TEST_ASSERT_EQUAL(1, deck_index(d, n, "sample"));
    TEST_ASSERT_EQUAL(0, deck_index(d, n, "no_such_deck"));
    TEST_ASSERT_EQUAL(0, deck_index(d, n, nullptr));
    TEST_ASSERT_EQUAL(-1, deck_index(d, 0, "sample"));
}

static UserSettings default_settings() {
    UserSettings s;
    memset(&s, 0, sizeof(s));
    strcpy(s.deck_id, "spbcn_r1");
    strcpy(s.cat_filter, "all");
    return s;
}

void test_selection_overrides_deck_and_cat() {
    UserSettings s = default_settings();
    const char* j = R"({"deck":"spbcn_r3","cat":"schwa"})";
    selection_parse(j, strlen(j), s);
    TEST_ASSERT_EQUAL_STRING("spbcn_r3", s.deck_id);
    TEST_ASSERT_EQUAL_STRING("schwa", s.cat_filter);
}

void test_selection_keeps_defaults_when_missing_or_broken() {
    UserSettings s = default_settings();
    selection_parse(nullptr, 0, s);
    selection_parse("{bad", 4, s);
    const char* partial = R"({"deck":"spbcn_r3"})";
    selection_parse(partial, strlen(partial), s);
    TEST_ASSERT_EQUAL_STRING("spbcn_r3", s.deck_id);
    TEST_ASSERT_EQUAL_STRING("all", s.cat_filter);
    const char* empty = R"({"deck":"","cat":3})";
    selection_parse(empty, strlen(empty), s);
    TEST_ASSERT_EQUAL_STRING("spbcn_r3", s.deck_id);
    TEST_ASSERT_EQUAL_STRING("all", s.cat_filter);
}

void test_filter_by_category_or_fallback_to_all() {
    TEST_ASSERT_TRUE(parse(R"({"cards":[{"key":"a-0","en":"a","cat":"plain"},
        {"key":"a-1","en":"b","cat":"silent"},{"key":"a-2","en":"c","cat":"plain"}]})"));
    int idx[3];
    TEST_ASSERT_EQUAL(2, deck_filter(s_deck, "plain", idx));
    TEST_ASSERT_EQUAL(0, idx[0]);
    TEST_ASSERT_EQUAL(2, idx[1]);
    TEST_ASSERT_EQUAL(3, deck_filter(s_deck, "all", idx));
    TEST_ASSERT_EQUAL(3, deck_filter(s_deck, "", idx));
    TEST_ASSERT_EQUAL(3, deck_filter(s_deck, nullptr, idx));
    TEST_ASSERT_EQUAL(3, deck_filter(s_deck, "no_such_cat", idx));
    TEST_ASSERT_EQUAL(2, idx[2]);
}

void test_short_label() {
    char out[16];
    deck_short_label("SPBCN R1 单音节", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("SPBCN R1", out);
    deck_short_label("Sample 5", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("Sample 5", out);
    deck_short_label("单词", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("", out);
    deck_short_label("ABCDEFGHIJKLMNOPQRST", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("ABCDEFGHIJKLMNO", out);
    deck_short_label(nullptr, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("", out);
}

void test_selection_serialize_roundtrip() {
    UserSettings s = default_settings();
    strcpy(s.deck_id, "spbcn_r3");
    char buf[64];
    size_t n = selection_serialize(s, buf, sizeof(buf));
    TEST_ASSERT_TRUE(n > 0);
    UserSettings back = default_settings();
    selection_parse(buf, n, back);
    TEST_ASSERT_EQUAL_STRING("spbcn_r3", back.deck_id);
    TEST_ASSERT_EQUAL_STRING("all", back.cat_filter);
    char tiny[8];
    TEST_ASSERT_EQUAL(0, selection_serialize(s, tiny, sizeof(tiny)));
}

// 仓库自带的示例 TF 卡: manifest 列出的词库能解析、词数与 manifest 一致、无截断
void test_sample_sdcard_decks() {
    std::string manifest = read_file("sample_sdcard/spell/decks/manifest.json");
    TEST_ASSERT_FALSE(manifest.empty());
    DeckEntry entries[4];
    int n = manifest_list(manifest.c_str(), manifest.size(), entries, 4);
    TEST_ASSERT_EQUAL(1, n);
    TEST_ASSERT_EQUAL_STRING("sample", entries[0].id);

    char path[96];
    snprintf(path, sizeof(path), "sample_sdcard/spell/decks/%s", entries[0].file);
    std::string json = read_file(path);
    TEST_ASSERT_FALSE_MESSAGE(json.empty(), path);
    Deck d;
    TEST_ASSERT_TRUE_MESSAGE(deck_parse(json.c_str(), json.size(), d, s_err, sizeof(s_err)), s_err);
    TEST_ASSERT_EQUAL_STRING("sample", d.id);
    TEST_ASSERT_EQUAL(5, d.count);
    TEST_ASSERT_EQUAL(0, d.skipped);
    for (int k = 0; k < d.count; k++) {
        TEST_ASSERT_TRUE(strlen(d.cards[k].def_en) < sizeof(d.cards[k].def_en) - 1);
        TEST_ASSERT_TRUE(strlen(d.cards[k].ex) < sizeof(d.cards[k].ex) - 1);
        TEST_ASSERT_TRUE(strstr(d.cards[k].ex_blank, "____") != nullptr);
    }
    deck_free(d);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_parses_all_fields);
    RUN_TEST(test_missing_optional_fields_become_empty);
    RUN_TEST(test_skips_cards_without_key_or_en);
    RUN_TEST(test_wrong_types_are_treated_as_missing);
    RUN_TEST(test_broken_json_fails_with_reason);
    RUN_TEST(test_missing_or_empty_cards_fails);
    RUN_TEST(test_long_fields_truncate_on_utf8_boundary);
    RUN_TEST(test_copy_utf8_edges);
    RUN_TEST(test_manifest_list_skips_invalid_and_makes_labels);
    RUN_TEST(test_manifest_list_broken_or_empty);
    RUN_TEST(test_deck_index_finds_or_falls_back);
    RUN_TEST(test_selection_overrides_deck_and_cat);
    RUN_TEST(test_selection_keeps_defaults_when_missing_or_broken);
    RUN_TEST(test_filter_by_category_or_fallback_to_all);
    RUN_TEST(test_short_label);
    RUN_TEST(test_selection_serialize_roundtrip);
    RUN_TEST(test_sample_sdcard_decks);
    return UNITY_END();
}
