

import sqlite3
import threading

class UsersRepo:

    TABLE_USERS = "Users"
    TABLE_SESSIONS = "UserSessions"

    class User:
        def __init__(self, id, email, passw) -> None:
            self.id = id
            self.email = email
            self.passw = passw
    
    class UserSession:
        def __init__(self, userid, session, timestamp) -> None:
            self.userid = userid
            self.session = session
            self.timestamp = timestamp

    def __init__(self, sqlconn: sqlite3.Connection, lock: threading.RLock) -> None:
        #self.sqlconn = sqlite3.connect(path, check_same_thread=True) #TODO check_same_thread ?
        self.lock = lock
        self.sqlconn = sqlconn
        self.cursor = sqlconn.cursor()
        self.__create_tables()

    def __create_tables(self):
            with self.lock:
                self.cursor.execute(f"""
                    CREATE TABLE IF NOT EXISTS {UsersRepo.TABLE_USERS}(
                        id INTEGER PRIMARY KEY AUTOINCREMENT,
                        email VARCHAR(128) UNIQUE NOT NULL,
                        passw VARCHAR(128) NOT NULL
                    )
                """)
                self.cursor.execute(f"""
                    CREATE TABLE IF NOT EXISTS {UsersRepo.TABLE_SESSIONS}(
                        session VARCHAR(128) UNIQUE NOT NULL,
                        timestamp TIMESTAMP NOT NULL,
                        userid INTEGER NOT NULL,
                        FOREIGN KEY(userid) REFERENCES {UsersRepo.TABLE_USERS}(id)
                    )
                """)
                self.sqlconn.commit()


    def create_user(self, email: str, passw: str):
        with self.lock:
            self.cursor.execute(f"INSERT INTO {UsersRepo.TABLE_USERS} (email, passw) VALUES (?, ?)", (email, passw,))
            self.sqlconn.commit()
            return self.get_user(email=email)
    
    def remove_user(self, email: str):
        with self.lock:
            user = self.get_user(email=email)
            if not user:
                return False
            #user_devices = self.getUserDevices(db_user.id) #TODO do in service
            #for device in user_devices:
                #cursor.execute(f"DELETE FROM {UsersRepo.TABLE_USERS} WHERE devkey=?", (device.devkey,))
            self.cursor.execute(f"DELETE FROM {UsersRepo.TABLE_SESSIONS} WHERE userid=?", (user.id,))
            self.cursor.execute(f"DELETE FROM {UsersRepo.TABLE_USERS} WHERE id=?", (user.id,))
            self.sqlconn.commit()
            return True

    def get_user(self, userid: int = None, email: str = None, sesskey: str = None):
        with self.lock:
            res = None

            if sesskey:
                user_session = self.get_session(sesskey)
                if user_session:
                    userid = user_session.userid
                    res = None

            if userid:
                res = self.cursor.execute(f"SELECT * FROM {UsersRepo.TABLE_USERS} WHERE id=?", (userid,)).fetchone()
            elif email:
                res = self.cursor.execute(f"SELECT * FROM {UsersRepo.TABLE_USERS} WHERE email=?", (email,)).fetchone()
            
            if res:
                return UsersRepo.User(
                    id=res[0],
                    email=res[1],
                    passw=res[2],
                )
            return None
    
    # def get_users(self):
    #     cursor = self.sqlconn.cursor()
    #     res = cursor.execute("SELECT * FROM Users").fetchall()
    #     users: List[DBController.UserEntry] = []
    #     for entry in res:
    #         users.append(self.getUser(email=entry[1]))
    #     return users
    
    def create_session(self, userid: int, sesskey: str, ts: int):
        with self.lock:
            self.cursor.execute(f"INSERT INTO {UsersRepo.TABLE_SESSIONS} (userid, session, timestamp) VALUES (?, ?, ?)", (userid, sesskey, ts))
            self.sqlconn.commit()
            return True
        
    def get_session(self, sesskey: str):
        with self.lock:
            res = self.cursor.execute(f"SELECT * FROM {UsersRepo.TABLE_SESSIONS} WHERE session=?", (sesskey,)).fetchone()
            if res:
                return UsersRepo.UserSession(res[2], res[0], res[1])
            return None
        
    def get_user_sessions(self, userid: int):
        res = self.cursor.execute(f"SELECT * FROM {UsersRepo.TABLE_SESSIONS} WHERE userid=?", (userid,)).fetchall()
        user_sessions = [UsersRepo.UserSession(col[2], col[0], col[1]) for col in res]
        return user_sessions
    
    def remove_session(self, sesskey: str):
        with self.lock:
            self.cursor.execute(f"DELETE FROM {UsersRepo.TABLE_SESSIONS} WHERE session=?", (sesskey,))
            self.sqlconn.commit()
            return True
