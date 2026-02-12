#!/usr/bin/env python3
"""
Ballad 播放验证脚本
用于手动验证 ballad 文件是否有音频输出
"""

import zmq
import json
import time
import sys

def send_command(sock, command, params=None):
    """发送命令到音乐播放服务"""
    msg = {"command": command}
    if params:
        msg.update(params)
    
    print(f"\n📤 命令: {command}")
    sock.send_json(msg)
    
    try:
        response = sock.recv_json(timeout=5000)
        print(f"📥 响应: {response}")
        return response
    except zmq.error.Again:
        print("❌ 超时: 没有收到响应")
        return None

def main():
    """主函数"""
    # 连接到 ZMQ
    context = zmq.Context()
    socket = context.socket(zmq.REQ)
    socket.setsockopt(zmq.RCVTIMEO, 5000)
    
    try:
        socket.connect("ipc:///tmp/music_player_cmd.sock")
        print("✅ 连接到音乐播放服务")
    except Exception as e:
        print(f"❌ 连接失败: {e}")
        return 1
    
    try:
        # 步骤 1: 检查状态
        print("\n" + "="*60)
        print("步骤 1: 获取当前状态")
        print("="*60)
        send_command(socket, "get_status")
        time.sleep(0.5)
        
        # 步骤 2: 播放当前曲目
        print("\n" + "="*60)
        print("步骤 2: 开始播放")
        print("="*60)
        send_command(socket, "play")
        time.sleep(2)
        print("⏵️  应该听到音乐声...")
        
        # 步骤 3: 跳到下一首
        print("\n" + "="*60)
        print("步骤 3: 跳到下一首")
        print("="*60)
        send_command(socket, "next")
        time.sleep(2)
        print("⏵️  应该听到不同的音乐...")
        
        # 步骤 4: 再跳一次，到达 ballad
        print("\n" + "="*60)
        print("步骤 4: 再跳一次 (应该是 ballad)")
        print("="*60)
        response = send_command(socket, "next")
        
        if response and "current_track" in response:
            current_track = response.get("current_track", "Unknown")
            print(f"\n🎵 当前播放: {current_track}")
        
        # 等待并监听
        print("\n" + "="*60)
        print("步骤 5: 监听 ballad 播放")
        print("="*60)
        print("⏵️  应该听到 'ballad' 曲目的音乐...")
        print("\n请在以下时间内仔细听音乐:")
        
        for i in range(5):
            print(f"  [{i+1}s] 继续播放中...")
            time.sleep(1)
        
        # 步骤 6: 暂停
        print("\n" + "="*60)
        print("步骤 6: 暂停播放")
        print("="*60)
        send_command(socket, "pause")
        time.sleep(0.5)
        
        # 步骤 7: 恢复
        print("\n" + "="*60)
        print("步骤 7: 恢复播放")
        print("="*60)
        send_command(socket, "resume")
        time.sleep(2)
        print("⏵️  应该继续听到音乐...")
        
        # 步骤 8: 停止
        print("\n" + "="*60)
        print("步骤 8: 停止播放")
        print("="*60)
        send_command(socket, "stop")
        
        # 总结
        print("\n" + "="*60)
        print("✅ 测试完成")
        print("="*60)
        print("\n验收清单:")
        print("  [ ] 第一首曲目有音乐声")
        print("  [ ] 第二首曲目有音乐声")
        print("  [ ] ballad 曲目有音乐声 ⭐ 重点")
        print("  [ ] 暂停/恢复工作正常")
        print("  [ ] 停止后无声音")
        print("\n如果所有项都勾选，则修复成功! ✅")
        
        return 0
        
    except KeyboardInterrupt:
        print("\n\n⚠️  用户中断")
        return 1
    except Exception as e:
        print(f"\n❌ 错误: {e}")
        return 1
    finally:
        socket.close()
        context.term()

if __name__ == "__main__":
    sys.exit(main())
