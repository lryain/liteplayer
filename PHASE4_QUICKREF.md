# Phase 4 快速参考卡 - 测试和调试

## ✅ 当前状态

**Phase 3**: ✅ 100%完成（50/50测试通过）  
**Phase 4**: ✅ 100%完成（8/8测试通过）

---

## � 快速启动（5分钟流程）

```bash
# 1. 清理
pkill -9 -f music_player_server
rm -f /home/pi/.local/share/music-player/music_library.db
cd /home/pi/dev/nora-xiaozhi-dev/3rd/liteplayer
rm -f logs/server.log

# 2. 启动
./engine/build/music_player_server config/music_player.yaml > logs/server.log 2>&1 &
sleep 3

# 3. 测试
python3 scripts/test_client.py

# 4. 验证（重要！）
tail -50 logs/server.log

# 5. 停止
pkill -f music_player_server
```

---

## 📊 实时监控（双终端）

**终端1 - 启动和测试**：
```bash
cd /home/pi/dev/nora-xiaozhi-dev/3rd/liteplayer
pkill -9 -f music_player_server && sleep 1
rm -f /home/pi/.local/share/music-player/music_library.db
./engine/build/music_player_server config/music_player.yaml > logs/server.log 2>&1 &
sleep 3
python3 scripts/test_client.py
```

**终端2 - 监控日志**：
```bash
tail -f /home/pi/dev/nora-xiaozhi-dev/3rd/liteplayer/logs/server.log
```

---

## 🔍 关键日志位置

```
/home/pi/dev/nora-xiaozhi-dev/3rd/liteplayer/logs/server.log
```

查看最后50行：
```bash
tail -50 /home/pi/dev/nora-xiaozhi-dev/3rd/liteplayer/logs/server.log
```

实时监控：
```bash
tail -f /home/pi/dev/nora-xiaozhi-dev/3rd/liteplayer/logs/server.log
```

---

## 🔑 关键文件

| 文件 | 用途 |
|------|------|
| `engine/build/music_player_server` | 可执行文件 |
| `config/music_player.yaml` | 配置文件 |
| `scripts/test_client.py` | 测试客户端 |
| `logs/server.log` | 服务器日志 |
| `/home/pi/.local/share/music-player/music_library.db` | 数据库文件 |

---

## 📊 测试结果（2026-02-12）

```
✅ Test 1: Get Status - PASS
✅ Test 2: Add Track - PASS
✅ Test 3: Get Track - PASS
✅ Test 4: Get All Tracks - PASS
✅ Test 5: Search Tracks - PASS
✅ Test 6: Playback Control - PASS
✅ Test 7: Navigation Control - PASS
✅ Test 8: Event Subscription - PASS

总计: 8/8 PASS (100%)
```

**服务器日志确认**：
- ✅ Database opened successfully
- ✅ All commands received and processed
- ✅ Track added successfully
- ✅ No errors or failures

---

## ⚠️ 重要：每次测试必做

1. **杀死旧进程**: `pkill -9 -f music_player_server`
2. **清理数据库**: `rm -f /home/pi/.local/share/music-player/music_library.db`
3. **启动新实例**: `./engine/build/music_player_server config/music_player.yaml > logs/server.log 2>&1 &`
4. **等待初始化**: `sleep 3`
5. **运行测试**: `python3 scripts/test_client.py`
6. **检查日志**: `tail -50 logs/server.log` ← 这步最重要！

---

## � 已实现命令（10个）

**播放控制**（5个）：
- `play` - 开始播放
- `pause` - 暂停播放
- `stop` - 停止播放
- `next` - 下一首
- `previous` - 上一首

**查询功能**（3个）：
- `get_status` - 获取播放状态
- `get_track(track_id)` - 获取曲目信息
- `search_tracks(query, limit)` - 搜索曲目

**库管理**（2个）：
- `add_track(...)` - 添加曲目
- `get_all_tracks(limit)` - 获取曲目列表

---

## 📡 已实现事件（2个）

- `track_added` - 曲目添加事件
- `heartbeat` - 心跳事件（5秒间隔）
  "data": {"track_id": 123, "title": "Song"},
  "timestamp": 1707734400
}
```

---

## 🧪 测试命令

---

**最后更新**: 2026-02-12  
**测试状态**: ✅ 全部通过  
**日志验证**: ✅ 已确认  
**下一步**: 生产部署或功能扩展

---

� 完整指南：请阅读 `docs/16_Phase4_完成报告.md` 和 `docs/DEBUG_GUIDE.md`
