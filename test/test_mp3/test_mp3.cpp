#include <unity.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "mp3_stream.h"

void setUp() {}
void tearDown() {}

// 从仓库自带的示例 TF 卡读 MP3
static uint8_t* readMp3(const char* name, size_t* len) {
    char path[96];
    snprintf(path, sizeof(path), "sample_sdcard/spell/audio/%s", name);
    FILE* f = fopen(path, "rb");
    if (!f) return nullptr;
    fseek(f, 0, SEEK_END);
    *len = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t* buf = (uint8_t*)malloc(*len);
    if (fread(buf, 1, *len, f) != *len) {
        free(buf);
        buf = nullptr;
    }
    fclose(f);
    return buf;
}

// 解码 speech.mp3: 24kHz 单声道, 时长与文件大小 (128kbps) 相符
void test_decode_sample_mp3() {
    size_t size = 0;
    uint8_t* data = readMp3("speech.mp3", &size);
    TEST_ASSERT_NOT_NULL(data);
    Mp3Stream s;
    s.begin(data, size);
    Mp3Frame f;
    long total = 0;
    int frames = 0;
    int peak = 0;
    while (s.next(f)) {
        TEST_ASSERT_EQUAL(24000, f.hz);
        TEST_ASSERT_EQUAL(1, f.channels);
        for (int i = 0; i < f.samples; i++) {
            int v = f.pcm[i] < 0 ? -f.pcm[i] : f.pcm[i];
            if (v > peak) peak = v;
        }
        total += f.samples;
        frames++;
    }
    float seconds = total / 24000.0f;
    float expected = size * 8.0f / 128000.0f;
    TEST_ASSERT_TRUE(frames > 10);
    TEST_ASSERT_FLOAT_WITHIN(0.15f, expected, seconds);
    TEST_ASSERT_TRUE_MESSAGE(peak > 1000, "decoded audio is silent");
    free(data);
}

// 空输入/垃圾数据不崩溃, 直接结束
void test_decode_garbage_ends_cleanly() {
    static const uint8_t junk[64] = {1, 2, 3};
    Mp3Stream s;
    Mp3Frame f;
    s.begin(junk, sizeof(junk));
    TEST_ASSERT_FALSE(s.next(f));
    s.begin(nullptr, 0);
    TEST_ASSERT_FALSE(s.next(f));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_decode_sample_mp3);
    RUN_TEST(test_decode_garbage_ends_cleanly);
    return UNITY_END();
}
