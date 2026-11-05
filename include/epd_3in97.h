#ifndef EPD_3IN97_H
#define EPD_3IN97_H

#include <stdint.h>
#include <stdbool.h>
#include <Arduino.h>
#include <driver/spi_master.h>
#include <driver/gpio.h>
#include "board_config.h"

/**
 * @file epd_3in97.h
 * @brief 微雪 Waveshare ESP32-S3-ePaper-3.97 墨水屏底层 SPI 驱动 (SSD1677, 800x480)
 */

class Epd3in97 {
public:
    static Epd3in97& getInstance() {
        static Epd3in97 instance;
        return instance;
    }

    // 初始化 SPI 引脚并为 48KB 显存申请 PSRAM 内存
    bool begin();

    // 墨水屏全刷初始化
    void initFull();

    // 墨水屏局刷初始化 (快速刷新, 无闪烁)
    void initPartial();

    // 清空显存 (0xFF 为纯白, 0x00 为纯黑)
    void clearBuffer(uint8_t color = 0xFF);

    // 全刷将当前显存送往墨水屏 (有黑白反转闪烁，消残影, 0xF7)
    void displayFull();

    // 快速刷新 (Fast update, 约 1.5s, 0xD7)
    void displayFast();

    // 写入当前显存作为基准底图
    void displayPartBaseImage();

    // 局部/快速更新
    void displayPart();

    // 进入深度休眠低功耗 (下次 displayFull/displayPart 时自动复位唤醒)
    void sleep();
    bool isAsleep() const { return _asleep; }

    // 获取显存指针 (大小为 EPD_BUFFER_SIZE = 48000 字节)
    uint8_t* getBuffer() { return _buffer; }
    size_t getBufferSize() const { return EPD_BUFFER_SIZE; }

    uint16_t getNativeWidth() const { return EPD_NATIVE_WIDTH; }   // 800
    uint16_t getNativeHeight() const { return EPD_NATIVE_HEIGHT; } // 480

    // 画单个物理像素 (0 <= x < 800, 0 <= y < 480)
    void drawPixel(int x, int y, bool black);

    // 等待 BUSY 引脚变低 (忙碌状态为 HIGH，空闲为 LOW)
    void waitBusy(uint32_t timeout_ms = 8000);

private:
    Epd3in97() = default;

    void spiGpioInit();
    void spiPortInit();
    void sendCommand(uint8_t cmd);
    void sendData(uint8_t data);
    void sendBytes(const uint8_t* data, int len);
    void setWindows(uint16_t xStart, uint16_t yStart, uint16_t xEnd, uint16_t yEnd);
    void setCursor(uint16_t xStart, uint16_t yStart);
    void resetRamCursor();
    void wakeIfAsleep();

    spi_device_handle_t _spi = nullptr;
    uint8_t* _buffer = nullptr;
    uint8_t* _prev = nullptr;   // 屏幕上当前显示的帧 (休眠后恢复局刷差分基准 0x26)
    bool _asleep = false;
    bool _initialized = false;
    bool _in_partial_mode = false;
};

#endif // EPD_3IN97_H
