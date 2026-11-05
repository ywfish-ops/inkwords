#include "epd_3in97.h"
#include <esp_log.h>
#include <esp_heap_caps.h>
#include <string.h>

static const char* TAG = "EPD_3IN97";

#define EPD_DC_0()    gpio_set_level((gpio_num_t)EPD_DC_PIN, 0)
#define EPD_DC_1()    gpio_set_level((gpio_num_t)EPD_DC_PIN, 1)
#define EPD_CS_0()    gpio_set_level((gpio_num_t)EPD_CS_PIN, 0)
#define EPD_CS_1()    gpio_set_level((gpio_num_t)EPD_CS_PIN, 1)
#define EPD_RST_0()   gpio_set_level((gpio_num_t)EPD_RST_PIN, 0)
#define EPD_RST_1()   gpio_set_level((gpio_num_t)EPD_RST_PIN, 1)

bool Epd3in97::begin() {
    if (_initialized) return true;

    // 1. 分配显存 (优先分配于 PSRAM)
    if (_buffer == nullptr) {
        if (psramFound()) {
            _buffer = (uint8_t*)heap_caps_malloc(EPD_BUFFER_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            ESP_LOGI(TAG, "Allocated 48KB EPD buffer in PSRAM at %p", _buffer);
        }
        if (_buffer == nullptr) {
            _buffer = (uint8_t*)heap_caps_malloc(EPD_BUFFER_SIZE, MALLOC_CAP_8BIT);
            ESP_LOGI(TAG, "Allocated 48KB EPD buffer in Internal RAM at %p", _buffer);
        }
    }

    if (_buffer == nullptr) {
        ESP_LOGE(TAG, "Failed to allocate 48KB EPD buffer!");
        return false;
    }

    if (_prev == nullptr) {
        _prev = (uint8_t*)heap_caps_malloc(EPD_BUFFER_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (_prev == nullptr) {
            _prev = (uint8_t*)heap_caps_malloc(EPD_BUFFER_SIZE, MALLOC_CAP_8BIT);
        }
        if (_prev == nullptr) {
            ESP_LOGE(TAG, "Failed to allocate 48KB EPD previous-frame buffer!");
            return false;
        }
        memset(_prev, 0xFF, EPD_BUFFER_SIZE);
    }

    // 默认全白
    clearBuffer(0xFF);

    // 2. 初始化 GPIO 与 SPI
    spiGpioInit();
    spiPortInit();

    // 3. 全刷初始化
    initFull();

    _initialized = true;
    return true;
}

void Epd3in97::spiGpioInit() {
    gpio_config_t gpio_conf = {};
    gpio_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_conf.mode = GPIO_MODE_OUTPUT;
    gpio_conf.pin_bit_mask = (0x1ULL << EPD_RST_PIN) | (0x1ULL << EPD_DC_PIN) | (0x1ULL << EPD_CS_PIN);
    gpio_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_config(&gpio_conf));

    gpio_conf.mode = GPIO_MODE_INPUT;
    gpio_conf.pin_bit_mask = (0x1ULL << EPD_BUSY_PIN);
    ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_config(&gpio_conf));

    EPD_RST_1();
    EPD_CS_1();
    EPD_DC_1();
}

void Epd3in97::spiPortInit() {
    spi_bus_config_t buscfg = {};
    buscfg.miso_io_num = -1;
    buscfg.mosi_io_num = EPD_MOSI_PIN;
    buscfg.sclk_io_num = EPD_SCK_PIN;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = 4096;

    spi_device_interface_config_t devcfg = {};
    devcfg.spics_io_num = -1; // 软件控制 CS
    devcfg.clock_speed_hz = 20 * 1000 * 1000;  // 20 MHz
    devcfg.mode = 0;                           // SPI mode 0
    devcfg.queue_size = 7;

    esp_err_t ret = spi_bus_initialize((spi_host_device_t)EPD_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "spi_bus_initialize failed: %s", esp_err_to_name(ret));
    }

    ret = spi_bus_add_device((spi_host_device_t)EPD_SPI_HOST, &devcfg, &_spi);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_add_device failed: %s", esp_err_to_name(ret));
    }
}

void Epd3in97::waitBusy(uint32_t timeout_ms) {
    uint32_t t0 = millis();
    int initial_level = gpio_get_level((gpio_num_t)EPD_BUSY_PIN);
    if (initial_level == 1) {
        Serial.printf("[EPD] BUSY is HIGH, waiting (timeout=%lu ms)...\n", timeout_ms);
    }
    while (gpio_get_level((gpio_num_t)EPD_BUSY_PIN) == 1) {
        if (millis() - t0 > timeout_ms) {
            Serial.printf("[EPD] WARNING: waitBusy timed out after %lu ms!\n", timeout_ms);
            break;
        }
        delay(5);
    }
    if (initial_level == 1) {
        Serial.printf("[EPD] BUSY is now LOW (waited %lu ms)\n", millis() - t0);
    }
}

void Epd3in97::sendCommand(uint8_t cmd) {
    EPD_DC_0();
    EPD_CS_0();
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 8;
    t.tx_buffer = &cmd;
    spi_device_polling_transmit(_spi, &t);
    EPD_CS_1();
}

void Epd3in97::sendData(uint8_t data) {
    EPD_DC_1();
    EPD_CS_0();
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 8;
    t.tx_buffer = &data;
    spi_device_polling_transmit(_spi, &t);
    EPD_CS_1();
}

void Epd3in97::sendBytes(const uint8_t* data, int len) {
    EPD_DC_1();
    EPD_CS_0();

    const int MAX_SPI_TRANSFER = 4096;
    int remaining = len;
    int offset = 0;

    while (remaining > 0) {
        int chunk_size = (remaining > MAX_SPI_TRANSFER) ? MAX_SPI_TRANSFER : remaining;
        spi_transaction_t t;
        memset(&t, 0, sizeof(t));
        t.length = 8 * chunk_size;
        t.tx_buffer = data + offset;
        esp_err_t ret = spi_device_polling_transmit(_spi, &t);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "SPI transmit chunk failed: %s", esp_err_to_name(ret));
            break;
        }
        remaining -= chunk_size;
        offset += chunk_size;
    }

    EPD_CS_1();
}

void Epd3in97::initFull() {
    Serial.println("[EPD] initFull: Hardware reset pulse...");
    EPD_RST_1();
    delay(50);
    EPD_RST_0();
    delay(2);
    EPD_RST_1();
    delay(50);

    waitBusy(4000);
    Serial.println("[EPD] initFull: Sending SWRESET (0x12)...");
    sendCommand(0x12);  // SWRESET
    waitBusy(4000);

    Serial.println("[EPD] initFull: Configuring SSD1677 registers...");
    sendCommand(0x18);
    sendData(0x80);

    sendCommand(0x0C);  // Driver output control
    sendData(0xAE);
    sendData(0xC7);
    sendData(0xC3);
    sendData(0xC0);
    sendData(0x80);

    sendCommand(0x01);  // Driver output control (Height = 480)
    sendData((EPD_NATIVE_HEIGHT - 1) % 256);
    sendData((EPD_NATIVE_HEIGHT - 1) / 256);
    sendData(0x02);

    sendCommand(0x3C);  // BorderWaveform
    sendData(0x01);

    sendCommand(0x11);  // Data entry mode
    sendData(0x01);

    sendCommand(0x44);  // Set Ram-X address start/end position (Width = 800)
    sendData(0x00);
    sendData(0x00);
    sendData((EPD_NATIVE_WIDTH - 1) % 256);
    sendData((EPD_NATIVE_WIDTH - 1) / 256);

    sendCommand(0x45);  // Set Ram-Y address start/end position (Height = 480)
    sendData((EPD_NATIVE_HEIGHT - 1) % 256);
    sendData((EPD_NATIVE_HEIGHT - 1) / 256);
    sendData(0x00);
    sendData(0x00);

    sendCommand(0x4E);  // Set RAM x address count to 0
    sendData(0x00);
    sendData(0x00);

    sendCommand(0x4F);  // Set RAM y address count to 0
    sendData(0x00);
    sendData(0x00);

    // 启用 Fast update 支持 (1.5s 快速刷新)
    sendCommand(0x1A);
    sendData(0x6A);

    waitBusy(4000);
    _in_partial_mode = false;
    Serial.println("[EPD] initFull: SSD1677 initialization complete!");
}

void Epd3in97::resetRamCursor() {
    sendCommand(0x4E);
    sendData(0x00);
    sendData(0x00);
    sendCommand(0x4F);
    sendData(0x00);
    sendData(0x00);
}

// 深睡后控制器 RAM 内容丢失: 硬件复位 + 重新初始化, 再把屏幕上现有画面写回 0x26 作为局刷基准
void Epd3in97::wakeIfAsleep() {
    if (!_asleep) return;
    Serial.println("[EPD] Waking from deep sleep...");
    initFull();
    resetRamCursor();
    sendCommand(0x26);
    sendBytes(_prev, EPD_BUFFER_SIZE);
    _asleep = false;
}

void Epd3in97::clearBuffer(uint8_t color) {
    if (_buffer) {
        memset(_buffer, color, EPD_BUFFER_SIZE);
    }
}

void Epd3in97::displayFull() {
    if (!_buffer) return;

    wakeIfAsleep();
    Serial.println("[EPD] displayFull: Sending 48KB RAM (0xF7, 全局消除残影)...");
    
    // 恢复边框波形
    sendCommand(0x3C);
    sendData(0x01);

    // 确保显存游标复位至 (0, 0) 并写入 RAM 0x24 (当前图) 与 0x26 (基准图)
    resetRamCursor();
    sendCommand(0x24);
    sendBytes(_buffer, EPD_BUFFER_SIZE);

    resetRamCursor();
    sendCommand(0x26);
    sendBytes(_buffer, EPD_BUFFER_SIZE);

    // EPD_TurnOnDisplay 全刷
    sendCommand(0x22);
    sendData(0xF7);
    sendCommand(0x20);
    waitBusy(8000);
    memcpy(_prev, _buffer, EPD_BUFFER_SIZE);
    Serial.println("[EPD] displayFull: Completed!");
}

void Epd3in97::displayFast() {
    if (!_buffer) return;

    wakeIfAsleep();
    Serial.println("[EPD] displayFast: Sending 48KB RAM (0xD7, 1.5s 极速更新)...");
    
    resetRamCursor();

    sendCommand(0x24);
    sendBytes(_buffer, EPD_BUFFER_SIZE);

    // EPD_TurnOnDisplay_Fast
    sendCommand(0x22);
    sendData(0xD7);
    sendCommand(0x20);
    waitBusy(5000);
    memcpy(_prev, _buffer, EPD_BUFFER_SIZE);
    Serial.println("[EPD] displayFast: Completed!");
}

void Epd3in97::displayPartBaseImage() {
    displayFull();
}

void Epd3in97::displayPart() {
    if (!_buffer) return;

    wakeIfAsleep();
    Serial.println("[EPD] displayPart: 触发真正无闪烁差分局刷 (0xFF)...");

    // 1. 设置边框波形为局刷防闪烁模式 (0x80)
    sendCommand(0x3C);
    sendData(0x80);

    // 2. 将新一帧写入 RAM 0x24
    resetRamCursor();

    sendCommand(0x24);
    sendBytes(_buffer, EPD_BUFFER_SIZE);

    // 3. 触发局部更新波形 (0xFF = Mode 2, 无闪烁，仅更新变化像素)
    sendCommand(0x22);
    sendData(0xFF);
    sendCommand(0x20);
    waitBusy(4000);

    // 4. 将当前帧同步写入 RAM 0x26 作为下一次局刷的差分基准底图
    resetRamCursor();

    sendCommand(0x26);
    sendBytes(_buffer, EPD_BUFFER_SIZE);
    memcpy(_prev, _buffer, EPD_BUFFER_SIZE);

    Serial.println("[EPD] displayPart: 局刷完成 (无闪烁)!");
}

void Epd3in97::sleep() {
    if (_asleep || !_initialized) return;
    Serial.println("[EPD] Entering deep sleep...");
    sendCommand(0x10);  // Enter deep sleep (与官方 EPD_3IN97_Sleep 一致)
    sendData(0x01);
    delay(100);
    _asleep = true;
}

void Epd3in97::drawPixel(int x, int y, bool black) {
    if (x < 0 || x >= EPD_NATIVE_WIDTH || y < 0 || y >= EPD_NATIVE_HEIGHT || !_buffer) {
        return;
    }
    uint32_t index = (uint32_t)y * (EPD_NATIVE_WIDTH / 8) + (x >> 3);
    uint8_t bit = 7 - (x & 0x07);
    if (!black) {
        _buffer[index] |= (0x01 << bit);   // White is bit 1
    } else {
        _buffer[index] &= ~(0x01 << bit);  // Black is bit 0
    }
}
