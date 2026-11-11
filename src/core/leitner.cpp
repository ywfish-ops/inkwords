#include "leitner.h"
#include <ArduinoJson.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

void progress_word_key(const char* en, char* out, size_t out_len) {
    if (out_len == 0) return;
    while (*en == ' ') en++;
    size_t n = strlen(en);
    while (n > 0 && en[n - 1] == ' ') n--;
    if (n >= out_len) n = out_len - 1;
    for (size_t i = 0; i < n; i++) out[i] = (char)tolower((unsigned char)en[i]);
    out[n] = '\0';
}

// 二分查找; 找不到时 *at 为应插入的位置
static bool store_find(const ProgressStore& store, const char* key, int* at) {
    int lo = 0, hi = store.count;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        int c = strcmp(store.items[mid].word, key);
        if (c == 0) {
            *at = mid;
            return true;
        }
        if (c < 0) lo = mid + 1;
        else hi = mid;
    }
    *at = lo;
    return false;
}

static bool store_put_key(ProgressStore& store, const char* key, uint8_t box, int32_t due_day) {
    int at;
    if (!store_find(store, key, &at)) {
        if (store.count >= store.capacity) return false;
        memmove(&store.items[at + 1], &store.items[at], (store.count - at) * sizeof(WordProgress));
        store.count++;
        snprintf(store.items[at].word, sizeof(store.items[at].word), "%s", key);
    }
    store.items[at].box = box;
    store.items[at].due_day = due_day;
    return true;
}

bool store_put(ProgressStore& store, const char* en, const CardProgress& p) {
    char key[sizeof(WordProgress::word)];
    progress_word_key(en, key, sizeof(key));
    return store_put_key(store, key, p.box, p.due_day);
}

bool store_parse(const char* json, size_t len, ProgressStore& store) {
    store.count = 0;
    store.state = {0, 0};
    if (!json || len == 0) return false;

    JsonDocument doc;
    if (deserializeJson(doc, json, len)) return false;
    store.state.day = doc["day"] | 0;
    store.state.new_today = doc["new_today"] | 0;
    for (JsonPairConst kv : doc["words"].as<JsonObjectConst>()) {
        char key[sizeof(WordProgress::word)];
        if (strlen(kv.key().c_str()) >= sizeof(key)) continue;
        progress_word_key(kv.key().c_str(), key, sizeof(key));
        int box = kv.value()["b"] | 1;
        box = box < 1 ? 1 : box > LEITNER_BOX_COUNT ? LEITNER_BOX_COUNT : box;
        if (!store_put_key(store, key, (uint8_t)box, kv.value()["d"] | 0)) break;
    }
    return true;
}

void store_load_deck(const ProgressStore& store, const Deck& deck, CardProgress* progress) {
    char key[sizeof(WordProgress::word)];
    for (int i = 0; i < deck.count; i++) {
        progress[i] = {1, 0, false};
        progress_word_key(deck.cards[i].en, key, sizeof(key));
        int at;
        if (store_find(store, key, &at)) progress[i] = {store.items[at].box, store.items[at].due_day, true};
    }
}

// 单词最长 47 字节, 转义后最多翻倍, 一条约 130 字节
size_t store_json_capacity(const ProgressStore& store) { return 64 + (size_t)store.count * 130; }

// 往 out 追加格式化文本; 超出后 n 停在 out_len, 调用方据此判断缓冲不够
static void append(char* out, size_t out_len, size_t& n, const char* fmt, ...) {
    if (n >= out_len) return;
    va_list ap;
    va_start(ap, fmt);
    int w = vsnprintf(out + n, out_len - n, fmt, ap);
    va_end(ap);
    n = (w < 0 || (size_t)w >= out_len - n) ? out_len : n + (size_t)w;
}

size_t store_serialize(const ProgressStore& store, char* out, size_t out_len) {
    size_t n = 0;
    append(out, out_len, n, "{\"day\":%ld,\"new_today\":%d,\"words\":{", (long)store.state.day,
           store.state.new_today);
    for (int i = 0; i < store.count; i++) {
        char esc[2 * sizeof(WordProgress::word)];
        size_t e = 0;
        for (const char* c = store.items[i].word; *c; c++) {
            if (*c == '"' || *c == '\\') esc[e++] = '\\';
            esc[e++] = *c;
        }
        esc[e] = '\0';
        append(out, out_len, n, "%s\n\"%s\":{\"b\":%d,\"d\":%ld}", i ? "," : "", esc, store.items[i].box,
               (long)store.items[i].due_day);
    }
    append(out, out_len, n, "}}\n");
    return n < out_len ? n : 0;
}

int study_unique_words(const Deck& deck, int* idx, int n) {
    char a[sizeof(WordProgress::word)], b[sizeof(WordProgress::word)];
    int kept = 0;
    for (int i = 0; i < n; i++) {
        progress_word_key(deck.cards[idx[i]].en, a, sizeof(a));
        bool dup = false;
        for (int j = 0; j < kept && !dup; j++) {
            progress_word_key(deck.cards[idx[j]].en, b, sizeof(b));
            dup = strcmp(a, b) == 0;
        }
        if (!dup) idx[kept++] = idx[i];
    }
    return kept;
}

bool study_roll_day(StudyState& state, int32_t today) {
    if (state.day == today) return false;
    state.day = today;
    state.new_today = 0;
    return true;
}

void study_grade(CardProgress& p, bool remembered, int32_t today, StudyState& state) {
    if (!p.seen) state.new_today++;
    p.seen = true;
    if (remembered) {
        if (p.box < LEITNER_BOX_COUNT) p.box++;
    } else {
        p.box = 1;
    }
    p.due_day = today + LEITNER_BOX_DAYS[p.box];
}

static uint32_t xorshift(uint32_t& s) {
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
}

static void shuffle(int* a, int n, uint32_t seed) {
    uint32_t s = seed ? seed : 0x9E3779B9u;
    for (int i = n - 1; i > 0; i--) {
        int j = (int)(xorshift(s) % (uint32_t)(i + 1));
        int t = a[i];
        a[i] = a[j];
        a[j] = t;
    }
}

int study_build_queue(const CardProgress* progress, const int* candidates, int n, int32_t today,
                      int new_per_day, const StudyState& state, uint32_t seed, int* out) {
    int new_left = new_per_day == NEW_PER_DAY_UNLIMITED ? n : new_per_day - state.new_today;
    int q = 0;
    for (int i = 0; i < n; i++) {
        const CardProgress& p = progress[candidates[i]];
        if (p.seen) {
            if (p.due_day <= today) out[q++] = candidates[i];
        } else if (new_left > 0) {
            out[q++] = candidates[i];
            new_left--;
        }
    }
    shuffle(out, q, seed);
    return q;
}

int study_build_all(const int* candidates, int n, uint32_t seed, int* out) {
    memcpy(out, candidates, n * sizeof(int));
    shuffle(out, n, seed);
    return n;
}

int study_count_due(const CardProgress* progress, const int* candidates, int n, int32_t day) {
    int c = 0;
    for (int i = 0; i < n; i++) {
        const CardProgress& p = progress[candidates[i]];
        if (p.seen && p.due_day <= day) c++;
    }
    return c;
}

int study_count_unseen(const CardProgress* progress, const int* candidates, int n) {
    int c = 0;
    for (int i = 0; i < n; i++) c += !progress[candidates[i]].seen;
    return c;
}
