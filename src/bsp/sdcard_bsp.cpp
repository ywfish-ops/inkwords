#include "sdcard_bsp.h"
#include "board_config.h"
#include <Arduino.h>
#include <SD_MMC.h>
#include <esp_heap_caps.h>

static bool s_tried = false;
static bool s_mounted = false;
static uint8_t s_width = 0;

static bool try_mount(bool one_bit) {
    bool ok = one_bit
        ? SD_MMC.setPins(SDMMC_CLK_PIN, SDMMC_CMD_PIN, SDMMC_D0_PIN)
        : SD_MMC.setPins(SDMMC_CLK_PIN, SDMMC_CMD_PIN, SDMMC_D0_PIN,
                         SDMMC_D1_PIN, SDMMC_D2_PIN, SDMMC_D3_PIN);
    if (!ok) return false;
    if (!SD_MMC.begin(SD_MOUNT_POINT, one_bit)) return false;
    if (SD_MMC.cardType() == CARD_NONE) {
        SD_MMC.end();
        return false;
    }
    return true;
}

bool sdcard_mount() {
    if (s_tried) return s_mounted;
    s_tried = true;

    if (try_mount(false)) {
        s_width = 4;
    } else {
        Serial.println("[SD] 4-bit 挂载失败, 尝试 1-bit...");
        if (try_mount(true)) s_width = 1;
    }
    s_mounted = s_width != 0;
    if (s_mounted) {
        Serial.printf("[SD] 挂载成功: %u-bit, 容量 %lu MB\n", s_width, (unsigned long)sdcard_size_mb());
    } else {
        Serial.println("[SD] 未检测到 TF 卡或挂载失败");
    }
    return s_mounted;
}

bool sdcard_is_mounted() { return s_mounted; }

uint8_t sdcard_bus_width() { return s_width; }

uint32_t sdcard_size_mb() {
    return s_mounted ? (uint32_t)(SD_MMC.cardSize() / (1024ULL * 1024ULL)) : 0;
}

// SD_MMC 的路径相对挂载点, 去掉 "/sdcard" 前缀
static const char* relative_path(const char* path) {
    const size_t prefix = strlen(SD_MOUNT_POINT);
    const char* rel = strncmp(path, SD_MOUNT_POINT, prefix) == 0 ? path + prefix : path;
    return *rel == '\0' ? "/" : rel;
}

char* sdcard_read_file(const char* path, size_t* out_len) {
    if (out_len) *out_len = 0;
    if (!s_mounted) return nullptr;
    File f = SD_MMC.open(relative_path(path), FILE_READ);
    if (!f || f.isDirectory()) return nullptr;
    size_t size = f.size();
    char* buf = (char*)heap_caps_malloc(size + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf) {
        f.close();
        return nullptr;
    }
    size_t got = f.read((uint8_t*)buf, size);
    f.close();
    if (got != size) {
        free(buf);
        return nullptr;
    }
    buf[size] = '\0';
    if (out_len) *out_len = size;
    return buf;
}

bool sdcard_write_file(const char* path, const char* data, size_t len) {
    if (!s_mounted) return false;
    const char* rel = relative_path(path);
    char tmp[128];
    if (snprintf(tmp, sizeof(tmp), "%s.tmp", rel) >= (int)sizeof(tmp)) return false;

    File f = SD_MMC.open(tmp, FILE_WRITE);
    if (!f) return false;
    size_t put = f.write((const uint8_t*)data, len);
    f.close();
    if (put != len) {
        SD_MMC.remove(tmp);
        return false;
    }
    // FAT 不支持覆盖式改名, 先删旧文件
    if (SD_MMC.exists(rel) && !SD_MMC.remove(rel)) return false;
    return SD_MMC.rename(tmp, rel);
}

void sdcard_list_dir(const char* path) {
    if (!s_mounted) return;
    const char* rel = relative_path(path);

    File dir = SD_MMC.open(rel);
    if (!dir || !dir.isDirectory()) {
        Serial.printf("[SD] 目录不存在: %s\n", path);
        return;
    }
    Serial.printf("[SD] %s:\n", path);
    for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
        Serial.printf("  %s%s  %lu\n", f.name(), f.isDirectory() ? "/" : "",
                      (unsigned long)f.size());
        f.close();
    }
    dir.close();
}
