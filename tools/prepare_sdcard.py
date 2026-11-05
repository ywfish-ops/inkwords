#!/usr/bin/env python3
"""
从两份 HTML 词表源文件 (内嵌 <script id="DATA_*"> JSON, 目录由命令行传入) 提取词库 (spbcn_r1, spbcn_r2, spbcn_r3) 与 412 个发音 MP3，
构建可直接拷入 TF 卡根目录的 sdcard_dist/spell/ 数据包。
"""

import argparse
import json
import os
import re
import shutil
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
DIST_SPELL_DIR = PROJECT_ROOT / "sdcard_dist" / "spell"


def audio_slug(word: str) -> str:
    s = word.strip().lower().replace(" ", "_")
    return re.sub(r"[^\w\-]", "", s)


def extract_script_json(html_text: str, script_id: str) -> list:
    pattern = rf'<script id="{re.escape(script_id)}" type="application/json">(.*?)</script>'
    m = re.search(pattern, html_text, re.S)
    if not m:
        raise ValueError(f"未找到 <script id='{script_id}'>")
    return json.loads(m.group(1))


def clean_text(text):
    """源数据里有复制时留下的断行 (源页面上看不出, 墨水屏会在句中换行): 连续空白合并成一个空格"""
    return " ".join(text.split())


def main():
    ap = argparse.ArgumentParser(description="从词表源文件生成 TF 卡数据包 sdcard_dist/spell/")
    ap.add_argument("--full", type=Path, required=True, help="完整词表目录 (index.html 内含完整例句与英文释义)")
    ap.add_argument("--blank", type=Path, required=True, help="挖空例句目录 (index.html 内含挖空例句, audio/ 内含 MP3)")
    args = ap.parse_args()
    blank_dir, full_dir = args.blank, args.full

    print("=== 开始构建 TF 卡数据包 (sdcard_dist/spell/) ===")
    decks_dir = DIST_SPELL_DIR / "decks"
    audio_dir = DIST_SPELL_DIR / "audio"
    userdata_dir = DIST_SPELL_DIR / "userdata"

    for d in (decks_dir, audio_dir, userdata_dir):
        d.mkdir(parents=True, exist_ok=True)

    full_html = (full_dir / "index.html").read_text(encoding="utf-8")
    blank_html = (blank_dir / "index.html").read_text(encoding="utf-8")

    # 提取完整词表 (完整例句与英文释义来自 --full, 挖空例句来自 --blank)
    blank_map = {}
    for sid in ("DATA_R1", "DATA_R2", "DATA_R3"):
        for row in extract_script_json(blank_html, sid):
            # row: [en, zh, ex_blanked, cat, ipa]
            blank_map[row[0].strip().lower()] = row[2]

    deck_specs = [
        ("spbcn_r1", "VOCAB_SPBCN_R1", "SPBCN R1 单音节", "拼字大会 · 小高组 · Round 1 单音节"),
        ("spbcn_r2", "VOCAB_SPBCN_R2", "SPBCN R2 双音节", "拼字大会 · 小高组 · Round 2 双音节"),
        ("spbcn_r3", "VOCAB_SPBCN_R3", "SPBCN R3 多音节", "拼字大会 · 小高组 · Round 3 多音节"),
    ]

    manifest = []
    for deck_id, script_id, name, sub in deck_specs:
        raw_rows = extract_script_json(full_html, script_id)
        cards = []
        for idx, r in enumerate(raw_rows):
            en = r[0]
            ipa = r[1]
            zh = r[2]
            ex = r[3]
            cat = r[4]
            def_en = r[5] if len(r) > 5 else ""
            slug = audio_slug(en)
            cards.append({
                "key": f"{deck_id}-{idx}",
                "en": en,
                "ipa": ipa,
                "zh": zh,
                "ex": clean_text(ex),
                "ex_blank": clean_text(blank_map.get(en.strip().lower(), "")),
                "cat": cat,
                "def": clean_text(def_en),
                "audio": f"{slug}.mp3"
            })

        deck_obj = {
            "id": deck_id,
            "name": name,
            "sub": sub,
            "count": len(cards),
            "cards": cards
        }
        out_file = decks_dir / f"{deck_id}.json"
        out_file.write_text(json.dumps(deck_obj, ensure_ascii=False, indent=2), encoding="utf-8")
        manifest.append({"id": deck_id, "name": name, "sub": sub, "count": len(cards), "file": f"{deck_id}.json"})
        print(f"  [✓] 导出词库 {deck_id}.json ({len(cards)} 词)")

    (decks_dir / "manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")

    # 拷贝 412 个 MP3 音频文件
    src_audio_dir = blank_dir / "audio"
    mp3_count = 0
    for mp3_file in sorted(src_audio_dir.glob("*.mp3")):
        shutil.copy2(mp3_file, audio_dir / mp3_file.name)
        mp3_count += 1
    print(f"  [✓] 同步音频文件: {mp3_count} 个 MP3 -> {audio_dir}")

    # 初始化默认 settings.json
    settings_path = userdata_dir / "settings.json"
    if not settings_path.exists():
        settings_path.write_text(json.dumps({
            "deck": "spbcn_r1",
            "cat": "all"
        }, ensure_ascii=False, indent=2), encoding="utf-8")
        print(f"  [✓] 创建默认配置: {settings_path}")

    print(f"=== 构建完成! 请将 {DIST_SPELL_DIR} 拷贝至 TF 卡根目录 (/sdcard/spell) ===")


if __name__ == "__main__":
    main()
