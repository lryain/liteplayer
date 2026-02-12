#!/bin/bash
# MusicPlayerEngine 快速测试脚本

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ENGINE_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$ENGINE_DIR/build"

echo "=== MusicPlayerEngine 快速测试 ==="
echo

# 检查是否需要重新编译
if [ ! -f "$BUILD_DIR/test_player" ] || [ "$1" == "--rebuild" ]; then
    echo "🔨 编译 MusicPlayerEngine..."
    cd "$ENGINE_DIR"
    rm -rf build
    mkdir build
    cd build
    cmake .. > /dev/null
    make -j2
    echo "✅ 编译完成"
    echo
fi

# 测试选项
TEST_FILE="${2:-$HOME/Music/test/sheep.wav}"
TEST_DIR="$HOME/Music/test"

echo "📁 测试文件: $TEST_FILE"
echo

# 检查文件是否存在
if [ ! -e "$TEST_FILE" ]; then
    echo "❌ 测试文件不存在: $TEST_FILE"
    echo
    echo "使用方法："
    echo "  $0 [--rebuild] [测试文件或目录]"
    echo
    echo "示例："
    echo "  $0                                    # 测试默认文件"
    echo "  $0 --rebuild                           # 重新编译后测试"
    echo "  $0 ~/Music/test/sheep.wav              # 测试指定文件"
    echo "  $0 ~/Music/test                        # 测试目录（播放列表）"
    exit 1
fi

# 运行测试
echo "🎵 开始测试播放..."
echo "   按 Ctrl+C 停止"
echo
"$BUILD_DIR/test_player" "$TEST_FILE"

echo
echo "✅ 测试完成"
