#include "word_player.h"
#include "board_config.h"
#include "mp3_stream.h"
#include "sdcard_bsp.h"
#include "device_log.h"

// minimp3 每帧解码在栈上使用约 16KB scratch
static const uint32_t PLAYER_STACK_BYTES = 32 * 1024;

struct PlayRequest {
    char file[48];
};

bool WordPlayer::begin(AudioEs8311& audio) {
    if (_queue) return true;
    if (!audio.isReady()) return false;
    _audio = &audio;
    _queue = xQueueCreate(1, sizeof(PlayRequest));
    if (!_queue) return false;
    // Arduino loop 运行在 core 1, 播放放到 core 0 避免与刷屏争抢
    BaseType_t ok = xTaskCreatePinnedToCore(taskEntry, "word_player", PLAYER_STACK_BYTES, this, 5, nullptr, 0);
    return ok == pdPASS;
}

bool WordPlayer::play(const char* file) {
    if (!file || !*file || !_queue) return false;
    PlayRequest req = {};
    strncpy(req.file, file, sizeof(req.file) - 1);
    xQueueOverwrite(_queue, &req);
    return true;
}

void WordPlayer::taskEntry(void* arg) {
    static_cast<WordPlayer*>(arg)->run();
}

// 读入请求的 MP3 (PSRAM), 失败返回 nullptr
static uint8_t* loadMp3(const PlayRequest& req, size_t* len) {
    char path[96];
    snprintf(path, sizeof(path), SPELL_ROOT_DIR "/audio/%s", req.file);
    uint8_t* data = (uint8_t*)sdcard_read_file(path, len);
    if (!data) LOGW("AUDIO", "读不到 audio/%s", req.file);
    return data;
}

void WordPlayer::run() {
    Mp3Stream stream;
    PlayRequest req;
    for (;;) {
        xQueueReceive(_queue, &req, portMAX_DELAY);
        size_t len = 0;
        uint8_t* data = loadMp3(req, &len);
        if (!data) continue;

        _playing = true;
        Serial.printf("[AUDIO] 播放: %s\n", req.file);
        uint32_t t0 = millis();
        uint32_t samples = 0;
        stream.begin(data, len);
        _audio->beginStream();

        Mp3Frame f;
        while (stream.next(f)) {
            // 播放中收到新请求: 直接切换到新单词 (功放保持开启)
            if (xQueueReceive(_queue, &req, 0) == pdTRUE) {
                Serial.printf("[AUDIO] 改播: %s\n", req.file);
                free(data);
                data = loadMp3(req, &len);
                if (!data) break;
                stream.begin(data, len);
                continue;
            }
            if (f.hz != (int)AUDIO_SAMPLE_RATE || f.channels != 1) {
                LOGE("AUDIO", "不支持的格式 %d Hz / %d ch: %s", f.hz, f.channels, req.file);
                break;
            }
            _audio->writeMono(f.pcm, f.samples);
            samples += f.samples;
        }
        _audio->endStream();
        free(data);
        _playing = false;
        Serial.printf("[AUDIO] 完成: 音频 %lu ms, 实际 %lu ms, 剩余栈 %u bytes\n",
                      (unsigned long)(samples * 1000UL / AUDIO_SAMPLE_RATE), (unsigned long)(millis() - t0),
                      (unsigned)uxTaskGetStackHighWaterMark(nullptr));
    }
}
