#!/usr/bin/env bash
set -euo pipefail

SERVICE_NAME="doly-liteplayer"
SERVICE_FILE="$SERVICE_NAME.service"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
LITEPLAYER_DIR="$REPO_ROOT"
BUILD_DIR="$LITEPLAYER_DIR/engine/build"
SERVICE_BINARY="$BUILD_DIR/music_player_server"
TARGET_BIN="/usr/local/bin/music_player_server"
SYSTEMD_PATH="/etc/systemd/system/${SERVICE_FILE}"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

print_status() { echo -e "${BLUE}[INFO]${NC} $1"; }
print_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
print_warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }
print_error() { echo -e "${RED}[ERROR]${NC} $1"; }

check_build_deps() {
    if ! command -v cmake >/dev/null 2>&1; then
        print_error "cmake 未安装。请安装 cmake 或手动构建二进制。"
        return 1
    fi
    return 0
}

build_service() {
    print_status "构建 liteplayer 二进制..."
    if [ -f "$LITEPLAYER_DIR/build.sh" ]; then
        (cd "$LITEPLAYER_DIR" && ./build.sh)
        return $?
    fi
    if ! check_build_deps; then return 1; fi
    mkdir -p "$BUILD_DIR"
    cmake -S "$LITEPLAYER_DIR/engine" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
    cmake --build "$BUILD_DIR" -j$(nproc 2>/dev/null || echo 1)
    print_success "构建完成: $SERVICE_BINARY"
}

install_service() {
    print_status "安装 liteplayer systemd 服务..."
    if [ ! -x "$SERVICE_BINARY" ]; then
        print_warning "未发现二进制 $SERVICE_BINARY，尝试构建"
        build_service || { print_error "构建失败，无法安装"; return 1; }
    fi

    sudo ln -sf "$SCRIPT_DIR/$SERVICE_FILE" "$SYSTEMD_PATH"
    sudo systemctl daemon-reload
    sudo systemctl enable "$SERVICE_NAME"
    print_success "服务已安装并设置为开机自启"
}

uninstall_service() {
    print_status "卸载 liteplayer 服务..."
    if sudo systemctl is-active --quiet "$SERVICE_NAME"; then
        sudo systemctl stop "$SERVICE_NAME" || true
    fi
    sudo systemctl disable "$SERVICE_NAME" || true
    [ -f "$SYSTEMD_PATH" ] && sudo rm -f "$SYSTEMD_PATH"
    sudo systemctl daemon-reload
    print_success "服务已卸载"
}

start_service() {
    print_status "启动 liteplayer 服务..."
    sudo systemctl start "$SERVICE_NAME"
}

stop_service() {
    print_status "停止 liteplayer 服务..."
    sudo systemctl stop "$SERVICE_NAME"
}

restart_service() {
    print_status "重启 liteplayer 服务..."
    sudo systemctl restart "$SERVICE_NAME"
}

show_status() {
    echo
    echo "=== 服务状态 ==="
    sudo systemctl status "$SERVICE_NAME" --no-pager -l || true
    echo
    echo "=== 最近日志 ==="
    sudo journalctl -u "$SERVICE_NAME" -n 10 --no-pager || true
}

show_logs() {
    echo "=== 服务最近日志 (非实时) ==="
    sudo journalctl -u "$SERVICE_NAME" -n 200 --no-pager || true
}

monitor_service() {
    print_status "实时监控服务日志 (Ctrl+C 退出)..."
    sudo journalctl -u "$SERVICE_NAME" -f
}

reload_config() {
    print_status "重载服务配置..."
    sudo systemctl reload "$SERVICE_NAME" 2>/dev/null || {
        print_warning "服务不支持 reload，自动重启..."
        restart_service
    }
}

redeploy() {
    print_status "重新部署 liteplayer: 构建(可选) 并重启服务"
    if [ -f "$LITEPLAYER_DIR/build.sh" ]; then
        build_service || { print_error "构建失败"; return 1; }
    else
        print_status "没有 build.sh，尝试构建引擎二进制"
        build_service || print_warning "构建失败或已存在二进制，继续部署" || true
    fi

    if [ ! -x "$SERVICE_BINARY" ]; then
        print_error "二进制不存在: $SERVICE_BINARY"
        return 1
    fi

    # 尝试从 service 文件中解析 ExecStart 中的可执行路径作为目标
    TARGET_BIN_CANDIDATE="$TARGET_BIN"
    if [ -f "$SCRIPT_DIR/$SERVICE_FILE" ]; then
        ES_LINE=$(grep -E "^ExecStart=" "$SCRIPT_DIR/$SERVICE_FILE" || true)
        if [ -n "$ES_LINE" ]; then
            ES_VAL=${ES_LINE#ExecStart=}
            # 取第一部分作为路径
            TARGET_BIN_CANDIDATE=$(echo "$ES_VAL" | awk '{print $1}')
        fi
    fi

    if sudo systemctl is-active --quiet "$SERVICE_NAME"; then
        print_status "服务正在运行，先停止以便替换二进制"
        sudo systemctl stop "$SERVICE_NAME"
        sleep 1
    fi

    print_status "更新 systemd 单元文件和二进制"
    if [ -f "$SCRIPT_DIR/$SERVICE_FILE" ]; then
        sudo cp "$SCRIPT_DIR/$SERVICE_FILE" "$SYSTEMD_PATH"
    fi
    print_status "复制 $SERVICE_BINARY 到 $TARGET_BIN_CANDIDATE"
    # 如果源和目标指向相同文件，跳过复制以避免 cp 报错
    SRC_REAL="$(readlink -f "$SERVICE_BINARY" 2>/dev/null || true)"
    DST_REAL="$(readlink -f "$TARGET_BIN_CANDIDATE" 2>/dev/null || true)"
    if [ -n "$SRC_REAL" ] && [ "$SRC_REAL" = "$DST_REAL" ]; then
        print_warning "源文件与目标相同，跳过复制：$SRC_REAL"
        # 仍然尝试确保目标可执行
        sudo chmod +x "$TARGET_BIN_CANDIDATE" || true
    else
        sudo cp "$SERVICE_BINARY" "$TARGET_BIN_CANDIDATE"
        sudo chmod +x "$TARGET_BIN_CANDIDATE"
    fi

    print_status "重载 systemd 并重启服务"
    sudo systemctl daemon-reload
    sudo systemctl restart "$SERVICE_NAME"
    sleep 1
    if sudo systemctl is-active --quiet "$SERVICE_NAME"; then
        print_success "重新部署成功，服务已运行"
        show_status
    else
        print_error "重新部署失败: 服务未运行"
        show_logs
        return 1
    fi
}

show_help() {
    echo "Liteplayer 管理脚本"
    echo
    echo "用法: $0 [命令]"
    echo
    echo "命令:"
    echo "  build       构建服务二进制"
    echo "  install     安装 systemd 服务"
    echo "  uninstall   卸载 systemd 服务"
    echo "  start       启动服务"
    echo "  stop        停止服务"
    echo "  restart     重启服务"
    echo "  status      查看服务状态和最近日志"
    echo "  logs        查看最近日志"
    echo "  monitor     实时监控服务日志"
    echo "  reload      重载服务配置"
    echo "  redeploy    重新构建/复制并重启服务"
    echo "  help        显示帮助信息"
}

case "${1:-help}" in
    build)
        build_service
        ;;
    install)
        install_service
        ;;
    uninstall)
        uninstall_service
        ;;
    start)
        start_service
        ;;
    stop)
        stop_service
        ;;
    restart)
        restart_service
        ;;
    status)
        show_status
        ;;
    logs)
        show_logs
        ;;
    monitor)
        monitor_service
        ;;
    reload)
        reload_config
        ;;
    redeploy)
        redeploy
        ;;
    help|--help|-h)
        show_help
        ;;
    *)
        print_error "未知命令: $1"
        echo
        show_help
        exit 1
        ;;
esac
