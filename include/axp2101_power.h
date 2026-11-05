#ifndef AXP2101_POWER_H
#define AXP2101_POWER_H

#include <stdint.h>
#include <stdbool.h>
#include <Arduino.h>

/**
 * @file axp2101_power.h
 * @brief 微雪 Waveshare ESP32-S3-ePaper-3.97 板载 AXP2101 PMIC 电源管理驱动
 */

class Axp2101Power {
public:
    static Axp2101Power& getInstance() {
        static Axp2101Power instance;
        return instance;
    }

    // 初始化 I2C 并配置 AXP2101 供电轨 (DC1=3.3V, ALDO1/2/3=3.3V 给墨水屏与外设供电)
    bool begin(int sda = 41, int scl = 42, uint32_t freq = 400000);

    // 读写寄存器
    uint8_t readReg(uint8_t reg);
    bool writeReg(uint8_t reg, uint8_t val);

    // 电池与电源状态
    int getBatteryPercent(); // 0 ~ 100%, 未初始化或读取失败返回 -1
    // 电池电流方向 (0x01 bit6:5): 0 待机, 1 充电, 2 放电; 读取失败返回 -1
    int batteryDirection();
    bool isCharging();
    bool isDischarging();

    // USB 供电是否存在 (0x00 bit5 VBUS good); 读取失败按不存在处理
    bool isVbusPresent();

    // 串口打印电源状态相关寄存器 (诊断用)
    void dumpStatus();

    // 软关机
    void powerOff();

private:
    Axp2101Power() = default;
    bool readRegChecked(uint8_t reg, uint8_t& out);
    bool _initialized = false;
    uint8_t _i2c_addr = 0x34;
};

#endif // AXP2101_POWER_H
