# MusicPlayerEngine ZMQ 接口规范

## 概述

MusicPlayerEngine 使用 ZMQ PUB/SUB 模式与 Doly 系统的其他模块通信。

- **架构模式**: Publisher-Subscriber (发布-订阅)
- **消息格式**: JSON
- **传输协议**: IPC (Unix Domain Socket)

---

## 1. 端点配置

### 1.1 命令订阅端点 (Command Subscriber)

MusicPlayerEngine **订阅**此端点以接收控制命令:

```
ipc:///tmp/music_player_cmd.sock
```

### 1.2 事件发布端点 (Event Publisher)

MusicPlayerEngine **发布**播放事件到此端点:

```
ipc:///tmp/music_player_event.sock
```

---

## 2. 消息话题 (Topics)

### 2.1 命令话题

| 话题 | 描述 | 示例 |
|-----|------|------|
| `music.cmd.control` | 播放控制命令 | play, pause, stop, next, prev |
| `music.cmd.playlist` | 播放列表管理 | set_playlist, add_track |
| `music.cmd.mode` | 播放模式设置 | set_play_mode |
| `music.cmd.query` | 查询命令 | search_tracks, get_current_track |
| `music.cmd.library` | 库管理命令 | scan_directory, rescan_all |
| `music.cmd.tag` | 标签管理命令 | add_tag, remove_tag |

### 2.2 事件话题

| 话题 | 描述 | 触发时机 |
|-----|------|---------|
| `music.event.state` | 播放状态变化 | 状态改变时 |
| `music.event.progress` | 播放进度更新 | 每秒更新 |
| `music.event.track` | 曲目变化 | 切换曲目时 |
| `music.event.error` | 错误事件 | 发生错误时 |
| `music.event.library` | 库更新事件 | 扫描完成时 |

---

## 3. 命令消息详细规范

### 3.1 播放控制命令

#### 3.1.1 播放 (play)

**话题**: `music.cmd.control`

```json
{
    "cmd": "play",
    "params": {
        "track_id": 123
    }
}
```

**参数**:
- `track_id` (可选): 指定要播放的曲目ID。如果省略,则播放当前曲目或播放列表的第一首。

**响应事件**: `music.event.state` (state="loading" -> "playing")

---

#### 3.1.2 暂停 (pause)

**话题**: `music.cmd.control`

```json
{
    "cmd": "pause"
}
```

**参数**: 无

**响应事件**: `music.event.state` (state="paused")

---

#### 3.1.3 继续 (resume)

**话题**: `music.cmd.control`

```json
{
    "cmd": "resume"
}
```

**参数**: 无

**响应事件**: `music.event.state` (state="playing")

---

#### 3.1.4 停止 (stop)

**话题**: `music.cmd.control`

```json
{
    "cmd": "stop"
}
```

**参数**: 无

**响应事件**: `music.event.state` (state="stopped")

---

#### 3.1.5 下一首 (next)

**话题**: `music.cmd.control`

```json
{
    "cmd": "next"
}
```

**参数**: 无

**响应事件**: `music.event.track` (track_changed)

---

#### 3.1.6 上一首 (prev)

**话题**: `music.cmd.control`

```json
{
    "cmd": "prev"
}
```

**参数**: 无

**响应事件**: `music.event.track` (track_changed)

---

#### 3.1.7 跳转 (seek)

**话题**: `music.cmd.control`

```json
{
    "cmd": "seek",
    "params": {
        "position_ms": 30000
    }
}
```

**参数**:
- `position_ms` (必需): 目标位置(毫秒)

**响应事件**: `music.event.progress`

---

### 3.2 播放列表管理

#### 3.2.1 设置播放列表

**话题**: `music.cmd.playlist`

```json
{
    "cmd": "set_playlist",
    "params": {
        "playlist_id": 1
    }
}
```

或者直接指定曲目ID列表:

```json
{
    "cmd": "set_playlist",
    "params": {
        "track_ids": [1, 5, 10, 15]
    }
}
```

---

#### 3.2.2 添加曲目

**话题**: `music.cmd.playlist`

```json
{
    "cmd": "add_track",
    "params": {
        "track_id": 123,
        "position": 5
    }
}
```

**参数**:
- `track_id` (必需): 曲目ID
- `position` (可选): 插入位置,默认添加到末尾

---

#### 3.2.3 移除曲目

**话题**: `music.cmd.playlist`

```json
{
    "cmd": "remove_track",
    "params": {
        "track_id": 123
    }
}
```

---

#### 3.2.4 清空播放列表

**话题**: `music.cmd.playlist`

```json
{
    "cmd": "clear"
}
```

---

#### 3.2.5 随机打乱

**话题**: `music.cmd.playlist`

```json
{
    "cmd": "shuffle"
}
```

---

### 3.3 播放模式

#### 3.3.1 设置播放模式

**话题**: `music.cmd.mode`

```json
{
    "cmd": "set_play_mode",
    "params": {
        "mode": "random"
    }
}
```

**参数**:
- `mode` (必需): 播放模式
  - `"sequential"` - 顺序播放
  - `"loop_all"` - 列表循环
  - `"random"` - 随机播放
  - `"single_loop"` - 单曲循环

---

### 3.4 查询命令

#### 3.4.1 搜索曲目

**话题**: `music.cmd.query`

```json
{
    "cmd": "search_tracks",
    "params": {
        "keyword": "周杰伦",
        "fields": ["title", "artist", "album"]
    }
}
```

**参数**:
- `keyword` (必需): 搜索关键词
- `fields` (可选): 搜索字段,默认全部字段

**响应**: 通过 `music.event.query_result` 返回结果

---

#### 3.4.2 按标签筛选

**话题**: `music.cmd.query`

```json
{
    "cmd": "filter_by_tags",
    "params": {
        "tags": ["relax", "evening"],
        "match_all": false
    }
}
```

**参数**:
- `tags` (必需): 标签列表
- `match_all` (可选): true=匹配所有标签, false=匹配任一标签(默认)

---

#### 3.4.3 获取当前曲目

**话题**: `music.cmd.query`

```json
{
    "cmd": "get_current_track"
}
```

**响应**: `music.event.query_result`

---

#### 3.4.4 获取播放列表

**话题**: `music.cmd.query`

```json
{
    "cmd": "get_playlists"
}
```

---

### 3.5 库管理

#### 3.5.1 扫描目录

**话题**: `music.cmd.library`

```json
{
    "cmd": "scan_directory",
    "params": {
        "path": "/home/pi/Music"
    }
}
```

**响应事件**: `music.event.library` (scan_started, scan_completed)

---

#### 3.5.2 重新扫描所有

**话题**: `music.cmd.library`

```json
{
    "cmd": "rescan_all"
}
```

---

### 3.6 标签管理

#### 3.6.1 添加标签到曲目

**话题**: `music.cmd.tag`

```json
{
    "cmd": "add_tag_to_track",
    "params": {
        "track_id": 123,
        "tag": "relax"
    }
}
```

---

#### 3.6.2 从曲目移除标签

**话题**: `music.cmd.tag`

```json
{
    "cmd": "remove_tag_from_track",
    "params": {
        "track_id": 123,
        "tag": "relax"
    }
}
```

---

## 4. 事件消息详细规范

### 4.1 播放状态变化

**话题**: `music.event.state`

```json
{
    "event": "state_changed",
    "timestamp": 1707680400,
    "data": {
        "state": "playing",
        "track_id": 123,
        "track_title": "稻香",
        "artist": "周杰伦",
        "album": "魔杰座"
    }
}
```

**状态值**:
- `"idle"` - 空闲
- `"loading"` - 加载中
- `"playing"` - 播放中
- `"paused"` - 已暂停
- `"stopped"` - 已停止
- `"error"` - 错误

---

### 4.2 播放进度

**话题**: `music.event.progress`

**频率**: 每秒发送一次

```json
{
    "event": "progress",
    "timestamp": 1707680400,
    "data": {
        "track_id": 123,
        "position_ms": 30000,
        "duration_ms": 240000,
        "percentage": 12.5
    }
}
```

---

### 4.3 曲目变化

**话题**: `music.event.track`

```json
{
    "event": "track_changed",
    "timestamp": 1707680400,
    "data": {
        "previous_track_id": 122,
        "current_track_id": 123,
        "track": {
            "id": 123,
            "title": "稻香",
            "artist": "周杰伦",
            "album": "魔杰座",
            "year": 2008,
            "genre": "流行",
            "duration_ms": 240000,
            "file_path": "/home/pi/Music/稻香.mp3",
            "tags": ["relax", "evening"]
        }
    }
}
```

---

### 4.4 错误事件

**话题**: `music.event.error`

```json
{
    "event": "error",
    "timestamp": 1707680400,
    "data": {
        "error_code": "FILE_NOT_FOUND",
        "error_message": "音频文件不存在: /home/pi/Music/missing.mp3",
        "track_id": 123
    }
}
```

**错误代码**:
- `FILE_NOT_FOUND` - 文件不存在
- `DECODE_ERROR` - 解码失败
- `PLAYBACK_ERROR` - 播放错误
- `LIBRARY_ERROR` - 库错误
- `ZMQ_ERROR` - ZMQ通信错误

---

### 4.5 库更新事件

**话题**: `music.event.library`

```json
{
    "event": "scan_completed",
    "timestamp": 1707680400,
    "data": {
        "tracks_added": 150,
        "tracks_updated": 10,
        "tracks_removed": 5,
        "scan_duration_ms": 5000,
        "total_tracks": 1000
    }
}
```

---

### 4.6 查询结果

**话题**: `music.event.query_result`

```json
{
    "event": "query_result",
    "timestamp": 1707680400,
    "data": {
        "query_id": "uuid-1234",
        "query_type": "search_tracks",
        "results": [
            {
                "id": 123,
                "title": "稻香",
                "artist": "周杰伦",
                "album": "魔杰座",
                "duration_ms": 240000
            }
        ],
        "total_count": 1
    }
}
```

---

## 5. 使用示例

### 5.1 Python 客户端示例

```python
import zmq
import json
import time

# 创建ZMQ上下文
context = zmq.Context()

# 发布命令
cmd_socket = context.socket(zmq.PUB)
cmd_socket.connect("ipc:///tmp/music_player_cmd.sock")

# 订阅事件
event_socket = context.socket(zmq.SUB)
event_socket.connect("ipc:///tmp/music_player_event.sock")
event_socket.setsockopt_string(zmq.SUBSCRIBE, "music.event.")

# 等待连接建立
time.sleep(0.5)

# 发送播放命令
cmd = {
    "cmd": "play",
    "params": {
        "track_id": 123
    }
}
topic = "music.cmd.control"
message = json.dumps(cmd)
cmd_socket.send_string(f"{topic} {message}")

# 接收事件
while True:
    try:
        msg = event_socket.recv_string(zmq.NOBLOCK)
        parts = msg.split(' ', 1)
        topic = parts[0]
        data = json.loads(parts[1])
        print(f"[{topic}] {data}")
    except zmq.Again:
        time.sleep(0.1)
```

### 5.2 命令行测试工具

可以使用 `scripts/music_player_cli.py` 进行测试:

```bash
# 播放指定曲目
./scripts/music_player_cli.py play --track-id 123

# 暂停
./scripts/music_player_cli.py pause

# 下一首
./scripts/music_player_cli.py next

# 搜索
./scripts/music_player_cli.py search "周杰伦"

# 监听事件
./scripts/music_player_cli.py listen
```

---

## 6. 集成建议

### 6.1 在 Doly daemon.py 中集成

```python
import zmq
import json

class DolyDaemon:
    def __init__(self):
        self.context = zmq.Context()
        
        # 订阅音乐播放器事件
        self.music_event_sub = self.context.socket(zmq.SUB)
        self.music_event_sub.connect("ipc:///tmp/music_player_event.sock")
        self.music_event_sub.setsockopt_string(zmq.SUBSCRIBE, "music.event.")
        
        # 发布音乐播放器命令
        self.music_cmd_pub = self.context.socket(zmq.PUB)
        self.music_cmd_pub.bind("ipc:///tmp/music_player_cmd.sock")
        
    def handle_voice_command_play_music(self, artist=None, title=None):
        """处理语音命令:播放音乐"""
        if artist or title:
            # 先搜索
            cmd = {
                "cmd": "search_tracks",
                "params": {
                    "keyword": f"{artist} {title}",
                    "fields": ["artist", "title"]
                }
            }
            self.send_music_command("music.cmd.query", cmd)
        else:
            # 直接播放
            cmd = {"cmd": "play"}
            self.send_music_command("music.cmd.control", cmd)
    
    def send_music_command(self, topic, cmd):
        message = json.dumps(cmd)
        self.music_cmd_pub.send_string(f"{topic} {message}")
    
    def process_music_events(self):
        """处理音乐播放器事件"""
        try:
            msg = self.music_event_sub.recv_string(zmq.NOBLOCK)
            parts = msg.split(' ', 1)
            topic = parts[0]
            data = json.loads(parts[1])
            
            if topic == "music.event.track":
                # 曲目变化,可以在眼睛上显示歌曲信息
                track = data['data']['track']
                self.display_track_info(track)
            elif topic == "music.event.error":
                # 播放错误,显示错误动画
                self.show_error_animation()
        except zmq.Again:
            pass
```

---

## 7. 测试清单

- [ ] 播放控制命令(play, pause, resume, stop, next, prev, seek)
- [ ] 播放列表管理(set_playlist, add_track, remove_track, clear, shuffle)
- [ ] 播放模式切换(sequential, loop_all, random, single_loop)
- [ ] 查询命令(search, filter, get_current_track)
- [ ] 库管理(scan_directory, rescan_all)
- [ ] 标签管理(add_tag, remove_tag)
- [ ] 状态事件接收
- [ ] 进度事件接收
- [ ] 曲目变化事件接收
- [ ] 错误事件接收
- [ ] 库更新事件接收
- [ ] 并发命令处理
- [ ] 异常情况处理

---

**版本**: v1.0  
**更新日期**: 2026-02-11
