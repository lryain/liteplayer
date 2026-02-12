import zmq
import json
import time

def send_command(command, params={}):
    context = zmq.Context()
    socket = context.socket(zmq.REQ)
    socket.setsockopt(zmq.RCVTIMEO, 5000)
    socket.connect("ipc:///tmp/music_player_cmd.sock")
    
    request = {
        "command": command,
        "params": params,
        "request_id": str(int(time.time()))
    }
    
    print(f"Sending: {command}")
    socket.send_string(json.dumps(request))
    
    try:
        response = socket.recv_string()
        print(f"Received: {response}")
        return json.loads(response)
    except zmq.error.Again:
        print("Timeout")
        return None

if __name__ == "__main__":
    # Play track index 2 (bear.wav)
    send_command("play", {"index": 2})
