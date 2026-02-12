#!/usr/bin/env python3
"""
测试音乐播放器的高级功能：
1. play_by_filter（按艺术家/专辑/流派播放）
2. set_play_mode（循环模式设置）
3. set_loop_count（循环次数设置）
"""

import zmq
import json
import time
import sys

def send_command(endpoint, command, params=None):
    """发送 ZMQ 命令并获取响应"""
    ctx = zmq.Context.instance()
    sock = ctx.socket(zmq.REQ)
    sock.setsockopt(zmq.RCVTIMEO, 3000)
    sock.setsockopt(zmq.SNDTIMEO, 3000)
    try:
        sock.connect(endpoint)
        req = {
            'command': command,
            'params': params or {}
        }
        print(f"📤 发送: {command} {json.dumps(params or {})}")
        sock.send_json(req)
        rsp = sock.recv_json()
        status = rsp.get('status', 'unknown')
        
        if status == 'success':
            result = rsp.get('result', {})
            print(f"✅ 成功: {json.dumps(result, ensure_ascii=False)}")
            return True, result
        else:
            print(f"❌ 失败: {rsp.get('error_message', 'unknown error')}")
            return False, rsp
    except zmq.error.Again:
        print("❌ 超时：服务无响应")
        return False, None
    except Exception as e:
        print(f"❌ 错误: {e}")
        return False, None
    finally:
        sock.close(0)


def test_get_all_tracks():
    """获取所有曲目列表"""
    print("\n=== 获取所有曲目 ===")
    ok, data = send_command('ipc:///tmp/music_player_cmd.sock', 'get_all_tracks', {'limit': 50})
    if ok:
        tracks = data.get('tracks', [])
        print(f"📚 共 {len(tracks)} 首曲目")
        # 显示前5首
        for i, track in enumerate(tracks[:5]):
            print(f"  {i+1}. {track.get('title')} - {track.get('artist', 'Unknown')}")
        if len(tracks) > 5:
            print(f"  ... 及其他 {len(tracks)-5} 首")
        return tracks
    return []


def test_play_by_filter():
    """测试按条件播放"""
    print("\n=== 测试 play_by_filter（按过滤条件播放） ===")
    
    # 先获取所有曲目来确定有哪些艺术家
    tracks = test_get_all_tracks()
    if not tracks:
        print("❌ 没有曲目可播放")
        return
    
    # 1. 无条件播放（随机）
    print("\n1️⃣  无条件播放（随机选择）")
    send_command('ipc:///tmp/music_player_cmd.sock', 'play_by_filter', {})
    time.sleep(0.5)
    
    # 2. 按艺术家播放（如果有）
    if tracks:
        artist = tracks[0].get('artist', 'Unknown')
        print(f"\n2️⃣  按艺术家播放: {artist}")
        send_command('ipc:///tmp/music_player_cmd.sock', 'play_by_filter', {'artist': artist})
        time.sleep(0.5)


def test_play_mode():
    """测试循环模式"""
    print("\n=== 测试 set_play_mode（循环模式） ===")
    
    modes = ['sequential', 'loop_all', 'random', 'single_loop']
    for mode in modes:
        print(f"\n设置模式: {mode}")
        ok, _ = send_command('ipc:///tmp/music_player_cmd.sock', 'set_play_mode', {'mode': mode})
        if ok:
            # 播放一次检查是否有效
            send_command('ipc:///tmp/music_player_cmd.sock', 'play', {})
            time.sleep(1.0)
            send_command('ipc:///tmp/music_player_cmd.sock', 'pause', {})
            time.sleep(0.2)


def test_loop_count():
    """测试循环次数"""
    print("\n=== 测试 set_loop_count（循环次数） ===")
    
    # 先设置为 sequential 模式（不循环）
    send_command('ipc:///tmp/music_player_cmd.sock', 'set_play_mode', {'mode': 'sequential'})
    time.sleep(0.3)
    
    # 设置循环 3 次
    print("\n设置循环 3 次")
    ok, data = send_command('ipc:///tmp/music_player_cmd.sock', 'set_loop_count', {'count': 3})
    if ok:
        print(f"  循环状态: {data}")
    
    # 播放
    send_command('ipc:///tmp/music_player_cmd.sock', 'play', {})
    time.sleep(0.5)
    
    # 获取状态
    print("\n获取循环状态")
    send_command('ipc:///tmp/music_player_cmd.sock', 'get_status', {})
    time.sleep(0.3)


def test_voice_commands():
    """测试语音命令集成"""
    print("\n=== 测试语音命令集成 ===")
    
    # 模拟语音命令（通过 daemon 的 handle_command）
    # 这里只是测试到 music service 的直接命令
    print("\n语音命令通过 VoiceCommandManager -> _handle_music_command 路由")
    print("  - cmd_ActPlayMusic -> play")
    print("  - cmd_ActPuase -> pause")
    print("  - cmd_ActResume -> resume")
    print("  - cmd_ActNext -> next")
    print("  - cmd_ActPrevious -> previous")
    print("  - cmd_SetPlayModeRandom -> set_play_mode {mode: random}")
    print("  - cmd_SetPlayModeSingleLoop -> set_play_mode {mode: single_loop}")
    print("  - cmd_SetPlayModeLoopAll -> set_play_mode {mode: loop_all}")
    print("  - cmd_SetLoopCount -> set_loop_count {count: N}")


def main():
    print("\n" + "="*60)
    print("🎵 音乐播放器高级功能测试")
    print("="*60)
    
    # 先获取状态确认服务运行
    print("\n🔍 检查服务状态...")
    ok, status = send_command('ipc:///tmp/music_player_cmd.sock', 'get_status', {})
    if not ok:
        print("❌ 无法连接到音乐服务，请确保服务已启动")
        sys.exit(1)
    
    print(f"✅ 服务运行中，当前: {status.get('state', 'unknown')}")
    
    # 运行各项测试
    test_get_all_tracks()
    test_play_by_filter()
    test_play_mode()
    test_loop_count()
    test_voice_commands()
    
    print("\n" + "="*60)
    print("✅ 所有测试完成")
    print("="*60 + "\n")


if __name__ == '__main__':
    main()
