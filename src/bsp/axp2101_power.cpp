#include "axp2101_power.h"
#include <Wire.h>
#include <esp_log.h>

static const char* TAG = "AXP2101";

bool Axp2101Power::begin(int sda, int scl, uint32_t freq) {
    Wire.begin(sda, scl, freq);

    // 探测设备
    Wire.beginTransmission(_i2c_addr);
    if (Wire.endTransmission() != 0) {
        ESP_LOGE(TAG, "AXP2101 not detected at address 0x%02X on SDA=%d, SCL=%d", _i2c_addr, sda, scl);
        return false;
    }

    ESP_LOGI(TAG, "AXP2101 detected. Configuring power rails...");

    writeReg(0x22, 0b110);  // PWRON > OFFLEVEL as POWEROFF Source enable
    // 0x27: bit5:4 IRQ 1.5s, bit3:2 硬件关机 8s (0:4s 1:6s 2:8s 3:10s), bit1:0 开机 128ms;
    // 8s 给长按 2s 后画关机页 + 约 2.1s 全刷留足余量, 死机时仍可按住强制关机
    writeReg(0x27, 0x18);

    // Disable All DCs but DC1
    writeReg(0x80, 0x01);

    // Disable All LDOs initially
    writeReg(0x90, 0x00);
    writeReg(0x91, 0x00);

    // Set DC1 to 3.3V: (3300 - 1500) / 100 = 18 = 0x12
    writeReg(0x82, (3300 - 1500) / 100);

    // Set ALDO1 to 3.3V: (3300 - 500) / 100 = 28 = 0x1C
    writeReg(0x92, (3300 - 500) / 100);
    // Set ALDO2 to 3.3V: (3300 - 500) / 100 = 28 = 0x1C
    writeReg(0x93, (3300 - 500) / 100);
    // Set ALDO3 to 3.3V: (3300 - 500) / 100 = 28 = 0x1C
    writeReg(0x94, (3300 - 500) / 100);

    // Enable ALDO1, ALDO2, ALDO3
    writeReg(0x90, 0x07);

    // 充电管理参数
    writeReg(0x64, 0x03);  // 恒压充电 4.2V
    writeReg(0x61, 0x02);  // 预充电流 50mA
    writeReg(0x62, 0x08);  // 充电流 200mA (0x08=200mA, 0x09=300mA, 0x0A=400mA, 与官方 BSP 一致)
    writeReg(0x63, 0x01);  // 截止电流 25mA

    _initialized = true;
    ESP_LOGI(TAG, "AXP2101 configured successfully. Battery level: %d%%", getBatteryPercent());
    return true;
}

bool Axp2101Power::readRegChecked(uint8_t reg, uint8_t& out) {
    Wire.beginTransmission(_i2c_addr);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) {
        return false;
    }
    if (Wire.requestFrom((uint8_t)_i2c_addr, (uint8_t)1) != 1) {
        return false;
    }
    out = Wire.read();
    return true;
}

uint8_t Axp2101Power::readReg(uint8_t reg) {
    uint8_t val = 0;
    readRegChecked(reg, val);
    return val;
}

bool Axp2101Power::writeReg(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(_i2c_addr);
    Wire.write(reg);
    Wire.write(val);
    return (Wire.endTransmission() == 0);
}

int Axp2101Power::getBatteryPercent() {
    uint8_t percent = 0;
    if (!_initialized || !readRegChecked(0xA4, percent)) return -1;
    if (percent > 100) return 100;
    return percent;
}

int Axp2101Power::batteryDirection() {
    uint8_t status = 0;
    if (!_initialized || !readRegChecked(0x01, status)) return -1;
    return (status >> 5) & 0x03;
}

// 与小智官方 Axp2101::IsCharging / IsDischarging 一致
bool Axp2101Power::isCharging() {
    return batteryDirection() == 1;
}

bool Axp2101Power::isDischarging() {
    return batteryDirection() == 2;
}

bool Axp2101Power::isVbusPresent() {
    uint8_t status = 0;
    if (!_initialized || !readRegChecked(0x00, status)) return false;
    return (status & (1 << 5)) != 0;
}

void Axp2101Power::dumpStatus() {
    uint8_t r00 = readReg(0x00), r01 = readReg(0x01), r18 = readReg(0x18);
    uint8_t r30 = readReg(0x30), r34 = readReg(0x34), r35 = readReg(0x35), ra4 = readReg(0xA4);
    Serial.printf("[AXP2101] 0x00=0x%02X (VBUS good=%d, BAT present=%d)\n", r00, (r00 >> 5) & 1, (r00 >> 3) & 1);
    Serial.printf("[AXP2101] 0x01=0x%02X (dir=%d [0待机 1充电 2放电], chg_stat=%d)\n", r01, (r01 >> 5) & 3, r01 & 7);
    Serial.printf("[AXP2101] 0x18=0x%02X (fuel gauge en=%d)\n", r18, (r18 >> 3) & 1);
    Serial.printf("[AXP2101] 0x30=0x%02X (VBAT ADC en=%d)  VBAT=%u mV\n", r30, r30 & 1,
                  (unsigned)(((r34 & 0x3F) << 8) | r35));
    Serial.printf("[AXP2101] 0xA4=%u (battery %%)\n", ra4);
    uint8_t r27 = readReg(0x27);
    Serial.printf("[AXP2101] 0x27=0x%02X (按住 %d s 硬件关机)\n", r27, 4 + 2 * ((r27 >> 2) & 3));
}

void Axp2101Power::powerOff() {
    ESP_LOGI(TAG, "Requesting AXP2101 power off...");
    // 置位 0x10 bit0 软关机, 保留其余位
    uint8_t val = 0;
    if (readRegChecked(0x10, val)) {
        writeReg(0x10, val | 0x01);
    }
}
