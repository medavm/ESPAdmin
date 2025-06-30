
import time
import threading


from repo.db_devices import DevicesRepo
from .controller import DeviceController


class DeviceService:

    class DeviceInfo:
        def __init__(self, devkey: str, name: str, userid: int, connected: int,
                      last_connect: int, last_disconnect: int, rssi: int, connections_24h_count: int) -> None:
            self.name = name
            self.devkey = devkey
            self.userid = userid
            self.connected = connected
            self.last_connect = last_connect
            self.last_disconnect = last_disconnect
            self.rssi = rssi
            self.connections_24h_count = connections_24h_count

    class DeviceLog:
        def __init__(self, id, level, message, timestamp, date):
            self.id = id
            self.level = level
            self.message = message
            self.timestamp = timestamp
            self.date = date

    class UpdateStatus:
        def __init__(self, state, last_started: int, progress: int, error: str):
            self.state = state
            self.last_started = last_started
            self.progress = progress
            self.error = error 

    def __init__(self, devices_repo: DevicesRepo):
        self._devices_repo = devices_repo
        
        self.controllers: dict[str, DeviceController] = {}
        self.controllers_lock = threading.Lock()

        self.controller_handlers: dict[str, int] = {} # [devkey, bool]
        self.controller_handlers_lock = threading.Condition()
        
    def create_device(self, user_id, device_name: str, devkey: str):
        device = self._devices_repo.create_device(user_id, device_name, devkey)
        return device
    
    def remove_device(self, devkey: str):
        controller = None
        with self.controllers_lock:
            if devkey in self.controllers:
                controller = self.controllers[devkey]
                del self.controllers[devkey]

        if controller and controller.connected():
            controller.disconnect() 
        return self._devices_repo.remove_device(devkey)

    def get_devices(self, user_id: int):
        devices = self._devices_repo.get_user_devices(user_id)
        return devices

    def get_device_info(self, devkey: str):
        controller = self._get_controller(devkey)
        if controller:
            return self.DeviceInfo(
                devkey=controller.info.devkey,
                name=controller.info.name,
                userid=controller.info.userid,
                connected=controller.connected(),
                last_connect = controller.info.last_connect,
                last_disconnect = controller.info.last_disconnect,
                rssi = controller.info.rssi,
                connections_24h_count = controller.info.connections24_count
            )

    def get_logs(self, devkey: str, date, startid):
        logs = self._devices_repo.get_logs(devkey, date, startid, 200)
        
        res: list[DeviceService.DeviceLog] = []
        for log in logs:
            t = time.localtime(log.timestamp)
            date = "%02d/%02d/%02d" % (t.tm_mday, t.tm_mon, t.tm_year)
            res.append(self.DeviceLog(log.id, log.level, log.message, log.timestamp, date))
        return res

    def device_exists(self, devkey):
        controller = self._get_controller(devkey)
        return controller != None

    def start_update(self, devkey, path):
        controller = self._get_controller(devkey)
        if controller:
            controller.start_update(path)

    def cancel_update(self, devkey):
        controller = self._get_controller(devkey)
        if controller:
            controller.cancel_update()

    def update_status(self, devkey):
        controller = self._get_controller(devkey)
        if controller:
            state = controller.update.state
            started = controller.update.last_started
            progress = controller.update.progress
            error = controller.update.error
            return self.UpdateStatus(state, started, progress, error)

    def restart_device(self, devkey):
        controller = self._get_controller(devkey)
        if controller:
            return controller.restart_device()

    def handle_device_ws(self, devkey, ws):
        try:
            controller = self._get_controller(devkey)
            if controller:
                    with self.controller_handlers_lock:
                        if not devkey in self.controller_handlers:
                            self.controller_handlers[devkey] = 0

                        while self.controller_handlers[devkey] > 0:
                            controller.disconnect()
                            self.controller_handlers_lock.wait(timeout=3)

                        self.controller_handlers[devkey] += 1
                    
                    controller.handle_device(ws) # main controller control loop

                    with self.controller_handlers_lock:
                        self.controller_handlers[devkey] -= 1
                        self.controller_handlers_lock.notify()

        except Exception as e:
            ws.close()
            raise e

    
    def _get_controller(self, devkey):
        with self.controllers_lock:
            if not devkey in self.controllers:
                device = self._devices_repo.get_device(devkey)
                if not device:
                    return None 
                self.controllers[devkey] = DeviceController(device.devkey, device.name, device.userid, self._devices_repo)
            return self.controllers[devkey]
    

