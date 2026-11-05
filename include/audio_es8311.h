#ifndef AUDIO_ES8311_H
#define AUDIO_ES8311_H

#include <stdint.h>
#include <stddef.h>
#include <Arduino.h>

/**
 * @file audio_es8311.h
 * @brief ES8311 音频编解码器 + I2S 输出 + 功放控制 (仅播放)
 *        寄存器流程参照乐鑫 esp_codec_dev 的 es8311 驱动 (ESP 为 I2S 主机, ES8311 从机, MCLK = 256 * fs)
 */

class AudioEs8311 {
public:
    static AudioEs8311& getInstance() {
        static AudioEs8311 instance;
        return instance;
    }

    // 初始化 ES8311 (I2C 须已由 Wire.begin 初始化) 与 I2S; 失败返回 false
    bool begin(uint32_t sample_rate);

    // 音量 0 ~ 100 (映射到 -49.5 ~ 0 dB)
    void setVolume(uint8_t percent);

    // 分段播放单声道 16-bit PCM (采样率须与 begin 一致): beginStream 打开功放, writeMono 可多次调用, endStream 冲刷尾部并关功放
    void beginStream();
    void writeMono(const int16_t* samples, size_t count);
    void endStream();

    // 阻塞播放正弦测试音
    void playTone(uint32_t freq_hz, uint32_t duration_ms);

    bool isReady() const { return _ready; }

private:
    AudioEs8311() = default;

    bool writeReg(uint8_t reg, uint8_t val);
    bool readReg(uint8_t reg, uint8_t& val);
    bool updateReg(uint8_t reg, uint8_t clear_mask, uint8_t set_bits);
    bool initCodec();
    void writeStereo(const int16_t* mono, size_t count);
    void setAmp(bool on);

    bool     _ready = false;
    uint32_t _sample_rate = 0;
};

#endif // AUDIO_ES8311_H
