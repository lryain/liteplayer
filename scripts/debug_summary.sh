#!/bin/bash
# 播放器重构测试总结报告生成脚本

echo "========================================="
echo "音乐播放器并发测试总结报告"
echo "日期: $(date)"
echo "========================================="
echo ""

LOG_FILE="/home/pi/dev/nora-xiaozhi-dev/3rd/liteplayer/engine/server_debug.log"

echo "## 1. 关键问题诊断"
echo "---"

# 检查是否有死锁迹象
echo "### 1.1 锁相关日志"
grep -E "(Releasing lock|Calling safeStopInternal|isTransitioning)" "$LOG_FILE" | tail -30
echo ""

# 检查状态转换
echo "### 1.2 状态转换日志"
grep -E "State transition" "$LOG_FILE" | tail -20
echo ""

# 检查播放命令
echo "### 1.3 播放命令接收情况"
grep -E "Received command|playTrack|play\(\)" "$LOG_FILE" | tail -20
echo ""

# 检查 startCurrentTrackInternal 的调用
echo "### 1.4 startCurrentTrackInternal 执行情况"
grep -E "startCurrentTrackInternal|Releasing lock|Loading file|Track started" "$LOG_FILE" | tail -30
echo ""

# 检查自动播放
echo "### 1.5 自动播放触发"
grep -E "Auto-playing|Track completed" "$LOG_FILE" | tail -15
echo ""

echo "========================================="
echo "## 2. 第二次播放失败分析"
echo "查找第二次 'Received command: play' 后的日志..."
echo "---"

# 找到第二次播放命令的位置
LINE_NUM=$(grep -n "Received command: play" "$LOG_FILE" | tail -2 | head -1 | cut -d: -f1)
if [ -n "$LINE_NUM" ]; then
    echo "第二次播放命令在第 $LINE_NUM 行，后续30行日志:"
    tail -n +$LINE_NUM "$LOG_FILE" | head -30
else
    echo "未找到第二次播放命令"
fi

echo ""
echo "========================================="
echo "报告生成完毕"
