# Phase 4 服务器调试指南

> 📌 本指南用于调试 ZMQ 音乐播放器服务
> 👤 作者：GitHub Copilot
> 📅 最后更新：2026-02-12

---

## ⚠️ 关键经验教训

### 问题1: 测试失败但不知道真正原因

**症状**：
```
❌ add_track 返回 error: Failed to add track
```

**错误做法**：
❌ 只看客户端输出，假设测试通过了

**正确做法**：
✅ **必须查看服务器日志！**
```bash
tail -50 /home/pi/dev/nora-xiaozhi-dev/3rd/liteplayer/logs/server.log
```

**发现的真实错误**：
```
[MusicLibrary] Insert failed: UNIQUE constraint failed: tracks.file_path
```

### 问题2: 累积的测试数据导致冲突

**症状**：第一次测试通过，第二次测试失败

**根本原因**：数据库中已有相同的 `file_path` 数据

**解决方案**：每次测试前清理数据库
```bash
rm -f /home/pi/.local/share/music-player/music_library.db
```

---

## 🔧 完整调试工作流

### 第1步: 清理环境

```bash
# 停止所有现有进程
pkill -9 -f music_player_server

# 等待进程完全退出
sleep 2

# 验证进程已停止
ps aux | grep music_player_server | grep -v grep
# 应该无输出

# 清理旧数据库
rm -f /home/pi/.local/share/music-player/music_library.db

# 清理旧日志
cd /home/pi/dev/nora-xiaozhi-dev/3rd/liteplayer
rm -f logs/server.log
```

### 第2步: 启动服务器（带日志输出）

```bash
cd /home/pi/dev/nora-xiaozhi-dev/3rd/liteplayer

# 方式A: 后台运行（推荐）
./engine/build/music_player_server config/music_player.yaml > logs/server.log 2>&1 &

# 等待服务器初始化
sleep 3

# 验证进程运行
ps aux | grep music_player_server | grep -v grep
```

**预期输出**：
```
[Server] Using config file: config/music_player.yaml
[ConfigLoader] Configuration loaded successfully
[MusicPlayerService] ZMQ initialized
[MusicLibrary] Database opened: /home/pi/.local/share/music-player/music_library.db
[MusicPlayerService] Service started
```

### 第3步: 运行测试客户端

```bash
cd /home/pi/dev/nora-xiaozhi-dev/3rd/liteplayer
python3 scripts/test_client.py
```

### 第4步: 实时监控日志（在另一个终端）

```bash
# 打开第二个终端，实时查看日志
tail -f /home/pi/dev/nora-xiaozhi-dev/3rd/liteplayer/logs/server.log
```

**预期日志流**：
```
[MusicPlayerService] Received command: get_status
[MusicPlayerService] Received command: add_track
[MusicLibrary] Added track: Test Song (ID: 1)  ← 关键！
[MusicPlayerService] Received command: get_track
[MusicPlayerService] Received command: get_all_tracks
[MusicPlayerService] Received command: search_tracks
[MusicPlayerService] Received command: play
[MusicPlayerService] Received command: pause
[MusicPlayerService] Received command: stop
[MusicPlayerService] Received command: next
[MusicPlayerService] Received command: previous
```

### 第5步: 分析错误

如果测试失败，查找错误日志：

```bash
# 查找所有错误
grep -i "error\|failed\|exception" logs/server.log

# 查找特定错误
grep "constraint" logs/server.log          # 数据库约束
grep "ZMQ" logs/server.log                 # ZMQ相关
grep "command" logs/server.log             # 命令处理
```

### 第6步: 停止服务器

```bash
pkill -f music_player_server
sleep 1
echo "✅ 服务器已停止"
```

---

## 📋 常见错误及解决方案

### ❌ 错误1: UNIQUE constraint failed

```
[MusicLibrary] Insert failed: UNIQUE constraint failed: tracks.file_path
```

**原因**: 尝试添加已存在的曲目  
**解决**: 
```bash
rm -f /home/pi/.local/share/music-player/music_library.db
```

### ❌ 错误2: Database failed to open

```
[MusicLibrary] Failed to open database
```

**原因**: 目录不存在或权限不足  
**解决**:
```bash
mkdir -p /home/pi/.local/share/music-player
chmod 755 /home/pi/.local/share/music-player
```

### ❌ 错误3: ZMQ connection failed

```
❌ Connection refused
zmq.error.ZMQError: Connection refused
```

**原因**: 服务器未运行或 socket 文件不存在  
**解决**:
```bash
# 检查进程
ps aux | grep music_player_server

# 检查 socket 文件
ls -la /tmp/music_player_*.sock

# 重启服务器
pkill -f music_player_server
sleep 2
./engine/build/music_player_server config/music_player.yaml > logs/server.log 2>&1 &
```

### ❌ 错误4: No response from server (timeout)

```
Response timeout - no data received
```

**原因**: 
1. 服务器卡顿
2. 命令处理耗时过长
3. 多线程竞态条件

**排查步骤**:
```bash
# 1. 检查日志中是否有命令记录
tail -20 logs/server.log | grep "Received command"

# 2. 检查服务器进程状态
ps aux | grep music_player_server

# 3. 检查 CPU 使用率（无限循环）
top -p $(pgrep music_player_server)

# 4. 重启服务器
pkill -9 -f music_player_server
sleep 2
./engine/build/music_player_server config/music_player.yaml > logs/server.log 2>&1 &
```

---

## 🔍 关键日志关键字

### 成功指标

| 关键字 | 含义 | 期望值 |
|--------|------|--------|
| `Configuration loaded successfully` | 配置加载 | ✅ 出现 |
| `ZMQ initialized` | ZMQ初始化 | ✅ 出现 |
| `Database opened` | 数据库打开 | ✅ 出现 |
| `Service started` | 服务启动完成 | ✅ 出现 |
| `Received command:` | 命令接收 | ✅ 每次请求出现 |
| `Added track:` | 曲目添加 | ✅ add_track时出现 |

### 错误指标

| 关键字 | 含义 | 需要行动 |
|--------|------|--------|
| `failed` | 操作失败 | ❌ 需调查 |
| `error` | 错误 | ❌ 需调查 |
| `constraint` | 数据库约束 | ❌ 清理数据库 |
| `ZMQ.*error` | ZMQ错误 | ❌ 重启服务 |
| `exception` | 异常 | ❌ 检查代码 |

---

## 📊 日志采集工具

### 快速检查脚本

```bash
#!/bin/bash
# save as check_music_server.sh

echo "=== 进程检查 ==="
ps aux | grep music_player_server | grep -v grep || echo "❌ 服务未运行"

echo -e "\n=== 数据库检查 ==="
ls -lh /home/pi/.local/share/music-player/music_library.db 2>/dev/null || echo "❌ 数据库不存在"

echo -e "\n=== Socket 检查 ==="
ls -la /tmp/music_player_*.sock 2>/dev/null || echo "❌ Socket 文件不存在"

echo -e "\n=== 日志最后20行 ==="
tail -20 /home/pi/dev/nora-xiaozhi-dev/3rd/liteplayer/logs/server.log

echo -e "\n=== 错误检查 ==="
grep -i "error\|failed" /home/pi/dev/nora-xiaozhi-dev/3rd/liteplayer/logs/server.log || echo "✅ 无错误"
```

使用方法：
```bash
chmod +x check_music_server.sh
./check_music_server.sh
```

---

## 🎯 测试检查清单

启动测试前，请确认以下事项：

- [ ] 清理了旧的 `music_player_server` 进程
- [ ] 删除了旧的数据库文件
- [ ] 删除了旧的日志文件
- [ ] 启动了新的服务器实例
- [ ] 等待了至少3秒以确保初始化完成
- [ ] 验证了进程正在运行
- [ ] 准备好了监控日志的终端
- [ ] 运行了测试客户端
- [ ] 检查了服务器日志中的关键操作
- [ ] 查找了任何错误信息
- [ ] 停止了测试后的服务器

---

## 🚨 紧急恢复

如果服务器卡住或行为异常：

```bash
# 强制杀死所有 music_player 相关进程
pkill -9 -f music_player

# 清理 IPC socket 文件
rm -f /tmp/music_player_*.sock

# 可选：清理数据库
rm -f /home/pi/.local/share/music-player/music_library.db

# 重新启动
cd /home/pi/dev/nora-xiaozhi-dev/3rd/liteplayer
./engine/build/music_player_server config/music_player.yaml > logs/server.log 2>&1 &
```

---

## 📞 获取帮助

遇到问题时，首先收集以下信息：

1. **完整的服务器日志**
   ```bash
   cat /home/pi/dev/nora-xiaozhi-dev/3rd/liteplayer/logs/server.log
   ```

2. **进程信息**
   ```bash
   ps aux | grep music_player
   ```

3. **系统信息**
   ```bash
   uname -a
   python3 --version
   ```

4. **测试输出**
   ```bash
   python3 scripts/test_client.py 2>&1 | tee test_output.log
   ```

---

**记住**: 🔍 **始终查看服务器日志！** 这是排查问题的关键。
