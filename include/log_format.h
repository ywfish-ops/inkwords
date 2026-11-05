#ifndef LOG_FORMAT_H
#define LOG_FORMAT_H

#include <stddef.h>
#include <stdint.h>
#include "study_day.h"

/**
 * @file log_format.h
 * @brief 设备日志的纯逻辑部分 (行格式与滚动判断), 可在主机上测试
 *
 * 一行: "2026-10-21 21:30:05 +12345 E SD    写入失败: ...\n"
 *        RTC 时间 (无效时为 "---------- --:--:--"), 开机后毫秒, 级别, 模块名, 内容
 */

// 格式化一行 (保证以 '\n' 结尾, 过长截断); dt 为 nullptr 表示 RTC 无效。返回长度
size_t log_format_line(char* out, size_t cap, const LocalDateTime* dt, uint32_t ms, char level,
                       const char* tag, const char* msg);

// 当前文件 cur 字节, 再写 add 字节是否会超过 limit (需要先滚动到 .1)
bool log_needs_rotate(size_t cur, size_t add, size_t limit);

#endif // LOG_FORMAT_H
