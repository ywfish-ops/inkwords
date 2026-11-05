#include "log_format.h"
#include <stdio.h>
#include <string.h>

size_t log_format_line(char* out, size_t cap, const LocalDateTime* dt, uint32_t ms, char level,
                       const char* tag, const char* msg) {
    if (cap < 2) return 0;
    int n;
    if (dt) {
        n = snprintf(out, cap, "%04d-%02d-%02d %02d:%02d:%02d +%lu %c %-5s %s", dt->year, dt->month, dt->day,
                     dt->hour, dt->minute, dt->second, (unsigned long)ms, level, tag ? tag : "", msg ? msg : "");
    } else {
        n = snprintf(out, cap, "---------- --:--:-- +%lu %c %-5s %s", (unsigned long)ms, level, tag ? tag : "",
                     msg ? msg : "");
    }
    size_t len = n < 0 ? 0 : (size_t)n;
    if (len > cap - 2) {
        // 截断, 给 '\n' 留位置, 且不切断 UTF-8 多字节字符
        len = cap - 2;
        while (len > 0 && ((unsigned char)out[len] & 0xC0) == 0x80) len--;
    }
    // 内容里的换行会把一条记录拆成多行, 换成空格
    for (size_t i = 0; i < len; i++) {
        if (out[i] == '\n' || out[i] == '\r') out[i] = ' ';
    }
    out[len++] = '\n';
    out[len] = '\0';
    return len;
}

bool log_needs_rotate(size_t cur, size_t add, size_t limit) { return cur + add > limit; }
