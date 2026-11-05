#ifndef RTC_PCF85063_H
#define RTC_PCF85063_H

#include "study_day.h"

/**
 * @file rtc_pcf85063.h
 * @brief 板载 PCF85063 实时时钟 (I2C 0x51, 与 AXP2101 共用总线, 须在其后初始化)
 *        存本地时间, 24 小时制
 */

// 探测芯片; 不在线返回 false
bool rtc_begin();

// 读时间; 从未对时、掉电导致振荡器停过 (OS 位) 或字段非法时返回 false
bool rtc_read(LocalDateTime& dt);

// 写时间并清除 OS 位
bool rtc_write(const LocalDateTime& dt);

#endif // RTC_PCF85063_H
