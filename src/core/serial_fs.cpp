#include "serial_fs.h"
#include "sdcard_bsp.h"
#include "rtc_pcf85063.h"
#include "board_config.h"
#include <Arduino.h>
#include <SD_MMC.h>
#include <esp_heap_caps.h>

static const uint32_t LINE_TIMEOUT_MS = 2000;
static const uint32_t DATA_TIMEOUT_MS = 5000;   // 两次收到数据的最大间隔
static const size_t   MAX_PUT_SIZE = 4 * 1024 * 1024;

static uint32_t crc32_update(uint32_t crc, const uint8_t* p, size_t n) {
    crc = ~crc;
    while (n--) {
        crc ^= *p++;
        for (int k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

static bool read_line(char* buf, size_t cap) {
    size_t n = 0;
    uint32_t t0 = millis();
    while (millis() - t0 < LINE_TIMEOUT_MS) {
        int c = Serial.read();
        if (c < 0) {
            delay(1);
            continue;
        }
        if (c == '\r') continue;
        if (c == '\n') {
            buf[n] = '\0';
            return true;
        }
        if (n + 1 >= cap) return false;
        buf[n++] = (char)c;
    }
    return false;
}

// USB 串口接收缓冲满时会丢数据, 所以电脑端每发 CHUNK 字节就等一行 "#A" 再继续
static const size_t CHUNK = 4096;

static bool read_exact(uint8_t* dst, size_t len) {
    size_t got = 0;
    size_t next_ack = CHUNK;
    uint32_t last = millis();
    while (got < len) {
        if (got >= next_ack) {
            Serial.print("#A\n");
            next_ack += CHUNK;
        }
        size_t avail = Serial.available();
        if (avail == 0) {
            if (millis() - last > DATA_TIMEOUT_MS) return false;
            delay(1);
            continue;
        }
        size_t want = len - got;
        got += Serial.read(dst + got, avail < want ? avail : want);
        last = millis();
    }
    return true;
}

static void reply_err(const char* msg) { Serial.printf("\n#ERR %s\n", msg); }

// 协议路径 -> SD_MMC 路径 ("/spell/..."); 拒绝 ".." 与空路径
static bool to_sd_path(const char* in, char* out, size_t cap) {
    while (*in == '/') in++;
    if (strstr(in, "..")) return false;
    return snprintf(out, cap, "/%s", in) < (int)cap;
}

static void make_dirs(const char* sd_path, bool include_last) {
    char tmp[160];
    strncpy(tmp, sd_path, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    for (char* p = tmp + 1; *p; p++) {
        if (*p != '/') continue;
        *p = '\0';
        if (!SD_MMC.exists(tmp)) SD_MMC.mkdir(tmp);
        *p = '/';
    }
    if (include_last && !SD_MMC.exists(tmp)) SD_MMC.mkdir(tmp);
}

// 读整个文件到 PSRAM, 失败返回 nullptr
static uint8_t* load(const char* sd_path, size_t* len) {
    char full[176];
    snprintf(full, sizeof(full), SD_MOUNT_POINT "%s", sd_path);
    return (uint8_t*)sdcard_read_file(full, len);
}

static void cmd_put(const char* path, size_t size, uint32_t crc) {
    if (size > MAX_PUT_SIZE) {
        reply_err("文件过大");
        return;
    }
    uint8_t* buf = (uint8_t*)heap_caps_malloc(size ? size : 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf) {
        reply_err("内存不足");
        return;
    }
    Serial.print("\n#READY\n");
    bool ok = read_exact(buf, size);
    if (!ok) {
        reply_err("接收超时");
    } else if (crc32_update(0, buf, size) != crc) {
        reply_err("CRC 不符");
    } else {
        make_dirs(path, false);
        char full[176];
        snprintf(full, sizeof(full), SD_MOUNT_POINT "%s", path);
        if (sdcard_write_file(full, (const char*)buf, size)) Serial.print("\n#OK\n");
        else reply_err("写入失败");
    }
    free(buf);
}

static void cmd_get(const char* path, bool with_data) {
    size_t len = 0;
    uint8_t* buf = load(path, &len);
    if (!buf) {
        reply_err("文件不存在");
        return;
    }
    Serial.printf("\n#OK %u %08lx\n", (unsigned)len, (unsigned long)crc32_update(0, buf, len));
    if (with_data) {
        Serial.write(buf, len);
        Serial.flush();
    }
    free(buf);
}

static void cmd_ls(const char* path) {
    File dir = SD_MMC.open(path);
    if (!dir || !dir.isDirectory()) {
        reply_err("目录不存在");
        return;
    }
    Serial.print("\n");
    for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
        if (f.isDirectory()) Serial.printf("#D %s\n", f.name());
        else Serial.printf("#F %lu %s\n", (unsigned long)f.size(), f.name());
        f.close();
    }
    dir.close();
    Serial.print("#OK\n");
}

static void cmd_del(const char* path) {
    File f = SD_MMC.open(path);
    if (!f) {
        reply_err("不存在");
        return;
    }
    bool is_dir = f.isDirectory();
    f.close();
    bool ok = is_dir ? SD_MMC.rmdir(path) : SD_MMC.remove(path);
    if (ok) Serial.print("\n#OK\n");
    else reply_err(is_dir ? "目录非空或删除失败" : "删除失败");
}

static void handle_line() {
    char line[200];
    if (!read_line(line, sizeof(line))) {
        reply_err("命令行超时或过长");
        return;
    }
    // PING 用于电脑端对齐应答 (丢弃上一次中断留下的输出), 不需要 TF 卡
    // TIME 读/写 RTC 本地时间 (秒数, 见 study_day.h), 不需要 TF 卡
    if (strncmp(line, "TIME", 4) == 0 && (line[4] == '\0' || line[4] == ' ')) {
        LocalDateTime dt;
        if (line[4] == ' ') {
            epoch_to_datetime(strtoll(line + 5, nullptr, 10), dt);
            if (!rtc_write(dt)) {
                reply_err("写 RTC 失败");
                return;
            }
        }
        if (rtc_read(dt)) Serial.printf("\n#OK %lld\n", (long long)datetime_to_epoch(dt));
        else Serial.print("\n#OK invalid\n");
        return;
    }
    if (strncmp(line, "PING ", 5) == 0) {
        Serial.printf("\n#OK %s\n", line + 5);
        return;
    }
    if (!sdcard_is_mounted()) {
        reply_err("未挂载 TF 卡");
        return;
    }

    char op[8] = "", arg[160] = "";
    unsigned long size = 0, crc = 0;
    int n = sscanf(line, "%7s %159s %lu %lx", op, arg, &size, &crc);
    char path[176];
    if (n < 2 || !to_sd_path(arg, path, sizeof(path))) {
        reply_err("命令格式错误");
        return;
    }

    if (strcmp(op, "PUT") == 0 && n == 4) cmd_put(path, size, (uint32_t)crc);
    else if (strcmp(op, "GET") == 0) cmd_get(path, true);
    else if (strcmp(op, "STAT") == 0) cmd_get(path, false);
    else if (strcmp(op, "LS") == 0) cmd_ls(path);
    else if (strcmp(op, "DEL") == 0) cmd_del(path);
    else if (strcmp(op, "MKDIR") == 0) {
        make_dirs(path, true);
        if (SD_MMC.exists(path)) Serial.print("\n#OK\n");
        else reply_err("建目录失败");
    } else reply_err("未知命令");
}

// 实测: 读写 TF 卡之后, USB 串口的应答偶尔滞留在发送缓冲里, 直到下一次写串口才发出 (电脑端就等不到)。
// 应答后等发送缓冲走空 (可写空间回到命令开始前的大小), 没走空就补发换行把它推出去 (电脑端忽略空行)
static void push_out_reply(int idle_free) {
    for (int i = 0; i < 50 && Serial.availableForWrite() < idle_free; i++) {
        delay(10);
        if (i % 2 == 1) Serial.write('\n');
    }
}

void serial_fs_handle_command() {
    int idle_free = Serial.availableForWrite();
    handle_line();
    push_out_reply(idle_free);
}
