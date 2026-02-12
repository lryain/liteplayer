#!/bin/bash
# Phase 4 依赖安装脚本

set -e  # 遇到错误立即退出

echo "=========================================="
echo "  Phase 4 依赖安装"
echo "=========================================="
echo ""

# 检查是否为root用户
if [ "$EUID" -ne 0 ]; then 
    echo "❌ 请使用 sudo 运行此脚本"
    exit 1
fi

# 更新包列表
echo "📦 更新包列表..."
apt-get update -qq

# 安装ZMQ
echo "📦 安装 libzmq3-dev..."
apt-get install -y libzmq3-dev

# 安装cppzmq
echo "📦 安装 cppzmq-dev..."
apt-get install -y cppzmq-dev || {
    echo "⚠️  cppzmq-dev 不可用，尝试手动安装..."
    # 如果包不存在，从源码安装
    cd /tmp
    if [ ! -d "cppzmq" ]; then
        git clone https://github.com/zeromq/cppzmq.git
    fi
    cd cppzmq
    mkdir -p build && cd build
    cmake .. -DCPPZMQ_BUILD_TESTS=OFF
    make install
    echo "✅ cppzmq 手动安装完成"
}

# 安装nlohmann-json
echo "📦 安装 nlohmann-json3-dev..."
apt-get install -y nlohmann-json3-dev || {
    echo "⚠️  nlohmann-json3-dev 不可用，尝试安装 nlohmann-json-dev..."
    apt-get install -y nlohmann-json-dev || {
        echo "⚠️  包不可用，尝试手动安装..."
        # 从源码安装
        cd /tmp
        if [ ! -d "json" ]; then
            git clone https://github.com/nlohmann/json.git
        fi
        cd json
        mkdir -p build && cd build
        cmake .. -DJSON_BuildTests=OFF
        make install
        echo "✅ nlohmann-json 手动安装完成"
    }
}

# 验证安装
echo ""
echo "=========================================="
echo "  验证安装"
echo "=========================================="

# 检查ZMQ
if pkg-config --exists libzmq; then
    ZMQ_VERSION=$(pkg-config --modversion libzmq)
    echo "✅ libzmq: $ZMQ_VERSION"
else
    echo "❌ libzmq 未找到"
fi

# 检查cppzmq头文件
if [ -f "/usr/include/zmq.hpp" ] || [ -f "/usr/local/include/zmq.hpp" ]; then
    echo "✅ cppzmq: 已安装"
else
    echo "❌ cppzmq 未找到"
fi

# 检查nlohmann-json头文件
if [ -f "/usr/include/nlohmann/json.hpp" ] || [ -f "/usr/local/include/nlohmann/json.hpp" ]; then
    echo "✅ nlohmann-json: 已安装"
else
    echo "❌ nlohmann-json 未找到"
fi

echo ""
echo "=========================================="
echo "  ✅ 依赖安装完成！"
echo "=========================================="
echo ""
echo "下一步："
echo "  1. 创建数据库目录"
echo "  2. 开始实现 ConfigLoader"
echo "  3. 实现 MusicPlayerService"
echo ""
