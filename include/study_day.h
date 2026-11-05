#ifndef STUDY_DAY_H
#define STUDY_DAY_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @file study_day.h
 * @brief 本地时间 <-> 秒数换算与"学习日" (纯逻辑, 可在主机上测试)
 *
 * RTC 存的就是本地时间 (电脑对时时写入 Mac 的本地时间), 这里把它当作
 * 无时区的秒数来算, 不涉及时区转换。学习日以凌晨 4 点为界:
 * 23:30 和次日 00:30 属于同一个学习日。
 */

struct LocalDateTime {
    int year;    // 2000 ~ 2099 (RTC 只存两位年份)
    int month;   // 1 ~ 12
    int day;     // 1 ~ 31
    int hour;    // 0 ~ 23
    int minute;  // 0 ~ 59
    int second;  // 0 ~ 59
};

#define STUDY_DAY_START_HOUR 4

// 字段是否是合法日期时间 (含闰年 2 月)
bool datetime_valid(const LocalDateTime& dt);

// 本地时间 -> 自 1970-01-01 00:00 起的秒数 (按本地时间计)
int64_t datetime_to_epoch(const LocalDateTime& dt);

void epoch_to_datetime(int64_t epoch, LocalDateTime& dt);

// 学习日编号: (秒数 - 4 小时) 所在的天数
int32_t study_day(int64_t epoch);

#endif // STUDY_DAY_H
