#include "deck_loader.h"
#include <ArduinoJson.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef ARDUINO
#include <esp_heap_caps.h>
static void* big_alloc(size_t n) { return heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); }
static void* big_realloc(void* p, size_t n) { return heap_caps_realloc(p, n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); }
#else
static void* big_alloc(size_t n) { return malloc(n); }
static void* big_realloc(void* p, size_t n) { return realloc(p, n); }
#endif

// 一套词库解析后约 150KB, 让 ArduinoJson 的内存池也走 PSRAM, 不占片内 SRAM
class BigAllocator : public ArduinoJson::Allocator {
public:
    void* allocate(size_t size) override { return big_alloc(size); }
    void deallocate(void* p) override { free(p); }
    void* reallocate(void* p, size_t n) override { return big_realloc(p, n); }
    static BigAllocator* instance() {
        static BigAllocator a;
        return &a;
    }
};

void copy_utf8(char* dst, size_t cap, const char* src) {
    if (cap == 0) return;
    if (!src) src = "";
    size_t n = strlen(src);
    if (n >= cap) {
        n = cap - 1;
        // 回退到字符起始字节 (非 10xxxxxx)
        while (n > 0 && ((unsigned char)src[n] & 0xC0) == 0x80) n--;
    }
    memcpy(dst, src, n);
    dst[n] = '\0';
}

static void set_err(char* err, size_t err_len, const char* msg) {
    if (err && err_len) copy_utf8(err, err_len, msg);
}

bool deck_parse(const char* json, size_t len, Deck& out, char* err, size_t err_len) {
    memset(&out, 0, sizeof(out));

    JsonDocument doc(BigAllocator::instance());
    DeserializationError e = deserializeJson(doc, json, len);
    if (e) {
        char msg[64];
        snprintf(msg, sizeof(msg), "JSON 解析失败: %s", e.c_str());
        set_err(err, err_len, msg);
        return false;
    }
    JsonArrayConst arr = doc["cards"].as<JsonArrayConst>();
    if (arr.isNull()) {
        set_err(err, err_len, "缺少 cards 数组");
        return false;
    }
    size_t total = arr.size();
    if (total == 0) {
        set_err(err, err_len, "词库没有卡片");
        return false;
    }

    WordCard* cards = (WordCard*)big_alloc(total * sizeof(WordCard));
    if (!cards) {
        set_err(err, err_len, "内存不足");
        return false;
    }

    int count = 0;
    for (JsonObjectConst c : arr) {
        const char* key = c["key"] | "";
        const char* en = c["en"] | "";
        if (!*key || !*en) {
            out.skipped++;
            continue;
        }
        WordCard& w = cards[count++];
        copy_utf8(w.key, sizeof(w.key), key);
        copy_utf8(w.en, sizeof(w.en), en);
        copy_utf8(w.ipa, sizeof(w.ipa), c["ipa"] | "");
        copy_utf8(w.zh, sizeof(w.zh), c["zh"] | "");
        copy_utf8(w.ex, sizeof(w.ex), c["ex"] | "");
        copy_utf8(w.cat, sizeof(w.cat), c["cat"] | "");
        copy_utf8(w.def_en, sizeof(w.def_en), c["def"] | "");
        copy_utf8(w.ex_blank, sizeof(w.ex_blank), c["ex_blank"] | "");
        copy_utf8(w.audio, sizeof(w.audio), c["audio"] | "");
    }
    if (count == 0) {
        free(cards);
        set_err(err, err_len, "没有有效卡片");
        return false;
    }

    copy_utf8(out.id, sizeof(out.id), doc["id"] | "");
    copy_utf8(out.name, sizeof(out.name), doc["name"] | "");
    out.count = count;
    out.cards = cards;
    return true;
}

void deck_free(Deck& deck) {
    free(deck.cards);
    deck.cards = nullptr;
    deck.count = 0;
}

void selection_parse(const char* json, size_t len, UserSettings& settings) {
    if (!json || len == 0) return;
    JsonDocument doc;
    if (deserializeJson(doc, json, len)) return;
    const char* deck = doc["deck"] | "";
    const char* cat = doc["cat"] | "";
    if (*deck) copy_utf8(settings.deck_id, sizeof(settings.deck_id), deck);
    if (*cat) copy_utf8(settings.cat_filter, sizeof(settings.cat_filter), cat);
}

int deck_filter(const Deck& deck, const char* cat, int* out_idx) {
    int n = 0;
    if (cat && *cat && strcmp(cat, "all") != 0) {
        for (int i = 0; i < deck.count; i++) {
            if (strcmp(deck.cards[i].cat, cat) == 0) out_idx[n++] = i;
        }
    }
    if (n == 0) {
        for (int i = 0; i < deck.count; i++) out_idx[i] = i;
        n = deck.count;
    }
    return n;
}

void deck_short_label(const char* name, char* out, size_t out_len) {
    if (out_len == 0) return;
    size_t n = 0;
    if (name) {
        while (name[n] && (unsigned char)name[n] < 0x80) n++;
    }
    while (n > 0 && name[n - 1] == ' ') n--;
    if (n >= out_len) n = out_len - 1;
    memcpy(out, name ? name : "", n);
    out[n] = '\0';
}

int manifest_list(const char* json, size_t len, DeckEntry* out, int max) {
    JsonDocument doc;
    if (deserializeJson(doc, json, len)) return 0;
    int n = 0;
    for (JsonObjectConst d : doc.as<JsonArrayConst>()) {
        if (n >= max) break;
        const char* id = d["id"] | "";
        const char* file = d["file"] | "";
        if (!*id || !*file) continue;
        DeckEntry& e = out[n++];
        copy_utf8(e.id, sizeof(e.id), id);
        copy_utf8(e.file, sizeof(e.file), file);
        deck_short_label(d["name"] | "", e.label, sizeof(e.label));
        if (!*e.label) copy_utf8(e.label, sizeof(e.label), id);
    }
    return n;
}

size_t selection_serialize(const UserSettings& settings, char* out, size_t out_len) {
    JsonDocument doc;
    doc["deck"] = settings.deck_id;
    doc["cat"] = settings.cat_filter;
    if (measureJsonPretty(doc) + 1 > out_len) return 0;
    return serializeJsonPretty(doc, out, out_len);
}

int deck_index(const DeckEntry* decks, int n, const char* id) {
    if (n <= 0) return -1;
    for (int i = 0; i < n; i++) {
        if (id && strcmp(decks[i].id, id) == 0) return i;
    }
    return 0;
}
