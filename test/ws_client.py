import websocket
import _thread
import time
import json
import rel



CMD_PING        = "PING"
CMD_PONG        = "PONG"
CMD_LOG         = "LOG"
CMD_UPDATE      = "UPDATE"
CMD_RESTART     = "RESTART"

count = [0]

def on_message(ws: websocket.WebSocket, message):
    data = json.loads(message)
    if "cmd" in data and data["cmd"] == CMD_PING:
        d = data["dummy"]
        data["cmd"] = CMD_PONG
        data["rssi"] = -77
        data["dummy"] = d
        ws.send_text(json.dumps(data))
        print(f"sent pong {d}\n")
    else:
        print(message+"\n")

def on_error(ws, error):
    print(error)

def on_close(ws, close_status_code, close_msg):
    print("### closed ###")

def on_open(ws):
    print(f"Opened connection {count[0]}")
    count[0] = count[0] + 1

if __name__ == "__main__":
    websocket.enableTrace(True)
    ws = websocket.WebSocketApp("wss://espadmin.alagoa.top/device/358ecb06a027f02a73535818596b6f31/ws",
                              on_open=on_open,
                              on_message=on_message,
                              on_error=on_error,
                              on_close=on_close)

    ws.run_forever(dispatcher=rel, ping_interval=15, reconnect=5)  # Set dispatcher to automatic reconnection, 5 second reconnect delay if connection closed unexpectedly
    rel.signal(2, rel.abort)  # Keyboard Interrupt
    rel.dispatch()