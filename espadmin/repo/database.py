

import sqlite3
import threading
import os

from .db_users import UsersRepo
from .db_devices import DevicesRepo



class Repos:
    def __init__(self, sql_path):
        
        os.makedirs(os.path.dirname(sql_path), exist_ok=True)
        if not os.path.isfile(sql_path):
            with open(sql_path, "wb") as file:
                pass  

        self.lock = threading.RLock()
        self.sqlconn = sqlite3.connect(sql_path, check_same_thread=False) #TODO check_same_thread ?
        self.users: UsersRepo = UsersRepo(self.sqlconn, self.lock)
        self.devices: DevicesRepo = DevicesRepo(self.sqlconn, self.lock)