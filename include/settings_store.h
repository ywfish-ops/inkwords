#ifndef SETTINGS_STORE_H
#define SETTINGS_STORE_H

#include "spell_types.h"

/**
 * @file settings_store.h
 * @brief 用户设置持久化 (芯片 NVS, 命名空间 "spell"), 断电保存, 不依赖 TF 卡
 */

// 读取设置; 未保存过或数值非法的字段使用默认值 (音量 100, 英→中, 不自动发音)
void settings_load(UserSettings& settings);

// 保存音量 / 背卡方向 / 切卡自动发音
void settings_save(const UserSettings& settings);

#endif // SETTINGS_STORE_H
