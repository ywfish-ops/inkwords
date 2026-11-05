#include "leitner.h"
#include <ArduinoJson.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

bool progress_parse(const char* json, size_t len, const Deck& deck, CardProgress* progress, StudyState& state) {
    for (int i = 0; i < deck.count; i++) progress[i] = {1, 0, false};
    state = {0, 0};
    if (!json || len == 0) return false;

    JsonDocument doc;
    if (deserializeJson(doc, json, len)) return false;
    state.day = doc["day"] | 0;
    state.new_today = doc["new_today"] | 0;
    JsonObjectConst cards = doc["cards"].as<JsonObjectConst>();
    for (int i = 0; i < deck.count; i++) {
        JsonObjectConst c = cards[deck.cards[i].key];
        if (c.isNull()) continue;
        int box = c["b"] | 1;
        progress[i].box = (uint8_t)(box < 1 ? 1 : box > LEITNER_BOX_COUNT ? LEITNER_BOX_COUNT : box);
        progress[i].due_day = c["d"] | 0;
        progress[i].seen = true;
    }
    return true;
}

// key 最长 31 字节, 一条约 60 字节
size_t progress_json_capacity(int card_count) { return 64 + (size_t)card_count * 64; }

// 往 out 追加格式化文本; 超出后 n 停在 out_len, 调用方据此判断缓冲不够
static void append(char* out, size_t out_len, size_t& n, const char* fmt, ...) {
    if (n >= out_len) return;
    va_list ap;
    va_start(ap, fmt);
    int w = vsnprintf(out + n, out_len - n, fmt, ap);
    va_end(ap);
    n = (w < 0 || (size_t)w >= out_len - n) ? out_len : n + (size_t)w;
}

size_t progress_serialize(const Deck& deck, const CardProgress* progress, const StudyState& state,
                          char* out, size_t out_len) {
    size_t n = 0;
    append(out, out_len, n, "{\"day\":%ld,\"new_today\":%d,\"cards\":{", (long)state.day, state.new_today);
    bool first = true;
    for (int i = 0; i < deck.count; i++) {
        if (!progress[i].seen) continue;
        append(out, out_len, n, "%s\n\"%s\":{\"b\":%d,\"d\":%ld}", first ? "" : ",", deck.cards[i].key,
               progress[i].box, (long)progress[i].due_day);
        first = false;
    }
    append(out, out_len, n, "}}\n");
    return n < out_len ? n : 0;
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
