#ifndef SERIAL_FS_H
#define SERIAL_FS_H

/**
 * @file serial_fs.h
 * @brief 通过 USB 串口读写 TF 卡文件, 配合电脑端 tools/sdsync.py 使用, 免拔卡
 *
 * 一行一条命令, 以 '#' 开头; 路径相对 TF 卡根目录 (如 spell/decks/manifest.json)。
 * 应答以 "#OK" 或 "#ERR <原因>" 开头, 期间串口上可能混有日志行, 电脑端需跳过。
 *
 *   #PUT <path> <size> <crc32>\n -> #READY, 之后按 4KB 一块发送, 每块后等板子回 "#A"
 *                                  写文件 (自动建父目录, 先写 .tmp 再改名)
 *   #GET <path>\n      -> #OK <size> <crc32>\n + size 字节
 *   #STAT <path>\n     -> #OK <size> <crc32>     文件不存在 -> #ERR
 *   #LS <path>\n       -> 每项一行 "#F <size> <name>" 或 "#D <name>", 最后 #OK
 *   #DEL <path>\n      删除文件或空目录
 *   #MKDIR <path>\n    逐级建目录
 *   #PING <token>\n    -> #OK <token>           电脑端连上后先对齐, 丢弃残留输出
 *   #TIME [秒数]\n     -> #OK <秒数>|invalid     读 RTC; 带参数时先写入 (本地时间, 见 study_day.h)
 *
 * crc32 为十六进制, 与 zlib.crc32 一致。
 */

// 主循环读到 '#' 后调用: 读完本行命令并执行 (阻塞直到传输完成或超时)
void serial_fs_handle_command();

#endif // SERIAL_FS_H
