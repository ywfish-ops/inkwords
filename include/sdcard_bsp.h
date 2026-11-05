#ifndef SDCARD_BSP_H
#define SDCARD_BSP_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * @file sdcard_bsp.h
 * @brief TF 卡 SDMMC 挂载 (优先 4-bit, 失败退回 1-bit), 挂载点 SD_MOUNT_POINT
 *
 * 只在开机时挂载一次, 不支持热插拔。
 */

// 挂载 TF 卡; 成功返回 true。重复调用直接返回上次结果
bool sdcard_mount();

bool sdcard_is_mounted();

// 当前总线宽度 (4 或 1), 未挂载为 0
uint8_t sdcard_bus_width();

// 卡容量 (MB), 未挂载为 0
uint32_t sdcard_size_mb();

// 把整个文件读进 PSRAM, 末尾补 '\0'; 返回缓冲 (调用方 free), 失败返回 nullptr
// path 为完整路径, 如 SPELL_ROOT_DIR "/decks/manifest.json"
char* sdcard_read_file(const char* path, size_t* out_len);

// 写整个文件: 先写 path.tmp 再删旧文件、改名, 写一半断电不会留下半个文件; 成功返回 true
bool sdcard_write_file(const char* path, const char* data, size_t len);

// 串口打印目录内容 (调试用)
void sdcard_list_dir(const char* path);

#endif // SDCARD_BSP_H
