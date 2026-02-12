# MusicPlayerEngine - 快速参考卡

## 📁 文件位置

```
/home/pi/dev/nora-xiaozhi-dev/3rd/liteplayer/
├── docs/
│   ├── 需求.md                    # 原始需求
│   ├── 01_评估报告.md              # liteplayer评估
│   ├── 02_架构设计.md              # 系统架构设计
│   ├── 03_ZMQ接口规范.md           # ZMQ消息协议
│   └── 04_Phase1完成报告.md        # 完成总结
├── config/music_player.yaml       # 配置文件模板
├── engine/scripts/music_player_cli.py  # CLI测试工具
├── ENGINE_README.md               # 项目README
└── EXECUTION_SUMMARY.md           # 执行总结
```

## 🚀 快速启动

### 1. 测试liteplayer

```bash
cd /home/pi/dev/nora-xiaozhi-dev/3rd/liteplayer/example/unix/out
./playlist_demo ~/Music/test
```

### 2. 查看文档

```bash
cd /home/pi/dev/nora-xiaozhi-dev/3rd/liteplayer
cat ENGINE_README.md          # 项目概览
cat EXECUTION_SUMMARY.md      # 执行总结
cat docs/02_架构设计.md        # 架构设计
cat docs/03_ZMQ接口规范.md     # ZMQ接口
```

### 3. 使用CLI工具 (待实现完成后)

```bash
cd /home/pi/dev/nora-xiaozhi-dev/3rd/liteplayer

# 播放
./engine/scripts/music_player_cli.py play --track-id 123

# 搜索
./engine/scripts/music_player_cli.py search "周杰伦"

# 监听事件
./engine/scripts/music_player_cli.py listen
```

## 📋 ZMQ 命令速查

### 播放控制

```bash
# 播放
{"cmd": "play", "params": {"track_id": 123}}

# 暂停
{"cmd": "pause"}

# 下一首
{"cmd": "next"}

# 上一首
{"cmd": "prev"}
```

### 播放模式

```bash
# 设置模式
{"cmd": "set_play_mode", "params": {"mode": "random"}}
# 模式: sequential, loop_all, random, single_loop
```

### 搜索

```bash
# 搜索曲目
{"cmd": "search_tracks", "params": {"keyword": "周杰伦"}}

# 按标签筛选
{"cmd": "filter_by_tags", "params": {"tags": ["relax"]}}
```

## 🔧 常用命令

### 编译

```bash
cd /home/pi/dev/nora-xiaozhi-dev/3rd/liteplayer/example/unix/out
cmake ..
make -j2
```

### 查看日志

```bash
tail -f /home/pi/dev/nora-xiaozhi-dev/3rd/liteplayer/logs/music_player.log
```

### 数据库

```bash
sqlite3 /home/pi/dev/nora-xiaozhi-dev/3rd/liteplayer/data/music_library.db
```

## 📊 已完成清单

- [x] liteplayer 编译和测试
- [x] 完整的评估报告
- [x] 完整的架构设计
- [x] ZMQ接口规范
- [x] 配置文件模板
- [x] Python CLI工具
- [x] 项目文档
- [x] 目录结构搭建

## ⏳ 待完成清单

- [ ] 核心功能实现 (LitePlayerWrapper, PlaybackController, PlaylistManager)
- [ ] 音乐库实现 (SQLite, FileScanner, ID3解析)
- [ ] ZMQ接口实现
- [ ] 推荐引擎
- [ ] 服务化部署

## 📞 关键路径

```
1. 阅读 ENGINE_README.md - 了解项目概况
2. 阅读 docs/02_架构设计.md - 理解架构
3. 阅读 docs/03_ZMQ接口规范.md - 了解接口
4. 开始 Phase 2 开发 - 核心功能
```

## 🎯 下一步

**开始 Phase 2**: 核心功能开发
- LitePlayerWrapper
- PlaybackController
- PlaylistManager

**预计时间**: 4-5天

---

**最后更新**: 2026-02-11
