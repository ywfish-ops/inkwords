#include "rtc_pcf85063.h"
#include "board_config.h"
#include <Wire.h>

static const uint8_t REG_CONTROL_1 = 0x00;
static const uint8_t REG_SECONDS = 0x04;     // 04..0A: 秒 分 时 日 星期 月 年
static const uint8_t SECONDS_OS_BIT = 0x80;  // 振荡器停过, 时间不可信

static uint8_t bcd2bin(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }
static uint8_t bin2bcd(int v) { return (uint8_t)(((v / 10) << 4) | (v % 10)); }

bool rtc_begin() {
    Wire.beginTransmission(I2C_ADDR_PCF85063);
    if (Wire.endTransmission() != 0) return false;
    // Control_1 = 0: 振荡器运行, 24 小时制
    Wire.beginTransmission(I2C_ADDR_PCF85063);
    Wire.write(REG_CONTROL_1);
    Wire.write(0x00);
    return Wire.endTransmission() == 0;
}

bool rtc_read(LocalDateTime& dt) {
    Wire.beginTransmission(I2C_ADDR_PCF85063);
    Wire.write(REG_SECONDS);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((uint8_t)I2C_ADDR_PCF85063, (uint8_t)7) != 7) return false;
    uint8_t r[7];
    for (int i = 0; i < 7; i++) r[i] = Wire.read();
    if (r[0] & SECONDS_OS_BIT) return false;

    dt.second = bcd2bin(r[0] & 0x7F);
    dt.minute = bcd2bin(r[1] & 0x7F);
    dt.hour = bcd2bin(r[2] & 0x3F);
    dt.day = bcd2bin(r[3] & 0x3F);
    dt.month = bcd2bin(r[5] & 0x1F);
    dt.year = 2000 + bcd2bin(r[6]);
    return datetime_valid(dt);
}

bool rtc_write(const LocalDateTime& dt) {
    if (!datetime_valid(dt)) return false;
    // 星期: 1970-01-01 是星期四 (4)
    int weekday = (int)((datetime_to_epoch(dt) / 86400 + 4) % 7);
    Wire.beginTransmission(I2C_ADDR_PCF85063);
    Wire.write(REG_SECONDS);
    Wire.write(bin2bcd(dt.second));  // 写秒时清 OS 位
    Wire.write(bin2bcd(dt.minute));
    Wire.write(bin2bcd(dt.hour));
    Wire.write(bin2bcd(dt.day));
    Wire.write((uint8_t)weekday);
    Wire.write(bin2bcd(dt.month));
    Wire.write(bin2bcd(dt.year - 2000));
    return Wire.endTransmission() == 0;
}
