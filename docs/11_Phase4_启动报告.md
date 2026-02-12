# Phase 4 启动报告 - ZMQ远程控制集成

> 开始日期：2026-02-12  
> 状态：🟡 **准备就绪**  
> 前置条件：✅ Phase 1-3 已完成

---

## 📋 Phase 4 目标

将MusicPlayerEngine集成ZMQ通信能力，实现远程控制和事件发布。

### 核心功能
1. **ZMQ服务器** - 接收命令、发布事件
2. **命令处理** - 26+种控制命令
3. **事件发布** - 6种事件类型
4. **JSON协议** - 消息序列化/反序列化

---

## 🏗️ 技术架构

### 通信模式
```
┌─────────────────┐         IPC/ZMQ          ┌──────────────────┐
│  Doly Daemon    │ ────────────────────────> │ MusicPlayerEngine│
│  (Publisher)    │  music.cmd.*             │  (Subscriber)    │
└─────────────────┘                          └──────────────────┘
                                                       │
                                                       │
                                                       v
┌─────────────────┐         IPC/ZMQ          ┌──────────────────┐
│  Doly Daemon    │ <──────────────────────── │ MusicPlayerEngine│
│  (Subscriber)   │  music.event.*           │  (Publisher)     │
└─────────────────┘                          └──────────────────┘
```

### 端点配置
- **命令订阅**: `ipc:///tmp/music_player_cmd.sock`
- **事件发布**: `ipc:///tmp/music_player_event.sock`

---

## 📦 Phase 4 任务清单

### 1. 环境准备 ⏳
- [ ] 安装ZMQ库（libzmq3-dev, cppzmq-dev）
- [ ] 安装JSON库（nlohmann-json3-dev）
- [ ] 更新CMakeLists.txt添加依赖

### 2. 核心实现 ⏳

#### 2.1 MusicPlayerService类
- [ ] `MusicPlayerService.h` - 服务器接口定义
- [ ] `MusicPlayerService.cpp` - 服务器实现
  - [ ] ZMQ上下文初始化
  - [ ] SUB socket创建（接收命令）
  - [ ] PUB socket创建（发布事件）
  - [ ] 消息循环

#### 2.2 命令处理器
- [ ] `handleControlCommand()` - play, pause, stop, next, prev等
- [ ] `handlePlaylistCommand()` - set_playlist, add_track等
- [ ] `handleModeCommand()` - set_play_mode
- [ ] `handleQueryCommand()` - search_tracks, get_current_track等
- [ ] `handleLibraryCommand()` - scan_directory, add_tracks等

#### 2.3 事件发布
- [ ] `publishStateEvent()` - 播放状态变化
- [ ] `publishProgressEvent()` - 播放进度（每秒）
- [ ] `publishTrackEvent()` - 曲目变化
- [ ] `publishErrorEvent()` - 错误事件
- [ ] `publishLibraryEvent()` - 库更新事件

#### 2.4 JSON消息处理
- [ ] `parseCommand()` - JSON反序列化
- [ ] `buildEvent()` - JSON序列化
- [ ] 命令参数验证
- [ ] 错误处理

### 3. 集成现有模块 ⏳
- [ ] 集成PlaybackController
- [ ] 集成MusicLibrary
- [ ] 集成PlaylistManager
- [ ] 回调函数注册（状态/进度）

### 4. 主程序入口 ⏳
- [ ] `music_player_server.cpp` - 主程序
- [ ] 信号处理（SIGINT, SIGTERM）
- [ ] 守护进程模式
- [ ] 日志系统

### 5. 测试验证 ⏳
- [ ] 单元测试（命令解析）
- [ ] 集成测试（ZMQ通信）
- [ ] 压力测试
- [ ] 文档完善

---

## 📝 实现计划

### Day 1: 基础框架（今天）
1. ✅ 安装依赖（ZMQ, JSON）
2. ✅ 创建MusicPlayerService类框架
3. ✅ 实现基本的ZMQ通信
4. ✅ 实现JSON消息解析

### Day 2: 命令处理
1. ⏳ 实现所有控制命令
2. ⏳ 实现播放列表命令
3. ⏳ 实现查询命令
4. ⏳ 测试验证

### Day 3: 事件发布和集成
1. ⏳ 实现所有事件发布
2. ⏳ 集成现有模块
3. ⏳ 完整测试
4. ⏳ 文档完善

---

## 🔧 依赖库

### ZMQ (ZeroMQ)
```bash
sudo apt-get install -y libzmq3-dev
```
- 版本: 4.3.x
- 用途: 进程间通信

### CPPZMQ
```bash
sudo apt-get install -y cppzmq-dev
```
- 版本: 4.x
- 用途: ZMQ的C++绑定

### nlohmann/json
```bash
sudo apt-get install -y nlohmann-json3-dev
```
- 版本: 3.x
- 用途: JSON序列化/反序列化

---

## 📊 预期代码量

```
文件                               预计行数    说明
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
include/MusicPlayerService.h        ~200      服务器接口
src/service/MusicPlayerService.cpp  ~800      服务器实现
src/music_player_server.cpp         ~150      主程序
tests/test_zmq_service.cpp           ~300      测试用例
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
总计                                ~1450      C++代码
```

---

## 🎯 命令列表（26+种）

### 播放控制（8个）
1. `play` - 开始播放
2. `pause` - 暂停播放
3. `resume` - 恢复播放
4. `stop` - 停止播放
5. `next` - 下一首
6. `prev` - 上一首
7. `seek` - 跳转位置
8. `set_volume` - 设置音量

### 播放列表（6个）
9. `set_playlist` - 设置播放列表
10. `add_track` - 添加曲目
11. `remove_track` - 移除曲目
12. `clear_playlist` - 清空列表
13. `shuffle_playlist` - 随机播放列表
14. `get_playlist` - 获取列表

### 播放模式（3个）
15. `set_play_mode` - 设置播放模式（Sequential/LoopAll/SingleLoop/Random）
16. `get_play_mode` - 获取播放模式
17. `toggle_repeat` - 切换重复模式

### 查询（5个）
18. `get_current_track` - 获取当前曲目
19. `get_status` - 获取播放状态
20. `search_tracks` - 搜索曲目
21. `get_favorites` - 获取收藏
22. `get_playlists` - 获取所有播放列表

### 库管理（4个）
23. `scan_directory` - 扫描目录
24. `add_tracks` - 批量添加
25. `set_favorite` - 设置收藏
26. `create_playlist` - 创建播放列表

---

## 🔔 事件类型（6种）

### 1. 状态事件 (music.event.state)
```json
{
    "event": "state_changed",
    "state": "playing",
    "timestamp": 1644739200
}
```

### 2. 进度事件 (music.event.progress)
```json
{
    "event": "progress",
    "position_ms": 45000,
    "duration_ms": 180000,
    "percentage": 25.0
}
```

### 3. 曲目事件 (music.event.track)
```json
{
    "event": "track_changed",
    "track": {
        "id": 123,
        "title": "Song Title",
        "artist": "Artist Name",
        "duration_ms": 180000
    }
}
```

### 4. 错误事件 (music.event.error)
```json
{
    "event": "error",
    "error_code": "FILE_NOT_FOUND",
    "message": "Track file not found"
}
```

### 5. 库更新事件 (music.event.library)
```json
{
    "event": "scan_complete",
    "tracks_added": 50,
    "tracks_total": 500
}
```

### 6. 播放列表事件 (music.event.playlist)
```json
{
    "event": "playlist_changed",
    "track_count": 20,
    "current_index": 5
}
```

---

## ✅ 成功标准

Phase 4完成标志：

1. ✅ ZMQ服务器正常运行
2. ✅ 所有26+命令正确处理
3. ✅ 所有6种事件正确发布
4. ✅ 与Doly daemon通信正常
5. ✅ 测试覆盖率 > 80%
6. ✅ 文档完整

---

## 📅 时间表

- **Day 1**: 2026-02-12 下午（今天）- 基础框架
- **Day 2**: 2026-02-13 - 命令处理完成
- **Day 3**: 2026-02-14 - 事件发布和集成测试
- **完成日期**: 2026-02-14

---

**当前状态：** 🟡 准备就绪  
**下一步：** 安装依赖并开始实现
