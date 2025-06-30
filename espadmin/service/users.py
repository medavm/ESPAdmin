

import time
import logging


from repo.db_users import UsersRepo
from repo.db_devices import DevicesRepo


# Configure the root logger
logging.basicConfig(level=logging.DEBUG, format='%(asctime)s - %(name)s - %(levelname)s - %(message)s')

# Create a logger
logger = logging.getLogger(__name__)

class UsersService:

    MAX_USER_SESSIONS = 20
    
    def __init__(self, users_repo: UsersRepo, devices_repo: DevicesRepo) -> None:
        self.users_repo = users_repo
        self.devices_repo = devices_repo

    def create_user(self, email: str, passw: str):
        user = self.users_repo.create_user(email, passw)
        if user:
            return self.get_user(email=user.email)
        return None

    def remove_user(self, email: str):
        user = self.get_user(email=email)
        if not user:
            return False
        user_devices = self.devices_repo.get_user_devices(userid=user.id)
        if user_devices:
            for device in user_devices:
                self.devices_repo.remove_device(devkey=device.devkey)
        res = self.users_repo.remove_user(user.email)
        return res
            
    def get_user(self, userid: int = None, email: str = None, sesskey: str = None):
        user = self.users_repo.get_user(userid=userid, email=email, sesskey=sesskey)
        return user
    
    def create_session(self, userid: int, sesskey: str):
        now = int(time.time())
        self.users_repo.create_session(userid, sesskey, now)
        user_sessions = self.users_repo.get_user_sessions(userid)
        if user_sessions and len(user_sessions) > UsersService.MAX_USER_SESSIONS:
            logger.warning(f"max {UsersService.MAX_USER_SESSIONS} sessions per user, cleaning previous sessions for user {userid}")
            for session in user_sessions:
                if session.session != sesskey: #if not this one
                    self.users_repo.remove_session(session.session)
