



import time, datetime, threading, sqlite3


from .db_users import UsersRepo



class DevicesRepo:

    TABLE_DEVICES = "Devices"
    TABLE_DEVICE_LOGS = "DeviceLogs"

    class Device:
        def __init__(self, id, name, devkey, userid) -> None:
            self.id = id
            self.name = name
            self.devkey = devkey
            self.userid = userid

    class DeviceLog:
            def __init__(self, id, timestamp, level, message, devkey) -> None:
                self.id = id
                self.timestamp = timestamp
                self.level = level
                self.message = message
                self.devkey = devkey

    def __init__(self, sqlconn: sqlite3.Connection, lock: threading.RLock) -> None:
        self.lock = lock
        self.sqlconn = sqlconn
        self.cursor = sqlconn.cursor()
        self.__create_tables()

    def __create_tables(self):
        with self.lock:
            self.cursor.execute(f"""
                CREATE TABLE IF NOT EXISTS {DevicesRepo.TABLE_DEVICES}(
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    name VARCHAR(128) NOT NULL,
                    devkey VARCHAR(128) NOT NULL UNIQUE,
                    userid INTEGER NOT NULL,
                    FOREIGN KEY(userid) REFERENCES {UsersRepo.TABLE_USERS}(id)
                )
            """)
            self.cursor.execute(f"""
                CREATE TABLE IF NOT EXISTS DeviceLogs(
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    timestamp TIMESTAMP NOT NULL,
                    level INTEGER NOT NULL,
                    message VARCHAR(256),    
                    devkey VARCHAR(128) NOT NULL,
                    FOREIGN KEY(devkey) REFERENCES {DevicesRepo.TABLE_DEVICE_LOGS}(devkey)
                )
            """)
            self.sqlconn.commit()

    def create_device(self, userid: int, name: str, devkey: str):
        with self.lock:
            self.cursor.execute(f"INSERT INTO {DevicesRepo.TABLE_DEVICES} (name, devkey, userid) VALUES (?, ?, ?)", (name, devkey, userid))
            self.sqlconn.commit()
            return self.get_device(devkey)
            
    def remove_device(self, devkey: str):
        with self.lock:
            self.cursor.execute(f"DELETE FROM {DevicesRepo.TABLE_DEVICES} WHERE devkey=?", (devkey,))
            self.cursor.execute(f"DELETE FROM {DevicesRepo.TABLE_DEVICE_LOGS} WHERE devkey=?", (devkey,))
            self.sqlconn.commit()
            return True
    
    def get_device(self, devkey: str):
        with self.lock:
            res = self.cursor.execute(f"SELECT * FROM {DevicesRepo.TABLE_DEVICES} WHERE devkey=?", (devkey,)).fetchone()
            if res:
                return DevicesRepo.Device(id=res[0],
                                            name=res[1],
                                            devkey=res[2],
                                            userid=res[3]
                                            )
            return None

    def get_user_devices(self, userid: int):
        with self.lock:
            res = self.cursor.execute(f"SELECT * FROM {DevicesRepo.TABLE_DEVICES} WHERE userid=?", (userid,)).fetchall()
            devices: list[DevicesRepo.Device] = []
            for entry in res:
                device = DevicesRepo.Device(id=entry[0],
                                            name=entry[1],
                                            devkey=entry[2],
                                            userid=entry[3]
                                            )
                devices.append(device)
            return devices
    
    def create_log(self, devkey: str, timestamp: int, level: int, message: str):
        with self.lock:
            self.cursor.execute(f"INSERT INTO {DevicesRepo.TABLE_DEVICE_LOGS} (timestamp, level, message, devkey) VALUES (?, ?, ?, ?)", (timestamp, level, message, devkey))
            self.sqlconn.commit()
            return True
        
    def get_logs(self, devkey: str, date=None, startid=None, limit=100):
        with self.lock:
            if limit > 1000:
                limit = 1000
            
            if not date:
                t = time.localtime()
                date = "%02d/%02d/%02d" % (t.tm_mday, t.tm_mon, t.tm_year)

            if startid < 0:
                startid = 9999999999

            ts = datetime.datetime.strptime(date, "%d/%m/%Y").timestamp()
            start_ts = ts
            end_ts = ts+3600*24

            res = self.cursor.execute(f"SELECT * FROM {DevicesRepo.TABLE_DEVICE_LOGS} WHERE id <= ? AND timestamp >= ? AND timestamp < ? AND devkey = ? ORDER BY id DESC LIMIT ?", (startid, start_ts, end_ts, devkey, limit)).fetchall()
            logs: list[DevicesRepo.DeviceLog] = []
            for entry in res:
                log = DevicesRepo.DeviceLog(entry[0], entry[1], entry[2], entry[3], entry[4])
                logs.append(log)
            return logs
    
    def count_logs(self, devkey: str):
        with self.lock:
            res = self.cursor.execute(f"SELECT COUNT(*) FROM {DevicesRepo.TABLE_DEVICE_LOGS} WHERE devkey = ?", (devkey,)).fetchone()
            if res:
                return res[0]
            return None
        
    def remove_logs(self, devkey: str, timestamp: int):
        with self.lock:
            self.cursor.execute(f"DELETE FROM {DevicesRepo.TABLE_DEVICE_LOGS} WHERE devkey=? AND timestamp < ?", (devkey, timestamp))
            self.sqlconn.commit()
            return True