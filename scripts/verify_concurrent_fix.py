#!/usr/bin/env python3
"""
音乐播放器并发修复验证脚本
用于快速验证双锁机制是否正常工作
"""

import zmq
import json
import time
import sys

def test_basic_play(iterations=5):
    """基础播放测试"""
    print(f"🎵 基础播放测试：{iterations} 次连续播放")
    print("=" * 60)
    
    ctx = zmq.Context()
    req = ctx.socket(zmq.REQ)
    req.connect("ipc:///tmp/music_player_cmd.sock")
    req.setsockopt(zmq.RCVTIMEO, 5000)
    
    success_count = 0
    
    for i in range(iterations):
        print(f"\n第 {i+1}/{iterations} 次播放...", end=" ")
        req.send_json({"command": "play"})
        try:
            resp = req.recv_json()
            if resp.get("status") == "success":
                track = resp.get("result", {}).get("current_track", "Unknown")
                pos = resp.get("result", {}).get("position_ms", 0)
                print(f"✅ SUCCESS - {track} (pos={pos}ms)")
                success_count += 1
            else:
                print(f"❌ FAILED - {resp.get('message')}")
        except zmq.error.Again:
            print("❌ TIMEOUT (5s)")
        
        time.sleep(0.5)  # 500ms 间隔
    
    req.close()
    ctx.term()
    
    print(f"\n" + "=" * 60)
    print(f"📊 测试结果：{success_count}/{iterations} 成功 ({success_count*100//iterations}%)")
    
    return success_count == iterations

def test_rapid_fire(iterations=10):
    """快速连发测试（无间隔）"""
    print(f"\n🔥 快速连发测试：{iterations} 次无间隔播放")
    print("=" * 60)
    
    success_count = 0
    
    for i in range(iterations):
        ctx = zmq.Context()
        req = ctx.socket(zmq.REQ)
        req.connect("ipc:///tmp/music_player_cmd.sock")
        req.setsockopt(zmq.RCVTIMEO, 5000)
        
        req.send_json({"command": "play"})
        try:
            resp = req.recv_json()
            if resp.get("status") == "success":
                success_count += 1
                print(f"✅ {i+1}", end=" ", flush=True)
            else:
                print(f"❌ {i+1}", end=" ", flush=True)
        except zmq.error.Again:
            print(f"⏱️  {i+1}", end=" ", flush=True)
        finally:
            req.close()
            ctx.term()
    
    print(f"\n" + "=" * 60)
    print(f"📊 测试结果：{success_count}/{iterations} 成功 ({success_count*100//iterations}%)")
    
    return success_count >= iterations * 0.8  # 80% 成功率及格

if __name__ == "__main__":
    print("\n" + "=" * 60)
    print("  音乐播放器并发修复验证")
    print("  验证双锁机制 (状态锁 + 播放器锁)")
    print("=" * 60 + "\n")
    
    # 测试 1: 基础播放
    test1_pass = test_basic_play(5)
    time.sleep(1)
    
    # 测试 2: 快速连发
    test2_pass = test_rapid_fire(10)
    
    # 总结
    print("\n" + "=" * 60)
    print("🏆 最终结果")
    print("=" * 60)
    print(f"基础播放测试: {'✅ PASS' if test1_pass else '❌ FAIL'}")
    print(f"快速连发测试: {'✅ PASS' if test2_pass else '❌ FAIL'}")
    
    if test1_pass and test2_pass:
        print("\n🎉 所有测试通过！并发修复验证成功！")
        sys.exit(0)
    else:
        print("\n⚠️  部分测试失败，需要进一步检查")
        sys.exit(1)
