# MusicPlayerEngine - 媒体播放引擎

基于 liteplayer 的智能音乐播放引擎,为 Doly 机器人系统提供完整的音乐播放功能。

---

## 📖 项目概述

MusicPlayerEngine 是一个功能完整的嵌入式音乐播放引擎,具有以下特点:

- 🎵 **多格式支持**: MP3, AAC, M4A, WAV
- 📁 **智能管理**: 自动扫描、元数据解析、播放列表管理
- 🏷️ **标签系统**: 支持自定义标签(流派、心情、场景)
- 💡 **智能推荐**: 基于历史和偏好的个性化推荐
- 🔌 **ZMQ集成**: 通过ZMQ与Doly系统无缝集成
- ⚡ **低开销**: 内存占用 < 100MB, CPU占用 < 5%

---

## 📚 文档导航

### 核心文档

| 文档 | 描述 | 状态 |
|-----|------|------|
| [需求文档](docs/需求.md) | 项目需求和建议步骤 | ✅ |
| [01_评估报告](docs/01_评估报告.md) | liteplayer模块评估和测试结果 | ✅ |
| [02_架构设计](docs/02_架构设计.md) | 完整的系统架构和数据库设计 | ✅ |
| [03_ZMQ接口规范](docs/03_ZMQ接口规范.md) | ZMQ消息协议和接口定义 | ✅ |
| [04_Phase1完成报告](docs/04_Phase1完成报告.md) | Phase 1 工作总结 | ✅ |
| [05_Phase2进度报告](docs/05_Phase2_进度报告.md) | Phase 2 LitePlayerWrapper完成 | ✅ |
| [06_Phase2_PlaylistManager完成](docs/06_Phase2_PlaylistManager完成.md) | PlaylistManager实现 | ✅ |
| [07_Phase2_完成报告](docs/07_Phase2_完成报告.md) | **Phase 2 核心功能完成** | ✅ |

### 快速链接

- **快速开始**: 见下方"快速开始"章节
- **配置文件**: `config/music_player.yaml`
- **测试工具**: `engine/scripts/music_player_cli.py`
- **liteplayer文档**: `example/unix/README.md`

---

## 🚀 快速开始

### 1. 编译 liteplayer

```bash
cd /home/pi/dev/nora-xiaozhi-dev/3rd/liteplayer

# 安装依赖(仅首次)
sudo apt-get install libasound2-dev

# 编译
mkdir -p example/unix/out
cd example/unix/out
cmake ..
make -j2
```

### 2. 编译 MusicPlayerEngine (Phase 2)

```bash
cd /home/pi/dev/nora-xiaozhi-dev/3rd/liteplayer/engine

# 方式1: 使用快速测试脚本
./scripts/quick_test.sh --rebuild ~/Music/test/sheep.wav

# 方式2: 手动编译
rm -rf build && mkdir build && cd build
cmake ..
make -j2

# 测试
./test_player ~/Music/test/sheep.wav
```

**编译输出**:
- `build/libmusic_player_engine.a` - 静态库 (14KB)
- `build/test_player` - 测试程序 (360KB)

### 3. 测试 liteplayer 基础功能

```bash
# 准备测试音频
mkdir -p ~/Music/test
cp ~/dev/nora-xiaozhi-dev/assets/sounds/animal/*.wav ~/Music/test/

# 运行播放列表demo
cd /home/pi/dev/nora-xiaozhi-dev/3rd/liteplayer/example/unix/out
./playlist_demo ~/Music/test
```

**交互命令**:
- `P` - 暂停
- `R` - 继续
- `N` - 下一首
- `V` - 上一首
- `O` - 启用单曲循环
- `Q` - 退出

### 4. 使用 CLI 测试工具 (当实现完成后)

```bash
cd /home/pi/dev/nora-xiaozhi-dev/3rd/liteplayer

# 监听事件
./engine/scripts/music_player_cli.py listen

# 播放音乐
./engine/scripts/music_player_cli.py play --track-id 123

# 搜索
./engine/scripts/music_player_cli.py search "周杰伦"

# 查看帮助
./engine/scripts/music_player_cli.py --help
```

---

## 🏗️ 项目结构

```
3rd/liteplayer/
├── adapter/              # liteplayer 适配器
├── config/               # 配置文件
│   └── music_player.yaml
├── data/                 # 数据目录(数据库)
├── docs/                 # 文档
│   ├── 需求.md
│   ├── 01_评估报告.md
│   ├── 02_架构设计.md
│   ├── 03_ZMQ接口规范.md
│   └── 04_Phase1完成报告.md
├── engine/               # MusicPlayerEngine (待实现)
│   ├── include/          # 头文件
│   ├── src/              # 源代码
│   │   ├── core/         # 核心模块
│   │   ├── library/      # 音乐库
│   │   ├── zmq/          # ZMQ接口
│   │   └── utils/        # 工具函数
│   ├── tests/            # 单元测试
│   └── scripts/          # 脚本工具
│       └── music_player_cli.py
├── example/              # liteplayer 示例
│   └── unix/
│       ├── basic_demo.c
│       ├── playlist_demo.c
│       └── out/          # 编译输出
├── include/              # liteplayer 头文件
├── logs/                 # 日志目录
├── src/                  # liteplayer 源码
└── thirdparty/           # 第三方库
    ├── codecs/           # 音频解码器
    ├── mbedtls/          # HTTPS支持
    └── sysutils/         # 系统工具
```

---

## 🎯 功能特性

### 已实现 (liteplayer)

- ✅ MP3/AAC/M4A/WAV 解码
- ✅ 本地文件播放
- ✅ HTTP/HTTPS 流播放
- ✅ 播放列表支持
- ✅ 基本播放控制(播放、暂停、停止、上/下一首)
- ✅ ALSA 音频输出
- ✅ 低内存占用(< 50MB)

### 设计完成 (待实现)

- ⏳ ID3 标签解析(歌名、艺术家、专辑)
- ⏳ 自定义标签系统
- ⏳ 播放模式(顺序、循环、随机、单曲循环)
- ⏳ 文件夹扫描和自动更新
- ⏳ SQLite 音乐库
- ⏳ 搜索和过滤功能
- ⏳ 播放历史记录
- ⏳ 个性化推荐
- ⏳ ZMQ 远程控制
- ⏳ 定时播放

### 未来扩展

- 🔮 音频均衡器
- 🔮 歌词显示(LRC)
- 🔮 专辑封面
- 🔮 网络电台
- 🔮 云同步

---

## 📡 ZMQ 接口

### 端点配置

- **命令订阅**: `ipc:///tmp/music_player_cmd.sock`
- **事件发布**: `ipc:///tmp/music_player_event.sock`

### 命令示例

```json
// 播放
{
    "cmd": "play",
    "params": {"track_id": 123}
}

// 搜索
{
    "cmd": "search_tracks",
    "params": {
        "keyword": "周杰伦",
        "fields": ["artist", "title"]
    }
}

// 设置播放模式
{
    "cmd": "set_play_mode",
    "params": {"mode": "random"}
}
```

### 事件示例

```json
// 状态变化
{
    "event": "state_changed",
    "timestamp": 1707680400,
    "data": {
        "state": "playing",
        "track_title": "稻香",
        "artist": "周杰伦"
    }
}

// 播放进度
{
    "event": "progress",
    "data": {
        "position_ms": 30000,
        "duration_ms": 240000,
        "percentage": 12.5
    }
}
```

详细接口文档: [03_ZMQ接口规范.md](docs/03_ZMQ接口规范.md)

---

## 🧪 测试

### liteplayer 基础测试

```bash
cd example/unix/out

# 测试单曲播放
./basic_demo ~/Music/test/song.mp3

# 测试播放列表
./playlist_demo ~/Music/test

# 测试TTS
./tts_demo
```

### MusicPlayerEngine 测试 (待实现)

```bash
# 单元测试
cd engine/build
./tests/unit_tests

# 集成测试
./tests/integration_tests

# ZMQ接口测试
cd ..
python3 scripts/test_zmq_interface.py
```

---

## 🔧 配置

配置文件: `config/music_player.yaml`

```yaml
# 扫描目录
library:
  scan_directories:
    - "/home/pi/Music"
    - "/media/usb/Music"

# 播放器
player:
  default_play_mode: "sequential"  # sequential, loop_all, random, single_loop
  audio_output:
    device: "default"

# 推荐引擎
recommendation:
  enabled: true
  history_retention_days: 90

# 日志
logging:
  level: "info"
  file: "logs/music_player.log"
```

---

## 🚢 部署

### 开发模式

```bash
# 启动服务
cd engine/build
./music_player_engine --config ../config/music_player.yaml
```

### 生产模式 (systemd)

```bash
# 安装服务
sudo ./scripts/manage_service.sh install

# 启动
sudo systemctl start music-player

# 查看状态
sudo systemctl status music-player

# 查看日志
sudo journalctl -u music-player -f
```

---

## 🛠️ 开发指南

### 编译环境

```bash
# 安装依赖
sudo apt-get install -y \
    build-essential \
    cmake \
    libasound2-dev \
    libzmq3-dev \
    libsqlite3-dev \
    libtag1-dev \
    libyaml-cpp-dev

# 克隆 spdlog
cd thirdparty
git clone https://github.com/gabime/spdlog.git
```

### 编译 MusicPlayerEngine (待实现)

```bash
mkdir -p engine/build
cd engine/build

cmake .. -DCMAKE_BUILD_TYPE=Release
make -j2

# 运行
./music_player_engine
```

### 代码规范

- **C++ 标准**: C++17
- **命名规范**: Google C++ Style Guide
- **注释**: Doxygen 格式
- **测试**: Google Test

---

## 📊 性能指标

### 目标指标

- **内存占用**: < 100MB (运行时)
- **CPU占用**: < 5% (播放时)
- **启动时间**: < 2秒
- **扫描性能**: > 1000首/秒

### 实测结果 (liteplayer)

- **内存占用**: ~50MB ✅
- **CPU占用**: ~3% ✅
- **启动时间**: < 1秒 ✅

---

## 🗺️ 开发路线图

### Phase 1: 评估和设计 ✅ (已完成)

- [x] liteplayer 评估
- [x] 架构设计
- [x] ZMQ 接口设计
- [x] 文档编写

### Phase 2: 核心功能 (4-5天)

- [ ] LitePlayerWrapper
- [ ] PlaybackController
- [ ] PlaylistManager
- [ ] 基础单元测试

### Phase 3: 音乐库 (3-4天)

- [ ] SQLite 数据库
- [ ] MusicLibrary
- [ ] FileScanner
- [ ] ID3 标签解析

### Phase 4: ZMQ 集成 (2-3天)

- [ ] ZMQControlInterface
- [ ] 命令处理
- [ ] 事件发布
- [ ] 集成测试

### Phase 5: 高级功能 (3-4天)

- [ ] 推荐引擎
- [ ] 定时播放
- [ ] 文件监控
- [ ] 性能优化

### Phase 6: 部署 (2-3天)

- [ ] systemd 服务
- [ ] 部署脚本
- [ ] 用户手册
- [ ] 集成到 Doly

**总计**: 约 14-19天

---

## 🤝 贡献

本项目为 Doly 机器人系统的一部分。

---

## 📄 许可证

- **liteplayer**: Apache License 2.0
- **MusicPlayerEngine**: 与 Doly 项目保持一致

---

## 📞 联系方式

- **项目主页**: `/home/pi/dev/nora-xiaozhi-dev`
- **文档目录**: `/home/pi/dev/nora-xiaozhi-dev/3rd/liteplayer/docs`

---

## 🙏 致谢

- [liteplayer](https://github.com/sepnic/liteplayer) - 优秀的轻量级音频播放库
- [ZMQ](https://zeromq.org/) - 高性能异步消息库
- [TagLib](https://taglib.org/) - 音频元数据库

---

**最后更新**: 2026-02-11  
**项目状态**: Phase 1 完成 ✅
