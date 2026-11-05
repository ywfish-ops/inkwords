#ifndef DEVICE_LOG_H
#define DEVICE_LOG_H

/**
 * @file device_log.h
 * @brief 持久日志: 写到 TF 卡 spell/logs/device.log (超过 256KB 滚动为 device.1.log),
 *        同时打印到串口。出 bug 后用 `sdsync log` 查看。
 *
 * 级别 'E' 错误 / 'W' 警告 / 'I' 信息。每条立即追加写卡 (记录的都是低频事件,
 * 不记每分钟的轮询), 崩溃或复位也不丢。可在任意任务中调用 (内部加锁)。
 * TF 卡未挂载时只打印到串口。
 */

// 挂载 TF 卡并初始化 RTC 之后调用: 记录本次开机原因 (含上次是否崩溃/看门狗/掉电)
void log_begin();

void log_write(char level, const char* tag, const char* fmt, ...) __attribute__((format(printf, 3, 4)));

#define LOGE(tag, ...) log_write('E', tag, __VA_ARGS__)
#define LOGW(tag, ...) log_write('W', tag, __VA_ARGS__)
#define LOGI(tag, ...) log_write('I', tag, __VA_ARGS__)

#endif // DEVICE_LOG_H
