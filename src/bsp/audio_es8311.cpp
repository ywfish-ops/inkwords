#include "audio_es8311.h"
#include "board_config.h"
#include <Wire.h>
#include <driver/i2s.h>
#include <esp_log.h>
#include <math.h>

static const char* TAG = "ES8311";
static const i2s_port_t I2S_PORT = I2S_NUM_0;

bool AudioEs8311::writeReg(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(I2C_ADDR_ES8311);
    Wire.write(reg);
    Wire.write(val);
    return Wire.endTransmission() == 0;
}

bool AudioEs8311::readReg(uint8_t reg, uint8_t& val) {
    Wire.beginTransmission(I2C_ADDR_ES8311);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((uint8_t)I2C_ADDR_ES8311, (uint8_t)1) != 1) return false;
    val = Wire.read();
    return true;
}

bool AudioEs8311::updateReg(uint8_t reg, uint8_t clear_mask, uint8_t set_bits) {
    uint8_t val = 0;
    if (!readReg(reg, val)) return false;
    return writeReg(reg, (val & ~clear_mask) | set_bits);
}

// 依次对应 esp_codec_dev es8311: open -> config_fmt(I2S) -> set_bits(16) -> config_sample -> start(DAC)
bool AudioEs8311::initCodec() {
    bool ok = true;

    // open (slave mode, 使用 MCLK 引脚, 时钟不反相)
    ok &= writeReg(0x0D, 0xFA);
    ok &= writeReg(0x44, 0x08);  // 增强 I2C 抗噪, 官方驱动写两次以防首次失败
    ok &= writeReg(0x44, 0x08);
    ok &= writeReg(0x01, 0x30);
    ok &= writeReg(0x02, 0x00);
    ok &= writeReg(0x03, 0x10);
    ok &= writeReg(0x16, 0x24);
    ok &= writeReg(0x04, 0x10);
    ok &= writeReg(0x05, 0x00);
    ok &= writeReg(0x0B, 0x00);
    ok &= writeReg(0x0C, 0x00);
    ok &= writeReg(0x10, 0x1F);
    ok &= writeReg(0x11, 0x7F);
    ok &= writeReg(0x00, 0x80);  // slave: bit6 = 0
    ok &= writeReg(0x01, 0x3F);  // MCLK 来自引脚: bit7 = 0, 不反相: bit6 = 0
    ok &= updateReg(0x06, 0x20, 0x00);
    ok &= writeReg(0x13, 0x10);
    ok &= writeReg(0x1B, 0x0A);
    ok &= writeReg(0x1C, 0x6A);
    ok &= writeReg(0x44, 0x58);

    // config_fmt: 标准 I2S; set_bits_per_sample: 16 bit
    ok &= updateReg(0x09, 0x03, 0x0C);
    ok &= updateReg(0x0A, 0x03, 0x0C);

    // config_sample: MCLK 6.144MHz / 24kHz -> pre_div 1, pre_multi x1, adc/dac_div 1,
    // fs_mode 0, lrck 0x00FF, bclk_div 4, adc/dac_osr 0x10
    ok &= updateReg(0x02, 0xF8, 0x00);
    ok &= writeReg(0x05, 0x00);
    ok &= updateReg(0x03, 0x7F, 0x10);
    ok &= updateReg(0x04, 0x7F, 0x10);
    ok &= updateReg(0x07, 0x3F, 0x00);
    ok &= writeReg(0x08, 0xFF);
    ok &= updateReg(0x06, 0x1F, 0x03);

    // start (DAC)
    ok &= writeReg(0x00, 0x80);
    ok &= writeReg(0x01, 0x3F);
    ok &= updateReg(0x09, 0x40, 0x00);
    ok &= updateReg(0x0A, 0x40, 0x00);
    ok &= writeReg(0x17, 0xBF);
    ok &= writeReg(0x0E, 0x02);
    ok &= writeReg(0x12, 0x00);
    ok &= writeReg(0x14, 0x1A);
    ok &= updateReg(0x14, 0x40, 0x00);
    ok &= writeReg(0x0D, 0x01);
    ok &= writeReg(0x15, 0x40);
    ok &= writeReg(0x37, 0x08);
    ok &= writeReg(0x45, 0x00);

    // 取消 DAC 静音
    ok &= updateReg(0x31, 0x60, 0x00);
    return ok;
}

bool AudioEs8311::begin(uint32_t sample_rate) {
    if (_ready) return true;
    _sample_rate = sample_rate;

    pinMode(AUDIO_PA_CTRL_PIN, OUTPUT);
    setAmp(false);

    Wire.beginTransmission(I2C_ADDR_ES8311);
    if (Wire.endTransmission() != 0) {
        Serial.printf("[ES8311] not detected at 0x%02X\n", I2C_ADDR_ES8311);
        return false;
    }
    uint8_t id1 = 0, id2 = 0;
    readReg(0xFD, id1);
    readReg(0xFE, id2);
    Serial.printf("[ES8311] chip id 0x%02X%02X (expect 0x8311)\n", id1, id2);

    // I2S 需先输出 MCLK, ES8311 才能按其时钟工作
    i2s_config_t cfg = {};
    cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
    cfg.sample_rate = sample_rate;
    cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    cfg.intr_alloc_flags = 0;
    cfg.dma_buf_count = 6;
    cfg.dma_buf_len = 240;
    cfg.use_apll = false;
    cfg.tx_desc_auto_clear = true;
    cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;

    esp_err_t err = i2s_driver_install(I2S_PORT, &cfg, 0, nullptr);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2s_driver_install failed: %s", esp_err_to_name(err));
        return false;
    }

    i2s_pin_config_t pins = {};
    pins.mck_io_num = AUDIO_I2S_MCLK_PIN;
    pins.bck_io_num = AUDIO_I2S_BCLK_PIN;
    pins.ws_io_num = AUDIO_I2S_WS_PIN;
    pins.data_out_num = AUDIO_I2S_DOUT_PIN;
    pins.data_in_num = I2S_PIN_NO_CHANGE;
    err = i2s_set_pin(I2S_PORT, &pins);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2s_set_pin failed: %s", esp_err_to_name(err));
        i2s_driver_uninstall(I2S_PORT);
        return false;
    }
    i2s_zero_dma_buffer(I2S_PORT);

    if (!initCodec()) {
        ESP_LOGE(TAG, "ES8311 register init failed");
        i2s_driver_uninstall(I2S_PORT);
        return false;
    }

    _ready = true;
    setVolume(100);  // 0 dB: 发音录音平均约 -20 dBFS, 再衰减会偏小
    return true;
}

void AudioEs8311::setVolume(uint8_t percent) {
    if (!_ready) return;
    if (percent > 100) percent = 100;
    // 寄存器 0x32: 0x00 = -95.5dB, 每级 0.5dB, 0xBF = 0dB
    float db = -49.5f + 0.495f * percent;
    int reg = (int)lroundf((db + 95.5f) * 2.0f);
    if (percent == 0) reg = 0;
    writeReg(0x32, (uint8_t)reg);
}

void AudioEs8311::setAmp(bool on) {
    digitalWrite(AUDIO_PA_CTRL_PIN, on ? HIGH : LOW);
}

void AudioEs8311::writeStereo(const int16_t* mono, size_t count) {
    int16_t frame[2 * 128];
    while (count > 0) {
        size_t n = count > 128 ? 128 : count;
        for (size_t i = 0; i < n; i++) {
            frame[2 * i] = mono[i];
            frame[2 * i + 1] = mono[i];
        }
        size_t written = 0;
        i2s_write(I2S_PORT, frame, n * 2 * sizeof(int16_t), &written, portMAX_DELAY);
        mono += n;
        count -= n;
    }
}

// 功放上电需要稳定时间, 打开后先送一段静音, 否则单词开头的弱辅音会被吞掉
static const uint32_t AMP_SETTLE_MS = 150;

void AudioEs8311::beginStream() {
    if (!_ready) return;
    setAmp(true);
    static const int16_t silence[240] = {0};
    size_t remaining = (size_t)_sample_rate * AMP_SETTLE_MS / 1000;
    while (remaining > 0) {
        size_t n = remaining > 240 ? 240 : remaining;
        writeStereo(silence, n);
        remaining -= n;
    }
}

void AudioEs8311::writeMono(const int16_t* samples, size_t count) {
    if (_ready && samples && count > 0) writeStereo(samples, count);
}

// 补一段静音把 DMA 中的尾部数据推出去, 再关功放, 避免截掉结尾或产生爆音
void AudioEs8311::endStream() {
    if (!_ready) return;
    static const int16_t silence[240] = {0};
    for (int i = 0; i < 4; i++) writeStereo(silence, 240);
    setAmp(false);
}

void AudioEs8311::playTone(uint32_t freq_hz, uint32_t duration_ms) {
    if (!_ready) return;
    const size_t chunk = 240;
    int16_t buf[chunk];
    size_t total = (size_t)_sample_rate * duration_ms / 1000;
    float phase = 0.0f;
    const float step = 2.0f * (float)M_PI * freq_hz / _sample_rate;

    beginStream();
    for (size_t done = 0; done < total; done += chunk) {
        size_t n = (total - done) < chunk ? (total - done) : chunk;
        for (size_t i = 0; i < n; i++) {
            buf[i] = (int16_t)(sinf(phase) * 8000.0f);
            phase += step;
            if (phase > 2.0f * (float)M_PI) phase -= 2.0f * (float)M_PI;
        }
        writeStereo(buf, n);
    }
    endStream();
}
