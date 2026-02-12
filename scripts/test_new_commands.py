#!/usr/bin/env python3
"""
测试新的音乐命令：play_by_filter, set_play_mode, set_loop_count
"""
import zmq
import json
import sys
import time

def send_command(cmd, params=None):
    """发送 ZMQ 命令到 liteplayer"""
    ctx = zmq.Context.instance()
    sock = ctx.socket(zmq.REQ)
    sock.setsockopt(zmq.RCVTIMEO, 3000)
    try:
        sock.connect('ipc:///tmp/music_player_cmd.sock')
        req = {
            'command': cmd,
            'params': params or {}
        }
        print(f"\n📤 发送命令: {cmd}")
        print(f"   参数: {json.dumps(params or {}, ensure_ascii=False)}")
        sock.send_json(req)
        rsp = sock.recv_json()
        status = rsp.get('status')
        data = rsp.get('data', {})
        if status == 'success':
            print(f"✅ 成功: {data.get('message', 'OK')}")
            for k, v in data.items():
                if k != 'message':
                    print(f"   {k}: {v}")
            return True
        else:
            print(f"❌ 失败: {rsp.get('error', 'Unknown error')}")
            return False
    except Exception as e:
        print(f"❌ ZMQ错误: {e}")
        return False
    finally:
        sock.close(0)

def main():
    print("="*60)
    print("🎵 测试新的音乐控制命令")
    print("="*60)
    
    # 1. 首先播放一首音乐
    print("\n[1] 首先播放音乐...")
    send_command('play')
    time.sleep(2)
    
    # 2. 测试 set_play_mode
    print("\n[2] 测试 set_play_mode - 随机播放...")
    send_command('set_play_mode', {'mode': 'random'})
    time.sleep(1)
    
    print("\n[3] 测试 set_play_mode - 单曲循环...")
    send_command('set_play_mode', {'mode': 'single_loop'})
    time.sleep(1)
    
    print("\n[4] 测试 set_play_mode - 列表循环...")
    send_command('set_play_mode', {'mode': 'loop_all'})
    time.sleep(1)
    
    print("\n[5] 测试 set_play_mode - 顺序播放...")
    send_command('set_play_mode', {'mode': 'sequential'})
    time.sleep(1)
    
    # 3. 测试 set_loop_count
    print("\n[6] 测试 set_loop_count - 播放3次...")
    send_command('set_loop_count', {'count': 3})
    time.sleep(1)
    
    print("\n[7] 测试 set_loop_count - 无限循环...")
    send_command('set_loop_count', {'count': 0})
    time.sleep(1)
    
    # 4. 测试 play_by_filter
    print("\n[8] 测试 play_by_filter - 按艺术家过滤...")
    send_command('play_by_filter', {'artist': 'Various'})
    time.sleep(2)
    
    print("\n[9] 测试 play_by_filter - 随机选曲...")
    send_command('play_by_filter', {})
    time.sleep(2)
    
    # 5. 基础命令验证
    print("\n[10] 基础命令 - 暂停...")
    send_command('pause')
    time.sleep(1)
    
    print("\n[11] 基础命令 - 继续...")
    send_command('play')
    time.sleep(1)
    
    print("\n[12] 基础命令 - 下一首...")
    send_command('next')
    time.sleep(2)
    
    # 6. 获取当前状态
    print("\n[13] 获取当前状态...")
    send_command('get_status')
    
    print("\n" + "="*60)
    print("✅ 测试完成！")
    print("="*60)

if __name__ == '__main__':
    main()
