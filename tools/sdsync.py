#!/usr/bin/env python3
"""
tools/sdsync.py
通过 USB 串口读写板子上 TF 卡的文件, 免拔卡 (固件端见 include/serial_fs.h)。
路径相对 TF 卡根目录, 如 spell/decks/manifest.json。

用法 (用 ~/.venv-esp/bin/python 运行, 需要 pyserial; 先确认串口没被占用):
  sdsync.py ls   [远端目录]
  sdsync.py get  <远端文件> <本地文件>
  sdsync.py put  <本地文件> <远端文件>
  sdsync.py rm   <远端路径>
  sdsync.py push <本地目录> <远端目录> [--delete]   只传有变化的文件; --delete 删除远端多出的文件
  sdsync.py reset                                    重启板子 (重新加载词库)
  sdsync.py time                                     显示板子时间 (每次连接都会先用 Mac 本地时间对时)
  sdsync.py log [-n 50] [--all] [--errors]           看设备日志最后几行; --all 含滚动出去的旧文件, --errors 只看错误/警告
"""
import argparse
import sys
import time
import zlib
from pathlib import Path

import serial
from serial.tools import list_ports

REPLY_TIMEOUT = 15.0
CHUNK = 4096  # 与固件 serial_fs.cpp 一致


class Board:
    def __init__(self, port):
        try:
            self.ser = serial.Serial(port, 115200, timeout=0.2)
        except serial.SerialException as e:
            sys.exit(f"打不开 {port}: {e}\n串口可能被占用, 先查 lsof {port}")
        time.sleep(0.1)
        self._sync()
        self.set_time()

    def set_time(self):
        """把 Mac 的本地时间写进板子 RTC (每次连接都顺便对时)"""
        now = time.time()
        local = int(now + time.localtime(now).tm_gmtoff)
        line, _ = self._cmd(f"TIME {local}")
        return line.split()[1]

    def _sync(self):
        """发 PING 并丢弃应答之前的一切 (上次中断的命令可能还在输出)"""
        token = f"{time.time_ns() & 0xFFFFFF:06x}"
        self.ser.reset_input_buffer()
        self.ser.write(f"#PING {token}\n".encode())
        deadline = time.time() + 30
        while time.time() < deadline:
            line = self.ser.readline().decode("utf-8", "replace").rstrip("\r\n")
            if line == f"#OK {token}":
                return
        raise TimeoutError("板子无应答 (固件是否已支持 serial_fs?)")

    def _reply(self, timeout=REPLY_TIMEOUT):
        """读到 #OK/#ERR/#READY 为止, 跳过混在中间的日志行; 返回 (首行, 之前的 # 数据行)"""
        rows = []
        deadline = time.time() + timeout
        while time.time() < deadline:
            raw = self.ser.readline()
            if not raw:
                continue
            deadline = time.time() + timeout  # 还在出数据就继续等 (列 400 多个文件的目录要十几秒)
            line = raw.decode("utf-8", "replace").rstrip("\r\n")
            if line.startswith(("#OK", "#ERR", "#READY")):
                return line, rows
            if line.startswith(("#F ", "#D ")):
                rows.append(line)
        raise TimeoutError("板子无应答 (固件是否已支持 serial_fs? 是否插着 TF 卡?)")

    def _wait_ack(self):
        deadline = time.time() + REPLY_TIMEOUT
        while time.time() < deadline:
            line = self.ser.readline().decode("utf-8", "replace").rstrip("\r\n")
            if line == "#A":
                return
            if line.startswith("#ERR"):
                raise RuntimeError(f"PUT 中断: {line[5:]}")
        raise TimeoutError("PUT 等待板子确认超时")

    def _cmd(self, text, timeout=REPLY_TIMEOUT):
        self.ser.write(f"#{text}\n".encode())
        line, rows = self._reply(timeout)
        if line.startswith("#ERR"):
            raise RuntimeError(f"{text.split()[0]} 失败: {line[5:]}")
        return line, rows

    def stat(self, path):
        try:
            line, _ = self._cmd(f"STAT {path}")
        except RuntimeError:
            return None
        _, size, crc = line.split()
        return int(size), int(crc, 16)

    def get(self, path):
        line, _ = self._cmd(f"GET {path}")
        _, size, crc = line.split()
        size, crc = int(size), int(crc, 16)
        data = bytearray()
        deadline = time.time() + REPLY_TIMEOUT + size / 50_000
        while len(data) < size and time.time() < deadline:
            data += self.ser.read(size - len(data))
        if len(data) != size or zlib.crc32(data) != crc:
            raise RuntimeError(f"GET {path} 数据不完整或 CRC 不符, 请重试")
        return bytes(data)

    def put(self, path, data, retries=2):
        crc = zlib.crc32(data)
        for attempt in range(retries + 1):
            line, _ = self._cmd(f"PUT {path} {len(data)} {crc:08x}")
            if not line.startswith("#READY"):
                raise RuntimeError(f"PUT {path}: 意外应答 {line}")
            # 每块之后等板子回 #A, 避免撑爆板子的串口接收缓冲
            for off in range(0, len(data), CHUNK):
                self.ser.write(data[off:off + CHUNK])
                if off + CHUNK < len(data):
                    self._wait_ack()
            line, _ = self._reply(REPLY_TIMEOUT + len(data) / 50_000)
            if line.startswith("#OK"):
                return
            if attempt == retries:
                raise RuntimeError(f"PUT {path} 失败: {line[5:]}")

    def ls(self, path):
        _, rows = self._cmd(f"LS {path}")
        out = []
        for r in rows:
            if r.startswith("#D "):
                out.append(("d", r[3:], 0))
            else:
                _, size, name = r.split(" ", 2)
                out.append(("f", name, int(size)))
        return out

    def rm(self, path):
        self._cmd(f"DEL {path}")

    def reset(self):
        self.ser.dtr = False
        self.ser.rts = True
        time.sleep(0.1)
        self.ser.rts = False


def walk_remote(board, root):
    """远端目录下所有文件的相对路径 -> 大小"""
    files = {}
    stack = [""]
    while stack:
        rel = stack.pop()
        base = f"{root}/{rel}".rstrip("/")
        for kind, name, size in board.ls(base):
            sub = f"{rel}/{name}".lstrip("/")
            if kind == "d":
                stack.append(sub)
            else:
                files[sub] = size
    return files


def push(board, local_dir, remote_dir, delete):
    local_dir = Path(local_dir)
    local = {p.relative_to(local_dir).as_posix(): p for p in sorted(local_dir.rglob("*"))
             if p.is_file() and not any(part.startswith(".") for part in p.relative_to(local_dir).parts)}
    try:
        remote = walk_remote(board, remote_dir)
    except RuntimeError:
        remote = {}  # 远端目录还不存在, PUT 会自动建
    sent = same = 0
    t0 = time.time()
    for rel, path in local.items():
        data = path.read_bytes()
        target = f"{remote_dir}/{rel}"
        if rel in remote and remote[rel] == len(data):
            st = board.stat(target)
            if st and st[1] == zlib.crc32(data):
                same += 1
                continue
        board.put(target, data)
        sent += 1
        print(f"  ↑ {target} ({len(data)} B)")
    removed = 0
    if delete:
        for rel in sorted(set(remote) - set(local)):
            board.rm(f"{remote_dir}/{rel}")
            removed += 1
            print(f"  ✕ {remote_dir}/{rel}")
    print(f"完成: 上传 {sent}, 未变 {same}, 删除 {removed}, 用时 {time.time() - t0:.1f}s")


def show_log(board, n, include_old, errors_only):
    text = ""
    names = (["spell/logs/device.1.log"] if include_old else []) + ["spell/logs/device.log"]
    for name in names:
        if board.stat(name) is not None:
            text += board.get(name).decode("utf-8", "replace")
    lines = text.splitlines()
    if errors_only:
        lines = [l for l in lines if " E " in l[:40] or " W " in l[:40]]
    if not lines:
        print("(没有日志)")
        return
    for line in lines[-n:] if n > 0 else lines:
        print(line)


def find_port():
    """板子的 USB 串口: macOS 上是 /dev/cu.usbmodem*, 插不同 USB 口时编号会变"""
    ports = [p.device for p in list_ports.comports() if "usbmodem" in p.device or "ttyACM" in p.device]
    if len(ports) != 1:
        sys.exit(f"找到 {len(ports)} 个候选串口 {ports}, 用 --port 指定")
    return ports[0]


def main():
    ap = argparse.ArgumentParser(description="通过 USB 串口读写板子 TF 卡")
    ap.add_argument("--port", help="串口, 默认自动查找")
    sub = ap.add_subparsers(dest="op", required=True)
    sub.add_parser("ls").add_argument("path", nargs="?", default="/")
    g = sub.add_parser("get"); g.add_argument("remote"); g.add_argument("local")
    p = sub.add_parser("put"); p.add_argument("local"); p.add_argument("remote")
    sub.add_parser("rm").add_argument("remote")
    s = sub.add_parser("push"); s.add_argument("local"); s.add_argument("remote")
    s.add_argument("--delete", action="store_true")
    sub.add_parser("reset")
    sub.add_parser("time")
    lg = sub.add_parser("log")
    lg.add_argument("-n", type=int, default=50, help="显示最后几行 (0 为全部)")
    lg.add_argument("--all", action="store_true", help="连同 device.1.log 一起看")
    lg.add_argument("--errors", action="store_true", help="只看 E/W 行")
    a = ap.parse_args()

    try:
        board = Board(a.port or find_port())
        if a.op == "ls":
            for kind, name, size in board.ls(a.path):
                print(f"{name}/" if kind == "d" else f"{size:>10}  {name}")
        elif a.op == "get":
            Path(a.local).write_bytes(board.get(a.remote))
        elif a.op == "put":
            board.put(a.remote, Path(a.local).read_bytes())
        elif a.op == "rm":
            board.rm(a.remote)
        elif a.op == "push":
            push(board, a.local, a.remote, a.delete)
        elif a.op == "time":
            line, _ = board._cmd("TIME")
            val = line.split()[1]
            if val == "invalid":
                print("板子时间无效")
            else:
                print("板子时间", time.strftime("%Y-%m-%d %H:%M:%S", time.gmtime(int(val))))
        elif a.op == "log":
            show_log(board, a.n, a.all, a.errors)
        elif a.op == "reset":
            board.reset()
    except (RuntimeError, TimeoutError) as e:
        sys.exit(str(e))


if __name__ == "__main__":
    main()
