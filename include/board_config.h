#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

/**
 * @file board_config.h
 * @brief 微雪 Waveshare ESP32-S3-ePaper-3.97 硬件引脚与外设配置表
 *        (经原厂固件二进制逆向与官方 BSP 双重验证)
 */

// ============================================================================
// 1. 墨水屏显示 (3.97寸 800x480 SSD1677, SPI3)
//    逻辑采用 480x800 竖屏旋转渲染
// ============================================================================
#define EPD_SPI_HOST            SPI3_HOST
#define EPD_DC_PIN              9
#define EPD_CS_PIN              10
#define EPD_SCK_PIN             11
#define EPD_MOSI_PIN            12
#define EPD_RST_PIN             46
#define EPD_BUSY_PIN            3

#define EPD_NATIVE_WIDTH        800
#define EPD_NATIVE_HEIGHT       480

// 竖屏逻辑分辨率 (Portrait Mode)
#define UI_WIDTH                480
#define UI_HEIGHT               800
#define EPD_BUFFER_SIZE         (EPD_NATIVE_WIDTH * EPD_NATIVE_HEIGHT / 8) // 48,000 Bytes

// ============================================================================
// 2. 物理按键与三向拨轮 (拨轮与 BOOT 为 Active LOW + 内部上拉; 电源键为 Active HIGH)
//    掩码 0x71 = (1<<6) | (1<<5) | (1<<4) | (1<<0)
// ============================================================================
#define BTN_BOOT_PIN            0   // 侧边 BOOT 键 (短按发音 / 双击菜单 / 长按全局全刷)
#define BTN_PWR_PIN             1   // 电源键 (VBAT_PWR_GPIO), 高电平有效: 实测松开 0 / 按下 1, 普通输入不加上下拉
#define DIAL_UP_PIN             4   // 拨轮上拨 (GPIO 4: 物理向上拨动 -> 没记住 / 上一张)
#define DIAL_CONFIRM_PIN        5   // 拨轮按压中键 (GPIO 5: 垂直按压 -> 翻面 / 确认选择)
#define DIAL_DOWN_PIN           6   // 拨轮下拨 (GPIO 6: 物理向下拨动 -> 记住了 / 下一张)

// ============================================================================
// 3. TF / MicroSD 卡槽 (SDMMC 4-bit / 1-bit 模式)
// ============================================================================
#define SDMMC_CLK_PIN           16
#define SDMMC_CMD_PIN           17
#define SDMMC_D0_PIN            15
#define SDMMC_D1_PIN            7
#define SDMMC_D2_PIN            8
#define SDMMC_D3_PIN            18
#define SD_MOUNT_POINT          "/sdcard"
#define SPELL_ROOT_DIR          "/sdcard/spell"

// ============================================================================
// 4. I2C 总线与板载外设 (PMIC / Audio / RTC / 温湿度 / IMU)
// ============================================================================
#define I2C_MASTER_NUM          0
#define I2C_MASTER_SDA_PIN      41
#define I2C_MASTER_SCL_PIN      42
#define I2C_MASTER_FREQ_HZ      400000

#define I2C_ADDR_AXP2101        0x34    // 电源管理芯片
#define I2C_ADDR_ES8311         0x18    // 低功耗音频编解码器
#define I2C_ADDR_PCF85063       0x51    // RTC 实时时钟
#define I2C_ADDR_SHTC3          0x70    // 温湿度传感器
#define I2C_ADDR_QMI8658        0x6B    // 6轴姿态传感器

// ============================================================================
// 5. I2S 音频接口 (ES8311 + 功放使能)
// ============================================================================
#define AUDIO_I2S_MCLK_PIN      13
#define AUDIO_I2S_BCLK_PIN      14
#define AUDIO_I2S_WS_PIN        47
#define AUDIO_I2S_DOUT_PIN      48      // ESP32 -> ES8311 DAC (扬声器输出)
#define AUDIO_I2S_DIN_PIN       21      // ES8311 ADC -> ESP32 (麦克风输入)
#define AUDIO_PA_CTRL_PIN       39      // 功放使能引脚 (HIGH 开启喇叭)
#define AUDIO_SAMPLE_RATE       24000

#endif // BOARD_CONFIG_H
