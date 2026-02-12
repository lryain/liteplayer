#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""为 liteplayer 数据库中的曲目打“多维度情绪标签”。

特点：
- 纯本地：不依赖在线模型/外部 API（避免密钥/网络不确定性）
- 支持交互式手工标注，也支持从 YAML 映射批量推断
- 标签存 SQLite，便于服务端按情绪检索/播放

用法示例：
- 交互式：python3 tag_tracks.py --db data/music_library.db --interactive
- 批量推断：python3 tag_tracks.py --db data/music_library.db --auto
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sqlite3
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional, Tuple

try:
    import yaml  # type: ignore
except Exception:
    yaml = None


DEFAULT_DIMENSIONS = [
    "valence",   # -1..1
    "arousal",   # 0..1
    "energy",    # 0..1
    "mood",      # 离散：happy/sad/...（可选）
    "tags",      # 任意标签 list[str]
]


@dataclass
class TrackRow:
    id: int
    file_path: str
    title: str
    artist: str
    album: str


def connect_db(db_path: str) -> sqlite3.Connection:
    conn = sqlite3.connect(db_path)
    conn.row_factory = sqlite3.Row
    return conn


def ensure_schema(conn: sqlite3.Connection) -> None:
    conn.executescript(
        """
        CREATE TABLE IF NOT EXISTS track_emotions (
            track_id INTEGER PRIMARY KEY,
            valence REAL DEFAULT 0.0,
            arousal REAL DEFAULT 0.3,
            energy REAL DEFAULT 0.7,
            mood TEXT,
            tags_json TEXT,
            updated_at INTEGER DEFAULT 0,
            FOREIGN KEY(track_id) REFERENCES tracks(id) ON DELETE CASCADE
        );
        CREATE INDEX IF NOT EXISTS idx_track_emotions_mood ON track_emotions(mood);
        """
    )
    conn.commit()


def _has_column(conn: sqlite3.Connection, table: str, col: str) -> bool:
    rows = conn.execute(f"PRAGMA table_info({table})").fetchall()
    for r in rows:
        # row: (cid, name, type, notnull, dflt_value, pk)
        if len(r) > 1 and str(r[1]) == col:
            return True
    return False


def list_tracks(conn: sqlite3.Connection, limit: int = 2000, include_bad: bool = False) -> List[TrackRow]:
    # 兼容旧数据库：可能还没有 bad_flag 字段
    if include_bad:
        where = "1=1"
    else:
        where = "COALESCE(bad_flag,0)=0" if _has_column(conn, "tracks", "bad_flag") else "1=1"
    sql = f"""
        SELECT id,
               file_path,
               title,
               COALESCE(artist,'') AS artist,
               COALESCE(album,'') AS album
        FROM tracks
        WHERE {where}
        ORDER BY title
        LIMIT ?
    """
    rows = conn.execute(sql, (limit,)).fetchall()
    out: List[TrackRow] = []
    for r in rows:
        out.append(
            TrackRow(
                id=int(r["id"]),
                file_path=str(r["file_path"]),
                title=str(r["title"]),
                artist=str(r["artist"]),
                album=str(r["album"]),
            )
        )
    return out


def upsert_emotion(
    conn: sqlite3.Connection,
    track_id: int,
    valence: float,
    arousal: float,
    energy: float,
    mood: Optional[str],
    tags: List[str],
) -> None:
    conn.execute(
        """
        INSERT INTO track_emotions(track_id, valence, arousal, energy, mood, tags_json, updated_at)
        VALUES(?,?,?,?,?,?,?)
        ON CONFLICT(track_id) DO UPDATE SET
            valence=excluded.valence,
            arousal=excluded.arousal,
            energy=excluded.energy,
            mood=excluded.mood,
            tags_json=excluded.tags_json,
            updated_at=excluded.updated_at
        """,
        (
            track_id,
            float(valence),
            float(arousal),
            float(energy),
            mood,
            json.dumps(tags, ensure_ascii=False),
            int(time.time()),
        ),
    )
    conn.commit()


def clamp(v: float, lo: float, hi: float) -> float:
    return max(lo, min(hi, v))


def parse_float(s: str, default: float) -> float:
    try:
        return float(s)
    except Exception:
        return default


def load_emotion_mapping_yaml(path: str) -> Dict[str, Dict]:
    if yaml is None:
        raise RuntimeError("PyYAML not available. Please install pyyaml or use --interactive.")
    with open(path, "r", encoding="utf-8") as f:
        data = yaml.safe_load(f) or {}
    return data.get("emotion_mapping", {})


def guess_from_text(text: str, mapping: Dict[str, Dict]) -> Optional[str]:
    t = text.lower()
    for emo in mapping.keys():
        if emo.lower() in t:
            return emo
    return None


def mood_to_vae(mood: str) -> Tuple[float, float, float]:
    # 经验映射：可后续改为更精细的配置文件
    table = {
        "happy": (0.8, 0.7, 0.8),
        "laughing": (0.9, 0.9, 0.9),
        "excited": (0.8, 0.95, 0.95),
        "playful": (0.7, 0.8, 0.8),
        "calm": (0.3, 0.2, 0.6),
        "relaxed": (0.4, 0.2, 0.6),
        "sad": (-0.7, 0.3, 0.3),
        "crying": (-0.8, 0.6, 0.2),
        "angry": (-0.6, 0.85, 0.7),
        "neutral": (0.0, 0.3, 0.7),
        "thinking": (0.1, 0.4, 0.6),
        "confused": (-0.1, 0.5, 0.5),
        "surprised": (0.2, 0.9, 0.7),
        "shocked": (0.0, 1.0, 0.7),
        "tired": (-0.2, 0.1, 0.2),
    }
    return table.get(mood, (0.0, 0.3, 0.7))


def interactive_tag(conn: sqlite3.Connection, tracks: List[TrackRow]) -> None:
    print("\n交互式标注：输入 q 退出，回车使用默认值。\n")
    for i, tr in enumerate(tracks, 1):
        print(f"[{i}/{len(tracks)}] {tr.title} | {tr.file_path}")
        mood = input("  mood (e.g. happy/sad/neutral): ").strip()
        if mood.lower() == "q":
            return
        if not mood:
            mood = "neutral"

        default_v, default_a, default_e = mood_to_vae(mood)
        v = clamp(parse_float(input(f"  valence [-1..1] (default {default_v}): ").strip(), default_v), -1.0, 1.0)
        a = clamp(parse_float(input(f"  arousal [0..1] (default {default_a}): ").strip(), default_a), 0.0, 1.0)
        e = clamp(parse_float(input(f"  energy [0..1] (default {default_e}): ").strip(), default_e), 0.0, 1.0)

        tags_raw = input("  tags (comma-separated, optional): ").strip()
        tags = [t.strip() for t in tags_raw.split(",") if t.strip()] if tags_raw else []

        upsert_emotion(conn, tr.id, v, a, e, mood, tags)
        print("  ✅ saved\n")


def auto_tag(conn: sqlite3.Connection, tracks: List[TrackRow], mapping_yaml: str) -> None:
    mapping = load_emotion_mapping_yaml(mapping_yaml)
    hit = 0
    for tr in tracks:
        guess = guess_from_text(tr.title + " " + Path(tr.file_path).name, mapping)
        if not guess:
            continue
        v, a, e = mood_to_vae(guess)
        upsert_emotion(conn, tr.id, v, a, e, guess, ["auto"])
        hit += 1
    print(f"auto-tag done: {hit}/{len(tracks)} tracks")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--db", required=True, help="SQLite db path, e.g. data/music_library.db")
    ap.add_argument("--limit", type=int, default=2000)
    ap.add_argument("--include-bad", action="store_true")
    ap.add_argument("--interactive", action="store_true")
    ap.add_argument("--auto", action="store_true")
    ap.add_argument("--mapping", default="/home/pi/dev/nora-xiaozhi-dev/config/xiaozhi_emotion_mapping.yaml")
    args = ap.parse_args()

    conn = connect_db(args.db)
    ensure_schema(conn)
    tracks = list_tracks(conn, limit=args.limit, include_bad=args.include_bad)

    if args.interactive:
        interactive_tag(conn, tracks)
        return 0
    if args.auto:
        auto_tag(conn, tracks, args.mapping)
        return 0

    print("Nothing to do. Use --interactive or --auto")
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
