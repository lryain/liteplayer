#!/usr/bin/env python3
"""
调试：直接测试 play_by_filter 命令
"""
import zmq
import json
import sys

def test_play_by_filter():
    ctx = zmq.Context.instance()
    sock = ctx.socket(zmq.REQ)
    sock.setsockopt(zmq.RCVTIMEO, 3000)
    try:
        sock.connect('ipc:///tmp/music_player_cmd.sock')
        
        # 测试 1: 无参数
        print("测试1: play_by_filter (无参数)")
        req = {
            'command': 'play_by_filter',
            'params': {}
        }
        sock.send_json(req)
        rsp = sock.recv_json()
        print(f"响应: {json.dumps(rsp, indent=2, ensure_ascii=False)}\n")
        
        # 测试 2: 指定艺术家
        print("测试2: play_by_filter (artist=XXX)")
        req = {
            'command': 'play_by_filter',
            'params': {'artist': 'Various'}
        }
        sock.send_json(req)
        rsp = sock.recv_json()
        print(f"响应: {json.dumps(rsp, indent=2, ensure_ascii=False)}\n")
        
    except Exception as e:
        print(f"错误: {e}")
    finally:
        sock.close(0)

if __name__ == '__main__':
    test_play_by_filter()
