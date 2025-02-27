


import websockets
import asyncio
import  sys
import json

# Server data
PORT = 9000
print("ws server listening port " + str(PORT))

# A set of connected ws clients
connected = set()

# The main behavior function for this server
async def echo(websocket, path):
    print("client connected", flush=True)
    # Store a copy of the connected client
    
    connected.add(websocket)
    # Handle incoming messages
    try:
        async for message in websocket:
            print("client: " + message, flush=True)
            d = {"cmd": 1, "error":""}
            s = json.dumps(d)
            await websocket.send(s)
            # Send a response to all connected clients except sender
            #for conn in connected:
            #    if conn != websocket:
            #        await conn.send("Someone said: " + message)
    # Handle disconnecting clients 
    except websockets.exceptions.ConnectionClosed as e:
        connected.remove(websocket)
        print("client disconnected", flush=True)
        sys.exit(0)


# Start the server
start_server = websockets.serve(echo, "0.0.0.0", PORT, ping_interval=15)
asyncio.get_event_loop().run_until_complete(start_server)
asyncio.get_event_loop().run_forever()