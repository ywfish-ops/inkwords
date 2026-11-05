#!/usr/bin/env bash
set -e

# 用法: ./backup_firmware.sh [串口], 不给则自动找唯一的 /dev/cu.usbmodem*
PORT="${1:-$(ls /dev/cu.usbmodem* 2>/dev/null | head -1)}"
[ -n "$PORT" ] || { echo "找不到板子的串口, 请插上 USB 或手动指定"; exit 1; }
OUTPUT="factory_backup_16MB_$(date +%Y%m%d_%H%M%S).bin"

echo "=== 开始备份 ESP32-S3 原厂固件 (16MB) ==="
echo "目标串口: $PORT"
echo "保存路径: $OUTPUT"

~/.venv-esp/bin/esptool --port "$PORT" --baud 921600 read_flash 0x0 0x1000000 "$OUTPUT"

echo "=== 备份完成: $OUTPUT ==="
