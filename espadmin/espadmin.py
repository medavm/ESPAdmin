

import os
import json
import random
import time

from flask import Flask, redirect, request, send_file, send_from_directory
from flask_cors import CORS
from functools import wraps
from werkzeug.exceptions import HTTPException, NotAcceptable, BadRequest, NotFound
from flask_sock import Sock, Server


from repo.database import Repos
from service.users import UsersService
from service.devices import DeviceService



DB_FILE = "./data/db.sqlite"


app = Flask(__name__)
CORS(app, origins=["*"])

app.config['SOCK_SERVER_OPTIONS'] = {'ping_interval': 60*5}
sock = Sock(app)


repos = Repos(DB_FILE) # Only create this in the actual running process
users = UsersService(repos.users, repos.devices)
devices =  DeviceService(repos.devices)



# TODO until registration page, create default user
user = users.get_user(email="admin")
if not user:
    if not users.create_user("admin", "admin"):
        raise Exception("failed to create default user")
    

class UserNotFound(HTTPException):
    def __init__(self, email: str) -> None:
        self.code = 404
        self.description = "email '%s' not found" % email
        super().__init__()

class DeviceNotFound(HTTPException):
    def __init__(self, devkey: str) -> None:
        self.code = 404
        self.description = "invalid device key '%s'" % devkey
        super().__init__()

class DeviceNotConnected(HTTPException):
    def __init__(self) -> None:
        self.code = 406
        self.description = "device not connected"
        super().__init__()

class InvalidSession(HTTPException):
    def __init__(self) -> None: 
        self.code = 401
        self.description = "invalid session"
        super().__init__()

class MissingKey(HTTPException):
    def __init__(self, key: str) -> None:
        self.code = 400
        self.description = "invalid request '%s' missing" % key
        super().__init__()

class InvalidType(HTTPException):
    def __init__(self, key: str) -> None:
        self.code = 400
        self.description = "invalid type for '%s'" % key
        super().__init__()

def json_params(params: list[str]):
    def inner1(f):
        @wraps(f)
        def inner2(*args, **kwargs):
            kwargs_ = {}
            data = request.get_json()
            for key in params:
                type_ = None
                if ":" in key:
                    key, type_ = key.rsplit(":")
                    type_ = type_.strip()
                if not key in data:
                    raise MissingKey(key)
                if type_ and type_=="str" and type(data[key]) != str:
                    raise InvalidType(key)
                if type_ and type_=="int" and type(data[key]) != int:
                    raise InvalidType(key)
                kwargs_[key] = data[key]

            return f(**kwargs_, **kwargs)
        return inner2
    return inner1


@app.errorhandler(HTTPException)
def handle_exception(e):
    app.log_exception(e)
    resp = e.get_response()
    resp.content_type = "application/json"
    # resp.data = json.dumps({
    #     "code": e.code,
    #     "name": e.name,
    #     "description": e.description,
    # })
    desc = e.description
    if len(desc) > 128:
        desc = desc[:128]
    resp.data = json.dumps({"error": desc})
    return resp

@app.errorhandler(Exception)
def handle_exception(e):
    app.log_exception(e)
    return {"error": str(e)}, 500
 
# @app.route('/')
# def index():
#     return redirect("/home", code=301)

# @app.route('/icon.png')
# def icon():
#     return send_from_directory("web_app/build", "icon.png")

# @app.route('/favicon.png')
# def icon2():
#     return send_from_directory("web_app/build", "favicon.png")
   
# @app.route("/login")
# def sendWebApp2():
#     return send_from_directory("web_app/build", "login.html")

# @app.route("/home")
# def sendWebApp3():
#     return send_from_directory("web_app/build", "home.html")

# @app.route("/_app/<path:path>")
# def sendWebApp4(path):
#     return send_from_directory("web_app/build/_app", path)



# Define the path to the directory containing the static files
if os.path.isdir("web"):
    WEB_BUILD_DIR = os.path.abspath("web/build")
else:
    WEB_BUILD_DIR = os.path.abspath("../web/build")

@app.route('/')
def index():
    return redirect("/web", code=301)

# @app.route("/_app/<path:path>")
# def sendWebApp4(path):
#     return send_from_directory(WEB_BUILD_DIR+"/_app", path)

@app.route('/web/<path:filename>')
def serve_web_files(filename):
    if not "." in filename:
        filename += ".html"
    return send_from_directory(WEB_BUILD_DIR, filename)

# Optional: Serve the index.html file for the root /web path
@app.route('/web/')
def serve_web_index():
    return send_from_directory(WEB_BUILD_DIR, "index.html")




@app.route("/api/user/create",  methods=['GET', 'POST'])
@json_params(["email: str", "passw: str"])
def create_user(email: str, passw:str): 
    user = users.get_user(email=email)
    if user:
        raise NotAcceptable("email address already exists")
    if len(passw) < 5:
        raise NotAcceptable("password must have at least 5 characters")
    user = users.create_user(email, passw)
    if not user:
        raise Exception("failed to create user")
    return {
        "id": user.id,
        "email": user.email
    }

@app.route("/api/user/login",  methods=['GET', 'POST'])
@json_params(["email: str", "passw: str"])
def user_login(email: str, passw: str):
    user = users.get_user(email=email)
    if not user:
        raise NotFound("email does not exists")
    if user.passw != passw:
        raise NotAcceptable("invalid password")
    s = format(random.getrandbits(128), 'x')
    users.create_session(user.id, s)
    user = users.get_user(sesskey=s)
    if user:
        return {"sesskey": s}
    raise Exception("failed to create user session")

@app.route("/api/device/list",  methods=['GET', 'POST'])
@json_params(["sesskey: str"])
def user_devices(sesskey: str):
    user = users.get_user(sesskey=sesskey)
    if not user:
        raise InvalidSession()
    
    user_devices = [device for device in devices.get_devices(user.id)]
    devices_info = [device_info(device.devkey) for device in user_devices]
    return {"devices": devices_info,}

@app.route("/api/device/create",  methods=['GET', 'POST'])
@json_params(["sesskey: str", "devname: str"])
def create_device(sesskey: str, devname: str):
    user = users.get_user(sesskey=sesskey)
    if not user:
        raise InvalidSession()
    devname = devname.strip()
    if len(devname) < 2:
        raise NotAcceptable("device name must have least 2 characters")

    for device in devices.get_devices(user.id):
        if device.name == devname:
            raise NotAcceptable("device with same name already exists")
    rand = format(random.getrandbits(128), 'x')
    device = devices.create_device(user.id, devname, rand)
    return device_info(device.devkey)

@app.route("/api/device/<devkey>/remove",  methods=['GET', 'POST'])
def remove_device(devkey: str):
    if not devices.device_exists(devkey):
        raise DeviceNotFound(devkey)
    devices.remove_device(devkey)
    return {}

@app.route("/api/device/<devkey>/info",  methods=['GET', 'POST'])
def device_info(devkey: str):
    deviceinfo = devices.get_device_info(devkey)
    if not deviceinfo:
        raise DeviceNotFound(devkey)
    
    return {
        "devkey": deviceinfo.devkey,
        "name": deviceinfo.name,
        "connected": deviceinfo.connected,
        "rssi": deviceinfo.rssi,
        "last_connect": deviceinfo.last_connect,
        "last_disconnect": deviceinfo.last_disconnect,
        "connections_24h": deviceinfo.connections_24h_count,
    }
    
@app.route("/api/device/<devkey>/update/start",  methods=['GET', 'POST'])
def start_update(devkey: str):
    deviceinfo = devices.get_device_info(devkey)
    update = devices.update_status(devkey)
    if not deviceinfo or not update:
        raise DeviceNotFound(devkey)
    if not deviceinfo.connected:
        raise DeviceNotConnected()
    if update.state == "running": # TODO not hardcode
        raise NotAcceptable("device update already running")

    if not os.path.isdir("update"):
        os.mkdir("update") 
    fpath = "update/%s.bin" % devkey
    if os.path.isfile(fpath):
        os.remove(fpath)
    f = request.files["file"]
    f.save(fpath)
    f.close()

    devices.start_update(devkey, fpath)
    return update_status(devkey)

@app.route("/api/device/<devkey>/update/status",  methods=['GET', 'POST'])
def update_status(devkey: str):
    status = devices.update_status(devkey)
    if not status:
        raise DeviceNotFound(devkey)
    
    return {
        "update": {
            "state"   : status.state,
            "last_started" : status.last_started,
            "progress": status.progress,
            "error" : status.error,
            "sync": 0
        }
    }

@app.route("/api/device/<devkey>/update/cancel",  methods=['GET', 'POST'])
def cancelUpdate(devkey: str):

    if not devices.device_exists(devkey):
        raise DeviceNotFound(devkey)

    devices.cancel_update(devkey)
    return {}


# @app.route("/device/<devkey>/update")
# def download_update(devkey: str):
#     user, device = users.getUserDevice(devkey)
#     if not user or not device:
#         raise DeviceNotFound(devkey)
    
#     fpath = "update/%s.bin" % devkey

#     if device.update.running() and os.path.isfile(fpath):
#         return send_file(fpath)
#     else:
#         raise NotAcceptable("no update available")

@app.route("/api/device/<devkey>/logs", methods=['GET', 'POST'])
@json_params(["date: str", "startid: int"])
def device_logs(devkey: str, date: str, startid: int):
    
    if not devices.device_exists(devkey):
        raise DeviceNotFound(devkey)
    
    if len(date) < 2:
        date = None
    
    logs = devices.get_logs(devkey, date, startid)
    formatted = [{
        "id": log.id,
        "timestamp": log.timestamp,
        "level": log.level,
        "message": log.message,
        "date": log.date
    } for log in logs]
    return {"logs": formatted}

@app.route("/api/device/<devkey>/restart",  methods=['GET', 'POST'])
def restart_device(devkey: str):
    deviceinfo = devices.get_device_info(devkey)
    if not deviceinfo:
        raise DeviceNotFound(devkey)
    if not deviceinfo.connected:
        raise DeviceNotConnected()

    res = devices.restart_device(devkey)
    return {"result": res if res else False}


@sock.route("/device/<devkey>/ws")
def device_ws(ws: Server, devkey: str):

    if not devices.device_exists(devkey):
        ws.close()
        raise DeviceNotFound(devkey)
    
    devices.handle_device_ws(devkey, ws)





if __name__ == "__main__":
    time.sleep(1)
    app.run(host='0.0.0.0', port=9000, debug=True, )
