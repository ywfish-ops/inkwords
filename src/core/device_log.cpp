#include "device_log.h"
#include "log_format.h"
#include "rtc_pcf85063.h"
#include "sdcard_bsp.h"
#include <Arduino.h>
#include <SD_MMC.h>
#include <esp_system.h>
#include <esp32s3/rom/rtc.h>
#include <stdarg.h>

static const char* LOG_DIR = "/spell/logs";
static const char* LOG_PATH = "/spell/logs/device.log";
static const char* LOG_OLD_PATH = "/spell/logs/device.1.log";
static const size_t LOG_LIMIT = 256 * 1024;
static SemaphoreHandle_t s_lock = nullptr;
static bool s_ready = false;

// 追加一行到日志文件, 超过上限先滚动; 调用方已持锁
static void append_locked(const char* line, size_t n) {
    File f = SD_MMC.open(LOG_PATH, FILE_READ);
    size_t cur = f ? f.size() : 0;
    if (f) f.close();
    if (log_needs_rotate(cur, n, LOG_LIMIT)) {
        SD_MMC.remove(LOG_OLD_PATH);
        SD_MMC.rename(LOG_PATH, LOG_OLD_PATH);
    }
    f = SD_MMC.open(LOG_PATH, FILE_APPEND);
    if (f) {
        f.write((const uint8_t*)line, n);
        f.close();
    }
}

void log_write(char level, const char* tag, const char* fmt, ...) {
    char msg[200];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    LocalDateTime dt;
    bool valid = rtc_read(dt);
    char line[256];
    size_t n = log_format_line(line, sizeof(line), valid ? &dt : nullptr, millis(), level, tag, msg);
    Serial.print(line);

    if (!s_lock || !s_ready) return;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    append_locked(line, n);
    xSemaphoreGive(s_lock);
}

static const char* reset_reason_name(esp_reset_reason_t r) {
    switch (r) {
        case ESP_RST_POWERON:   return "上电";
        case ESP_RST_EXT:       return "外部复位";
        case ESP_RST_SW:        return "软件重启";
        case ESP_RST_PANIC:     return "程序崩溃";
        case ESP_RST_INT_WDT:   return "中断看门狗";
        case ESP_RST_TASK_WDT:  return "任务看门狗";
        case ESP_RST_WDT:       return "看门狗";
        case ESP_RST_DEEPSLEEP: return "深睡唤醒";
        case ESP_RST_BROWNOUT:  return "电压过低";
        default:                return "未知";
    }
}

void log_begin() {
    if (!s_lock) s_lock = xSemaphoreCreateMutex();
    if (sdcard_is_mounted()) {
        if (!SD_MMC.exists(LOG_DIR)) SD_MMC.mkdir(LOG_DIR);
        s_ready = true;
    }
    esp_reset_reason_t r = esp_reset_reason();
    bool abnormal = r == ESP_RST_PANIC || r == ESP_RST_INT_WDT || r == ESP_RST_TASK_WDT || r == ESP_RST_WDT ||
                    r == ESP_RST_BROWNOUT;
    // 异常重启记为错误, 方便 `sdsync log --errors` 筛出来
    // rom 为芯片原始复位码; esp 归为"未知"的 21/22 是 USB 串口/JTAG 复位 (烧录或 sdsync reset)
    int rom = (int)rtc_get_reset_reason(0);
    const char* name = (r == ESP_RST_UNKNOWN && (rom == 21 || rom == 22)) ? "USB 复位" : reset_reason_name(r);
    log_write(abnormal ? 'E' : 'I', "BOOT", "开机, 原因: %s (esp=%d, rom=%d)", name, (int)r, rom);
}
