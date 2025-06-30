

import time
import json
import logging
import base64
import os
import hashlib
import traceback
import threading
import queue
import sys


from typing import Callable

from flask_sock import Server
from logging import Logger
from repo.db_devices import DevicesRepo


# Create a logger for this module
logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)  # Set the logging level

# Create a handler (e.g., StreamHandler for console output)
handler = logging.StreamHandler()
handler.setLevel(logging.DEBUG)

# Disable propagation to the root logger
logger.propagate = False  # <-- Add this line

# Create a formatter with a custom format
formatter = logging.Formatter("%(asctime)s - %(name)s - %(levelname)s - %(device)s - %(message)s")

# Attach the formatter to the handler
handler.setFormatter(formatter)

# Attach the handler to the logger
logger.addHandler(handler)

# Log some messages
# logger.debug("This is a debug message from module1")
# logger.info("This is an info message from module1")


# Step 4: Log messages (no need to pass extra parameter every time)
# logger.debug("This is a debug message")
# logger.info("This is an info message")
# logger.warning("This is a warning message")
# logger.error("This is an error message")
# logger.critical("This is a critical message")


# def loadData(params: List[str], data, cmd=None):
#     res = []
#     for key in params:
#         type_ = None
#         if ":" in key:
#             key, type_ = key.rsplit(":")
#             type_ = type_.strip()
#         if not key in data:
#             raise Exception(f"message missing '{key}' key")
#         if type_ and type_=="str" and type(data[key]) != str:
#             raise Exception(f"invalid data type for '{type_}'")
#         if type_ and type_=="int" and type(data[key]) != int:
#             raise Exception("invalid data type for '%s'")
#         res.append(data[key])
#     if len(res)==1:
#         return res[0]
#     return res


MESSAGE_TYPE_CONNECT =          "connect"
MESSAGE_TYPE_PING =             "ping"
MESSAGE_TYPE_PING_ACK =         "ping-ack"
MESSAGE_TYPE_CLOSE =            "close"
MESSAGE_TYPE_SETTINGS =         "settings"
MESSAGE_TYPE_SETTINGS_ACK =     "settings-ack"
MESSAGE_TYPE_LIVEDATA =         "livedata"
MESSAGE_TYPE_LOG =              "log"
MESSAGE_TYPE_UPDATE =           "update"
MESSAGE_TYPE_UPDATE_ACK =       "update-ack"
MESSAGE_TYPE_RESTART =          "restart"
MESSAGE_TYPE_RESTART_ACK =      "restart-ack"


# def ws_send(ws, message_type, message_data = {}):
    
#     if ws and ws.connected:
#         ws.send(json.dumps({
#             "message_type" : message_type,
#             "message_data": message_data,
#             "timestamp" : int(time.time())
#         }))


# class DeviceHandler:

#     def __init__(self, devkey, ws: Server):
#         self._ws = ws
#         self.logger = logging.LoggerAdapter(logger, {"device": devkey[-4:]})

#     def ws_send(self, message_type, message_data = {}):
#         if self._ws and self._ws.connected:
#             self._ws.send(json.dumps({
#                 "message_type" : message_type,
#                 "message_data": message_data,
#                 "timestamp" : int(time.time())
#             }))

#     # def ws_connect(self, callback: Callable[[Server], None]):
#     #     pass




def _connected(ws: Server):
    return ws and ws.connected

def _send(ws: Server, message_type, message_data = {}):
    if _connected(ws):
        ws.send(json.dumps({
            "message_type" : message_type,
            "message_data": message_data,
            "timestamp" : int(time.time())
        }))


class DeviceInfoHandler:
    def __init__(self, devkey, name, userid):
        self.devkey = devkey
        self.name = name
        self.userid = userid
        self.last_message = 0
        self.last_connect = 0
        self.last_disconnect = 0
        self.rssi = 0
        self._connections_24h = []
        self.connections24_count = 0

    def on_connect(self):
        self.last_connect = int(time.time())
        self.last_message = int(time.time())
        self._connections_24h.append(int(time.time()))
        self._connections_24h  = [ts for ts in self._connections_24h if ts > (int(time.time()) - 3600*24)] # clear previous
        self.connections24_count = len(self._connections_24h)

    def on_disconnect(self):
        self.last_disconnect = int(time.time())

class DeviceLogHandler:

    MAX_DEVICE_LOGS = 20*1000
    LOG_CLEAN_INTERVAL = 3600

    def __init__(self, devkey, repo: DevicesRepo, logger: Logger):
        self._devkey = devkey
        self._repo  = repo
        self._logger = logger
        self._last_clean = 0

    def process_log_message(self, message_data):
        log_level = int(message_data["log_level"])
        log_msg = str(message_data["log_message"])
        log_timestamp = int(message_data["log_timestamp"])

        if not log_level or log_level < 0 or log_level > 10:
            raise Exception("invalid log message level")
        
        if not log_timestamp or log_timestamp < 1000:
            log_timestamp = int(time.time()) #now

        self._logger.debug(f"log - {log_msg}")
        self._repo.create_log(self._devkey, log_timestamp, log_level, log_msg)

        if not self._last_clean or int(time.time()) - self._last_clean > self.LOG_CLEAN_INTERVAL:
            count = self._repo.count_logs(self._devkey)
            if count and count > self.MAX_DEVICE_LOGS:
                last_week = int(time.time()) - 3600*24*7
                self._repo.remove_logs(self._devkey, last_week)
            self._last_clean = int(time.time())

class DeviceUpdateHandler:

    UPDATE_TIMEOUT = 60*2
    UPDATE_ACK_TIMEOUT = 10
    UPDATE_CHUCK_SIZE = 512 #TODO 1024 doesnt work though cf tunnel why?
    UPDATE_CHUCK_SIZE_WAIT = UPDATE_CHUCK_SIZE*50 # wait for ack every n kB
    UPDATE_FILE_MAX_SIZE = 1024*1024*10

    UPDATE_STATUS_ABORT = -1
    UPDATE_STATUS_ACK = -2
    UPDATE_STATUS_ERROR = -3


    UPDATE_STATE_RUNNING = "running"
    UPDATE_STATE_COMPLETE = "complete"
    UPDATE_STATE_PAUSED = "paused"
    UPDATE_STATE_ERROR = "error"
    UPDATE_STATE_NONE = "none"



    def __init__(self, devkey, logger: Logger):
        self.devkey = devkey
        self.state = self.UPDATE_STATE_NONE
        self.last_started = 0
        self.error = None
        self.progress = 0
        self._content = None
        self._content_md5 = None
        self._sent = 0
        self._wait_ack = 0
        self._wait_cancel = 0
        self._logger = logger

    def start_update(self, ws, path: str):

        if self.state == self.UPDATE_STATE_RUNNING:
            return self.on_error(ws, f"cant start device update, already ongoing")

        if not path or not os.path.isfile(path):
            return self.on_error(ws, f"cant start device update, file '{path}' not found")
        
        if not _connected(ws):
            return self.on_error(ws, f"cant start device update, ws not connected")
        
        if os.path.getsize(path) > self.UPDATE_FILE_MAX_SIZE:
            return self.on_error(ws, f"cant start device update, file exceeds max update size")
        
        content = None
        with open(path, "rb") as file:
            content = file.read()

        if content:
            self._content = content
            self._content_md5 = hashlib.md5(self._content).hexdigest()
            self.state = self.UPDATE_STATE_RUNNING
            self.last_started = int(time.time())
            self.progress = 0
            self.error = None
            self._sent = 0
            self._wait_cancel = 0
            self._wait_ack = 0
            self._logger.info(f"starting device update ({len(self._content)} bytes)")
        else:
            return self.on_error(ws, f"cant start device update, no file content")

    def cancel_update(self, ws):
        if self.state == self.UPDATE_STATE_RUNNING:
            self._logger.warning("user canceled the update")
            _send(ws, MESSAGE_TYPE_UPDATE, message_data = {
                "status": self.UPDATE_STATUS_ABORT,
            })
            self.state = self.UPDATE_STATE_ERROR
            self.error = "user canceled the update"
            self._content = None

    def process_update_ack(self, ws, data):
        status = int(data["status"])
        if self.state == self.UPDATE_STATE_RUNNING:

            if status > 0:
                md5 = data["md5"]
                if md5 != self._content_md5:
                    return self.on_error("update ack invalid md5")
                self.progress = int((100.0 / len(self._content) ) *status)

            if status > 0 and status < self._sent:
                self._logger.warning(f"update going back to {status}")
                self._sent = status
                self._wait_ack = 0
                self.error = None
            elif status > 0 and status > self._sent:
                self._logger.warning(f"update skipping to {status}")
                self._sent = status
                self._wait_ack = 0
                self.error = None
            elif status > 0 and status < len(self._content):
                self._logger.debug(f"device update ack, {self.progress}% complete")
                self._wait_ack = 0
                self.error = None
            elif status == len(self._content): #complete 
                self.state = self.UPDATE_STATE_COMPLETE
                self.progress = 100
                self.error = None
                self._wait_ack = 0
                self._logger.info(f"device update ack, update complete")
            elif status == 0:
                return self.on_error(ws, "invalid update state 0")
            elif status < 0 and status == self.UPDATE_STATUS_ERROR:
                if not self.error:
                    self.state = self.UPDATE_STATE_ERROR
                    self.error = "update ack error: " + data["error"] if "error" in data else "unknown error"
                    self._logger.error(f"update ack error: {self.error}")
            else:
                raise Exception(f"invalid update status '{status}'")
        else:
            if status > 0:
                self._logger.debug("device update ack but no update running")
                _send(ws, MESSAGE_TYPE_UPDATE, message_data = {
                    "status": self.UPDATE_STATUS_ABORT, # cancel
                })

    def handle_loop(self, ws: Server):

        if self.state == self.UPDATE_STATE_RUNNING:
            if _connected(ws):
                
                if self._wait_ack:           
                    if time.time()-self._wait_ack > self.UPDATE_ACK_TIMEOUT:
                        self.error = "ack timeout, waiting for device"
                        self._logger.warning("ack timeout, waiting for device")
                        _send(ws, MESSAGE_TYPE_UPDATE, {
                            "status" : self.UPDATE_STATUS_ACK,
                        })
                        self._wait_ack = time.time()
                    return
                
                data = self._content[self._sent:self._sent + self.UPDATE_CHUCK_SIZE]
                if not data: return
                if self._sent == 0:
                    _send(ws, MESSAGE_TYPE_UPDATE, {
                        "status": self._sent,
                        "size": len(self._content),
                        "md5": self._content_md5,
                        "data_size": len(data),
                        "data": base64.b64encode(data).decode()
                    })

                else:
                    start = time.time()
                    _send(ws, MESSAGE_TYPE_UPDATE, {
                        "status": self._sent,
                        "data_size": len(data),
                        "data": base64.b64encode(data).decode()
                    })
                    if time.time() -start > .1:
                        self._logger.warning(f"update data send took {time.time()-start}sec")

                self._sent += len(data)
                if self._sent == self.UPDATE_CHUCK_SIZE or self._sent % self.UPDATE_CHUCK_SIZE_WAIT == 0 or self._sent == len(self._content):
                    _send(ws, MESSAGE_TYPE_UPDATE, {
                        "status" : self.UPDATE_STATUS_ACK,
                    })
                    self._logger.debug(f"device update running {self._sent}/{len(self._content)}, waiting ack")
                    self._wait_ack = time.time()

            else:
                if not self.error:
                    self.error = "connection lost, waiting for device"
                    self._logger.warning("connection lost, waiting for device")
                    self._wait_ack = time.time()

        # elif self.state == self.UPDATE_STATE_PAUSED:
        #     if _connected(ws):
        #         self.state = self.UPDATE_STATE_RUNNING
        #         self._logger.info("resuming device update")

        # if self.running:
            
        #     if not _connected(ws):
        #         return self.on_error(ws, "update failed, ws disconnected")

        #     if self._wait_ack:           
        #         if time.time()-self._wait_ack > self.UPDATE_ACK_TIMEOUT:
        #             return self.on_error(ws, "update failed, ack timeout")
                
        #     elif self._wait_cancel:
        #         if time.time()-self._wait_cancel > self.UPDATE_ACK_TIMEOUT:
        #             return self.on_error(ws, "update abort failed, ack timeout")
        #     else:
        #         data = self._content[self._sent:self._sent + self.UPDATE_CHUCK_SIZE]
        #         if data:
        #             if self._sent == 0: # first packer
        #                 _send(ws, MESSAGE_TYPE_UPDATE, {
        #                     "current": self._sent,
        #                     "size": len(self._content),
        #                     "md5": hashlib.md5(self._content).hexdigest(),
        #                     "data_size": len(data),
        #                     "data": base64.b64encode(data).decode()
        #                 })
                        
        #             else:
        #                 t = time.time()
        #                 _send(ws, MESSAGE_TYPE_UPDATE, {
        #                     "current": self._sent,
        #                     "data_size": len(data),
        #                     "data": base64.b64encode(data).decode()
        #                 })
        #                 self._logger.debug(f"send took {time.time()-t}sec")

        #             self._sent += len(data)
        #             if self._sent == self.UPDATE_CHUCK_SIZE or self._sent % self.UPDATE_CHUCK_SIZE_WAIT == 0 or self._sent == len(self._content):
        #                 self._logger.debug(f"device update running {self._sent}/{len(self._content)}, waiting ack")
        #                 self._wait_ack = time.time()
 
    def on_error(self, ws, message):
        if self.state == self.UPDATE_STATE_RUNNING:
            self.state = self.UPDATE_STATE_ERROR
            self.error = message
            self._logger.error(f"device update error: {message}")
        

class DeviceRestartHandler:

    def __init__(self, devkey, logger: Logger):
        self.devkey  =devkey
        self._logger = logger
        self._ack_callback: Callable[[bool], None] = None
    
    def start_restart(self, ws, on_ack):
        if self._ack_callback:
            self._logger.warning("system already waiting restart ack")
        self._ack_callback = on_ack
        _send(ws, MESSAGE_TYPE_RESTART)
        self._logger.debug("sent restart message, waiting ack")

    def process_restart_ack(self, message_data):
        self._logger.info("device is restarting")
        if self._ack_callback:
            self._ack_callback(True)
            self._ack_callback = None
    
class DeviceController:

    PING_INTERVAL = 60*5
    PING_TIMEOUT = 20

    TASK_UPDATE_START = "start-update"
    TASK_UPDATE_CANCEL = "cancel-update"
    TASK_RESTART = "restart"
    TASK_DISCONNECT = "disconnect"


    def __init__(self, devkey, name, userid, repo):
        
        self.logger = logging.LoggerAdapter(logger, {"device": devkey[-4:]})
        self.info = DeviceInfoHandler(devkey, name, userid)
        self.update = DeviceUpdateHandler(devkey, self.logger)
        self.logs = DeviceLogHandler(devkey, repo, self.logger)
        self.restart = DeviceRestartHandler(devkey, self.logger)

        self._task_lock = threading.Lock()
        self._task_queue = queue.Queue()
        self.ping_started = 0

        self._ws = None

    def handle_device(self, ws: Server):

        try:
            self.logger.info("ws client connected")
            self.info.on_connect()
            self.ping_started = 0 # reset
            self._ws = ws
            while _connected(ws): 

                # handle incoming messages
                timeout_ = .1 if self.update.state != DeviceUpdateHandler.UPDATE_STATE_RUNNING else .001 #loop faster while updating
                message = ws.receive(timeout=timeout_)
                if message:
                    self._handle_message(ws, json.loads(message))
                    
                # start ping 
                if not self.ping_started  and int(time.time()) - self.info.last_message > self.PING_INTERVAL: 
                    self.ping_started = int(time.time())
                    self.logger.info("pinging device")
                    _send(ws, MESSAGE_TYPE_PING)
                    
                # handle ping timeout 
                if self.ping_started and int(time.time()) - self.ping_started > self.PING_TIMEOUT:
                    self.ping_started = 0
                    self.logger.warn("ping timeout")
                    ws.close()

                # update main loop
                self.update.handle_loop(ws)

                # handle todo tasks
                self._handle_task(ws)

        except Exception as e:

            tb = traceback.format_exc()
            self.logger.error(f"exception \n{tb}")

            #if self.update.state == DeviceUpdateHandler.UPDATE_STATE_RUNNING or self.update.state == DeviceUpdateHandler.UPDATE_STATE_PAUSED:
            #    self.update.on_error(ws, "exception occurred")

            if _connected(ws):
                ws.close()

        self.info.on_disconnect()
        self.logger.info("ws client disconnected")

    def start_update(self, path: str):
        with self._task_lock:
            self._task_queue.put((self.TASK_UPDATE_START, path))

    def cancel_update(self):
        with self._task_lock:
            self._task_queue.put((self.TASK_UPDATE_CANCEL, None))

    def restart_device(self):
        result = [None]
        def callback(success):
            result[0] = success
            
        with self._task_lock:
            self._task_queue.put((self.TASK_RESTART, callback))

        start = time.time()
        while not result[0] and time.time()-start < 5:
            time.sleep(.1)
        if not result[0]:
            self.logger.error("device failed to ack restart")
        return result[0]
        
    def connected(self):
        return _connected(self._ws)

    def disconnect(self):
        if self._ws:
            self._ws.close()

    def _handle_task(self, ws):

        task_ = None
        with self._task_lock:
            if self._task_queue.qsize() > 0:
                task_ = self._task_queue.get(block=False)
            else:
                return None

        task, params =  task_
        if task == DeviceController.TASK_RESTART:
            self.restart.start_restart(ws, params)
        elif task == DeviceController.TASK_DISCONNECT:
            _send(ws, MESSAGE_TYPE_CLOSE, {"reason": params if params else "unknown"})
            self.logger.info("closing connection: "+ params)
            if _connected(ws):
                self.logger("closing ws connection")
                ws.close()
        elif task == DeviceController.TASK_UPDATE_START:
            self.update.start_update(ws, params)
        elif task == DeviceController.TASK_UPDATE_CANCEL:
            self.update.cancel_update(ws)
        else:
            self.logger.warn(f"invalid task '{task}'")

    def _handle_message(self, ws, message):
        
        if "rssi" in message:
            self.info.rssi = int(message["rssi"])
        else:
            self.logger.warn("message is missing 'rssi' field")
        
        self.info.last_message = int(time.time()) 
        type = message["message_type"]
        message_data = message["message_data"]
        self.logger.debug(f"processing '{type}' message")

        if type == MESSAGE_TYPE_CONNECT:
            settings_version = message_data["settings_version"]
            self.logger.info(f"device reported settings version {settings_version}")
            #todo send set setting message if version < current 
            
        elif type == MESSAGE_TYPE_PING:
            self.logger.debug("sending ping ack")
            _send(ws, MESSAGE_TYPE_PING_ACK)
            
        elif type == MESSAGE_TYPE_PING_ACK:
            self.ping_started = 0
            self.logger.debug("device ping ack")
            
        elif type == MESSAGE_TYPE_CLOSE:
            reason = message_data["reason"] if "reason" in message_data else "unknown"
            self.logger.warn(f"device closing connection, reason: '{reason}'")
            if _connected(ws):
                ws.close()

        elif type == MESSAGE_TYPE_UPDATE_ACK:
            self.update.process_update_ack(ws, message["message_data"])

        elif type == MESSAGE_TYPE_LOG:
            self.logs.process_log_message(message["message_data"])

        elif type == MESSAGE_TYPE_RESTART_ACK:
            self.restart.process_restart_ack(message["message_data"])
            
        else:
            raise Exception(f"invalid message type '{type}'")
