#!/usr/bin/env python3
"""
Music Player Service 测试客户端
测试ZMQ命令和事件通信
"""

import zmq
import json
import time
import uuid
import sys
from datetime import datetime

class MusicPlayerClient:
    """音乐播放器客户端"""
    
    def __init__(self, cmd_endpoint="ipc:///tmp/music_player_cmd.sock",
                 event_endpoint="ipc:///tmp/music_player_event.sock"):
        self.context = zmq.Context()
        
        # 命令socket (REQ)
        self.cmd_socket = self.context.socket(zmq.REQ)
        self.cmd_socket.connect(cmd_endpoint)
        self.cmd_socket.setsockopt(zmq.RCVTIMEO, 5000)  # 5秒超时
        
        # 事件socket (SUB)
        self.event_socket = self.context.socket(zmq.SUB)
        self.event_socket.connect(event_endpoint)
        self.event_socket.subscribe(b"")  # 订阅所有事件
        self.event_socket.setsockopt(zmq.RCVTIMEO, 1000)  # 1秒超时
        
        print(f"✅ Connected to music player service")
        print(f"   Command: {cmd_endpoint}")
        print(f"   Events:  {event_endpoint}")
    
    def send_command(self, command, params=None):
        """发送命令"""
        if params is None:
            params = {}
        
        request = {
            "command": command,
            "params": params,
            "request_id": str(uuid.uuid4())
        }
        
        print(f"\n📤 Sending: {command}")
        print(f"   Params: {params}")
        
        try:
            # 发送请求
            self.cmd_socket.send_json(request)
            
            # 接收响应
            try:
                response = self.cmd_socket.recv_json()
                print(f"📥 Response: {response['status']}")
                
                if response['status'] == 'success':
                    print(f"   Result: {response.get('result', {})}")
                    return response['result']
                else:
                    print(f"   ❌ Error: {response.get('error_message', 'Unknown error')}")
                    return None
                    
            except zmq.error.Again:
                print(f"   ❌ Timeout: No response received")
                # 恢复socket状态：需要重建socket以清除EFSM状态
                self._recover_socket()
                return None
                
        except zmq.error.ZMQError as e:
            if "EFSM" in str(e) or "state" in str(e).lower():
                print(f"   ❌ Socket state error: {e}")
                # 恢复socket
                self._recover_socket()
                return None
            else:
                raise
    
    def _recover_socket(self):
        """恢复socket状态 - REQ socket 在超时后需要重建"""
        print(f"   🔄 Recovering socket state...")
        try:
            self.cmd_socket.close(linger=0)
        except:
            pass
        
        # 重建socket
        self.cmd_socket = self.context.socket(zmq.REQ)
        self.cmd_socket.connect("ipc:///tmp/music_player_cmd.sock")
        self.cmd_socket.setsockopt(zmq.RCVTIMEO, 5000)
        print(f"   ✅ Socket recovered")
    
    def receive_events(self, duration=2):
        """接收事件（指定时长）"""
        print(f"\n📡 Listening for events ({duration}s)...")
        start_time = time.time()
        event_count = 0
        
        while time.time() - start_time < duration:
            try:
                event_str = self.event_socket.recv_string()
                event = json.loads(event_str)
                event_count += 1
                
                timestamp = datetime.fromtimestamp(event['timestamp']).strftime('%H:%M:%S')
                print(f"   [{timestamp}] {event['event']}: {event.get('data', {})}")
                
            except zmq.error.Again:
                continue
            except Exception as e:
                print(f"   ⚠️  Event parse error: {e}")
        
        print(f"   Total events received: {event_count}")
    
    def close(self):
        """关闭连接"""
        self.cmd_socket.close()
        self.event_socket.close()
        self.context.term()
        print("\n✅ Connection closed")


def run_tests():
    """运行测试套件"""
    print("="*60)
    print("  Music Player Service - Test Suite")
    print("="*60)
    
    client = MusicPlayerClient()
    
    try:
        # Test 1: 获取状态
        print("\n" + "="*60)
        print("Test 1: Get Status")
        print("="*60)
        client.send_command("get_status")
        
        # Test 2: 添加曲目
        print("\n" + "="*60)
        print("Test 2: Add Track")
        print("="*60)
        import time
        # 使用真实存在的音乐文件（相对路径）
        result = client.send_command("add_track", {
            "file_path": "../../assets/sounds/animal/dog.wav",
            "title": "Dog Sound",
            "artist": "Nature",
            "album": "Animal Sounds",
            "year": 2024,
            "duration_ms": 0
        })
        
        track_id = result.get('track_id') if result else None
        
        # Test 3: 获取曲目
        if track_id:
            print("\n" + "="*60)
            print("Test 3: Get Track")
            print("="*60)
            client.send_command("get_track", {"track_id": track_id})
        
        # Test 4: 获取所有曲目
        print("\n" + "="*60)
        print("Test 4: Get All Tracks")
        print("="*60)
        result = client.send_command("get_all_tracks", {"limit": 10})
        if result:
            print(f"   📚 Total tracks: {result.get('count', 0)}")
        
        # Test 5: 搜索曲目
        print("\n" + "="*60)
        print("Test 5: Search Tracks")
        print("="*60)
        result = client.send_command("search_tracks", {
            "query": "Test",
            "limit": 5
        })
        if result:
            print(f"   🔍 Found: {result.get('count', 0)} tracks")
        
        # Test 6: 播放控制
        print("\n" + "="*60)
        print("Test 6: Playback Control")
        print("="*60)
        client.send_command("play")
        time.sleep(5)
        client.send_command("pause")
        time.sleep(2)
        client.send_command("stop")
        
        # Test 7: 导航控制
        print("\n" + "="*60)
        print("Test 7: Navigation Control")
        print("="*60)
        client.send_command("next")
        time.sleep(5)
        client.send_command("previous")
        time.sleep(5)
        client.send_command("next")
        time.sleep(5)
        # Test 8: 接收事件
        print("\n" + "="*60)
        print("Test 8: Event Subscription")
        print("="*60)
        client.receive_events(duration=3)
        
        # 测试完成
        print("\n" + "="*60)
        print("  ✅ All tests completed!")
        print("="*60)
        
    except KeyboardInterrupt:
        print("\n\n⚠️  Tests interrupted by user")
    except Exception as e:
        print(f"\n\n❌ Test error: {e}")
        import traceback
        traceback.print_exc()
    finally:
        client.close()


def interactive_mode():
    """交互模式"""
    print("="*60)
    print("  Music Player Client - Interactive Mode")
    print("="*60)
    print("\nAvailable commands:")
    print("  play, pause, stop, next, previous")
    print("  get_status, get_all_tracks")
    print("  add_track, search_tracks")
    print("  events (listen for 5 seconds)")
    print("  quit")
    print()
    
    client = MusicPlayerClient()
    
    try:
        while True:
            cmd = input("\n> ").strip()
            
            if not cmd:
                continue
            
            if cmd == "quit":
                break
            
            if cmd == "events":
                client.receive_events(duration=5)
                continue
            
            # 简单命令（无参数）
            if cmd in ["play", "pause", "stop", "next", "previous", "get_status", "get_all_tracks"]:
                client.send_command(cmd)
            else:
                print(f"Unknown command: {cmd}")
    
    except KeyboardInterrupt:
        print("\n")
    finally:
        client.close()


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "interactive":
        interactive_mode()
    else:
        run_tests()
