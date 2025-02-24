

#include <ESPAdmin.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <WiFiClient.h>
#include <WSClient.h>
#include <Update.h>
#include <EEPROM.h>



#define ESPAD_TIMEOUT_MS        1000l*5
#define ESPAD_PING_INTERVAL_MS  1000l*60*2
#define ESPAD_PING_TIMEOUT_MS   1000l*10

#define ESPAD_CMD_CLOSE         "CLOSE"
#define ESPAD_CMD_SETTINGS      ""
#define ESPAD_CMD_LIVEDATA      ""
#define ESPAD_CMD_PING          "PING"
#define ESPAD_CMD_PONG          "PONG"
#define ESPAD_CMD_LOG           "LOG"
#define ESPAD_CMD_UPDATE        "UPDATE"
#define ESPAD_CMD_RESTART       "RESTART"

#define ESPAD_UPDATE_REQUEST    "GET /device/%s/update HTTP/1.1\r\nHost: %s:%d\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n"

#define ESPAD_MESSAGE_MAX_LEN   1024*4

Client* _client;
Client* _updateclient;
WSClient* _wsclient;
int _newclient = 0;
int _newupdateclient = 0;

StaticJsonDocument<ESPAD_MESSAGE_MAX_LEN> _doc;
uint8_t _buffer[ESPAD_MESSAGE_MAX_LEN];

char _devicekey[128];
char _host[128];
int _port;

int _cancelupdate = 0;

uint32_t _timestamp;
uint32_t _lasttimestamp;
uint32_t _pingstarted = 0;
uint32_t _lastMessage = 0;
uint32_t _lastreconn = 0;



int __stop()
{
    if(_wsclient!=NULL)
        _wsclient->stop();
    _pingstarted = 0;
    _host[0] = '\0';
    return 1;
}

int __connected()
{
    return _wsclient!=NULL && _wsclient->connected();
}

int sendMessage(const char* cmd, StaticJsonDocument<ESPAD_MESSAGE_MAX_LEN> &doc)
{
    doc["cmd"] = cmd;
    doc["devkey"] = _devicekey;
    doc["rssi"] =  WiFi.RSSI();

    int res = serializeJson(doc, _buffer, sizeof(_buffer));
    if(res < 1)
    {
        log_e("serializeJson() error %d", res);
        return 0;
    }
    else if(res+1 > sizeof(_buffer))
    {
        log_e("message is too long, max buffer size %d", sizeof(_buffer));
        return 0;
    }
    
    if(_wsclient!=NULL)
    {
        res = _wsclient->writeFrame(WS_OPCODE_TEXT, _buffer, res);
        if(res > 0)
        {
            log_d("sent %s message (%d bytes)", cmd, res);
            _lastMessage = millis();
            return res;
        }
            
        log_e("writeFrame() error %d", res);
        __stop();
        return 0;
    }
    else
    {
        log_e("wsclient not connected");
        return 0;
    }
}

int abortUpdate(const char* error)
{
    
    log_e("%s", error);

    if(_wsclient && _wsclient->connected())
    {
        _doc.clear();
        _doc["state"] = -1;
        _doc["error"] = error;
        sendMessage(ESPAD_CMD_UPDATE, _doc);
    }

    if(_updateclient && _updateclient->connected())
        _updateclient->stop();

    Update.abort();
    return 0;       
}

int performUpdate()
{

    log_d("starting update");

    if(!__connected())
    {
        abortUpdate("update failed, not connected");
        return 0;
    }
        
    char buff[1024]; //todo stack overflow?
    int len = snprintf(buff, sizeof(buff), ESPAD_UPDATE_REQUEST, _devicekey, _host, _port);
    if(len+1 > sizeof(buff))
    {
        abortUpdate("update failed, snprintf() error");
        return 0;
    }

    if(_updateclient==NULL)
    {
        _updateclient = new WiFiClient();
        _newupdateclient = 1;
    }
    
  
    // log_v("\n%s", buff);
    int res = _updateclient->connect(_host, _port);
    if(!res)
    {
        abortUpdate("update failed, connect() error");
        return 0;
    }

    res = _updateclient->write((uint8_t*)buff, len);
    if(res < len)
    {
        abortUpdate("update failed, write() error");
        return 0;
    }

    int pos = 0;
    uint32_t started = millis();

    String resp = "";
    while (true)
    {
        int c = _updateclient->read();
        if(c > -1)
            buff[pos++] = c;

        if(pos > sizeof(buff)-2)
        {
            abortUpdate("update failed, response header too long");
            return 0;
        }

        if(millis()-started > ESPAD_TIMEOUT_MS)
        {
            abortUpdate("update failed, response timeout");
            return 0;
        }

        if(pos > 5 && buff[pos-4]=='\r' && buff[pos-3]=='\n' && buff[pos-2]=='\r' && buff[pos-1]=='\n') 
        { //header complete
            resp = String(buff, pos);
            // log_v("update response header");
            // log_v("%s", resp.c_str());
            break;
        }
    }
    
    resp.toLowerCase();
    int idx = resp.indexOf("http/1.1");
    if(idx < 0)
    {
        abortUpdate("update failed, error extracting response code");
        return 0;
    }
    String codestr = resp.substring(idx+9, idx+12);
    int code = codestr.toInt();
    if(code!=200)
    {
        abortUpdate("update failed, invalid response code");
        return 0;
    }
    idx = resp.indexOf("content-length");
    if(idx < 0)
    {
        abortUpdate("update failed, error extracting response length");
        return 0;
    }
    String s1 = resp.substring(idx+15);
    idx = s1.indexOf("\r\n");
    if(idx < 1)
    {
        abortUpdate("update failed, error extracting response length");
        return 0;
    }
    String s2 = s1.substring(0, idx);
    s2.trim();
    len = s2.toInt();

    log_d("update length %d bytes", len);

    if(!Update.begin(len, U_FLASH)) 
    {
        // log_d("Update.begin() failed err %d", Update.getError());
        abortUpdate("update failed, Update.begin() error");
        return 0;
    }

    // if(md5!=NULL) 
    // {
    //     if(!Update.setMD5(md5)) 
    //     {
    //         log_d("Update.setMD5() failed, md5 %s", md5);
    //         return 0;
    //     }
    // }
    
    int count = 0;
    uint32_t lastreport = 0;

    while (count < len)
    {
        int remaining = len - count;
        if(remaining > sizeof(buff))
            remaining = sizeof(buff);


        if(_updateclient->available())
        {
            res = _updateclient->read((uint8_t*)buff, remaining);
            if(count==0 && res > 0 && buff[0] != 0xE9)
            {
                abortUpdate("update failed, invalid file (first byte != 0xE9)");
                return 0;
            }

            int write_ = Update.write((uint8_t*)buff, res);
            if(write_ != res)
            {
                // log_e("Update write() failed, err %d", Update.getError());
                abortUpdate("update failed, Update.write() error");
                return 0;
            }
            
            count+=res;
        }
     
        
        if(!lastreport || millis()- lastreport > 1000l*3)
        {
            int progress = 100.0 / len * count;
            log_d("update progress %d%", progress);
            _doc.clear();
            _doc["state"] = progress;
            _doc["error"] = "";
            sendMessage(ESPAD_CMD_UPDATE, _doc);

            lastreport = millis();
        }
        
        if(millis()-started > 1000l*60)
        {
            abortUpdate("update failed, timeout downloading data");
            return 0;
        }

        ESPAdmin::loop(); //sets _cancelupdate

        if(_cancelupdate)
        {
            abortUpdate("update failed, user canceled");
            return 0;
        }
    }

     
    // Update.onProgress(onUpdateProgress);
    // if(Update.writeStream(*_updateclient) != len) 
    // {
    //     // log_d("Update.writeSteam() failed err %d", Update.getError());
    //     abortUpdate("Update.writeSteam() failed");
    //     return 0;
    // }

    if(!Update.end()) 
    {
        // log_e("Update.end() failed err %d", Update.getError());
        abortUpdate("update failed, Update.end() error");
        return 0;
    }

    _doc.clear();
    _doc["state"] = 200; //complete
    _doc["error"] = "";
    sendMessage(ESPAD_CMD_UPDATE, _doc);
    delay(3000);
    __stop();
    _updateclient->stop();
    log_d("update complete");
    log_d("restarting in 3s");
    delay(3000);
    ESP.restart();

    return 1;
}

int processUpdate(StaticJsonDocument<ESPAD_MESSAGE_MAX_LEN> &doc)
{
    if(doc.containsKey("cancel"))
    {
        _cancelupdate = 1;
        log_d("canceling update");
        return 1;
    }

    _cancelupdate = 0;
    return performUpdate();
}

int processPing(StaticJsonDocument<ESPAD_MESSAGE_MAX_LEN> &doc)
{
    uint32_t d = 0;
    if(doc.containsKey("dummy"))
        d = doc["dummy"];
    
    _doc.clear();
    _doc["dummy"] = d;
    return sendMessage(ESPAD_CMD_PONG, _doc);
}

int processPong(StaticJsonDocument<ESPAD_MESSAGE_MAX_LEN> &doc)
{
    if(doc.containsKey("dummy"))
    {
        uint32_t d = doc["dummy"];
        _pingstarted = 0;
        return 1;
    }
    else
    {
        log_e("message missing key 'dummy'");
    }

    return 0;
}

int processClose(StaticJsonDocument<ESPAD_MESSAGE_MAX_LEN> &doc)
{
    if(doc.containsKey("reason") && doc["reason"].as<const char*>() != NULL)
        log_w("server closing connection '%s'", doc["reason"].as<const char*>());
    else
        log_w("server closing connection");
    
    __stop();
    return 1;
}

int processRestart(StaticJsonDocument<ESPAD_MESSAGE_MAX_LEN> &doc)
{
    log_d("restarting in 3s");
    __stop();
    delay(3000);
    ESP.restart();
    return 1;
}

int processMessage(StaticJsonDocument<ESPAD_MESSAGE_MAX_LEN> &doc)
{
    if(doc.containsKey("cmd"))
    {
        const char* cmd = doc["cmd"];
        log_d("Processing %s message", cmd);

        if(doc.containsKey("timestamp"))
        {
            _timestamp = doc["timestamp"];
            _lasttimestamp = millis();
        }
        else
        {
            log_w("message missing key 'timestamp'");
        }

        if(strcmp(ESPAD_CMD_PING, cmd)==0) //CMD_PING
        {
            processPing(doc);
        }
        else if(strcmp(ESPAD_CMD_PONG, cmd)==0)
        {
            processPong(doc);
        }
        else if(strcmp(ESPAD_CMD_CLOSE, cmd)==0)
        {
            processClose(doc);
        }
        else if(strcmp(ESPAD_CMD_UPDATE, cmd)==0)
        {
            processUpdate(doc);
        }
        else if(strcmp(ESPAD_CMD_RESTART, cmd)==0)
        {
            processRestart(doc);
        }
        else
        {
            log_w("unknown cmd '%s'", cmd);
            return 0;
        }
    }
    else
    {
        log_e("message missing key 'cmd'");
        __stop();
        return 0;
    }

    return 1;
}

int processWSFrame(uint8_t opcode, uint8_t* _payload, size_t len)
{
    if(_wsclient==NULL)
        return 0;
    
    if(opcode==WS_OPCODE_CONT)
    {
        log_e("continuation ws frames not supported");
        __stop();
    }
    else if(opcode==WS_OPCODE_TEXT || opcode==WS_OPCODE_BIN) //todo check payload len > 0 ?
    {
        _doc.clear();
        auto res = deserializeJson(_doc, _payload, len);
        if(res == DeserializationError::Ok)
        {
            processMessage(_doc);
        }
        else
        {
            log_e("failed to process payload, deserializeJson() error");
            __stop(); //todo stop?
        }
    }
    else if(opcode==WS_OPCODE_CLOSE)
    {
        if(len==2)
        {
            uint16_t errcode = (_payload[0] << 8) | _payload[1];
            log_w("server closing ws %d", errcode);
        }
        else
        {
            log_w("server closing ws, unknown reason");
        }

        _wsclient->writeFrame(WS_OPCODE_CLOSE, NULL, 0);
        __stop();
    }
    else if(opcode==WS_OPCODE_PING)
    {
        _wsclient->writeFrame(WS_OPCODE_PONG, _payload, len);
        log_d("sent ping response");
    }
    else if(opcode==WS_OPCODE_PONG)
    {
        log_d("pong frame received");
    }
    else
    {
        log_e("unknown ws frame opcode %d", opcode);
        __stop();
    }

    return 1;
}

int ESPAdmin::setUpdateClient(Client* client)
{
    if(client==NULL)
        return 0;

    if(_newupdateclient && _updateclient!=NULL)
        delete _updateclient;

    _updateclient = client;
    _newupdateclient = 0;

    return 1;
}

int ESPAdmin::setWSClient(Client* client)
{
    if(client==NULL)
        return 0;

    if(_wsclient!=NULL)
        _wsclient->setClient(client);
    
    if(_newclient && _client!=NULL)
        delete _client;

    _client = client;
    _newclient = 0;

    return 1;
}

int ESPAdmin::connect(const char* host, int port, const  char* devicekey)
{   

    _port = port;
    strncpy(_host, host, sizeof(_host));
    strncpy(_devicekey, devicekey, sizeof(_devicekey));

    char path[256];
    int res = snprintf(path, sizeof(path), "/device/%s/ws", devicekey);

    if(_wsclient==NULL)
    {
        if(_client==NULL)
        {
            _client = new WiFiClient();
            _newclient = 1;
        }

        _wsclient = new WSClient(_client, path);
    }

    log_d("starting connection to %s:%d", _host, _port);
    _wsclient->setTimeout(ESPAD_TIMEOUT_MS);
    if(_wsclient->connect(_host, port))
    {
        log_d("wsclient connected");
        ping();
        return 1;
    }
    else
    {
        log_e("wsclient failed to connect");
        return 0;
    }

}

int ESPAdmin::connected()
{
    return __connected();  
}

int ESPAdmin::stop()
{
    return __stop();
}

int ESPAdmin::ping()
{
    if(!connected())
        return 0;

    _doc.clear();
    _doc["dummy"] = millis();
    if(sendMessage(ESPAD_CMD_PING, _doc))
    {
        _pingstarted = millis();
        return 1;
    }
        
    return 0;
}

int ESPAdmin::log(LogType type, const char* format, ...)
{

    va_list arg;
    va_start(arg, format);
    char msg[256];
    unsigned int len = vsnprintf(msg, sizeof(msg), format, arg);
    va_end(arg);
    if(len > sizeof(msg) - 1)
    {
        log_w("log message is too long");
        return 0;
    }

    if(connected()) //todo  queue to send later
    {
        _doc.clear();
        _doc["level"] = type;
        _doc["message"] = msg;
        _doc["timestamp"] = unixtime();
   
        sendMessage(ESPAD_CMD_LOG, _doc);
    }

    if(type == LogType::VERBOSE)
    {
       Serial.printf("[VERBOSE]%s\n", msg);
    }
    else if(type == LogType::DEBUG)
    {
        Serial.printf("[DEBUG]%s\n", msg);
    }
    else if(type == LogType::INFO)
    {   
        Serial.printf("[INFO]%s\n", msg);
    }
    else if(type == LogType::WARNING)
    {
        Serial.printf("[WARNING]%s\n", msg);
    }
    else if(type == LogType::ERROR)
    {
        Serial.printf("[ERROR]%s\n", msg);
    }
    else if(type == LogType::CRITICAL)
    {
        Serial.printf("[CRITICAL]%s\n", msg);
    }

    return 1;
}

uint32_t ESPAdmin::unixtime()
{
    if(!_timestamp)
        return 0;

    return _timestamp + ((millis()-_lasttimestamp)/1000);
}

int ESPAdmin::loop()
{
    if(_wsclient != NULL)
    {
        uint8_t opcode = 0;
        int len = _wsclient->readFrame(&opcode, _buffer, sizeof(_buffer)-1);
        if(len > -1)
        {
            processWSFrame(opcode, _buffer, len);
            _lastMessage = millis();
        }

        if(_pingstarted && millis()-_pingstarted > ESPAD_PING_TIMEOUT_MS)
        {
            log_e("timeout waiting for ping response");
            stop();
        }

        if(!_pingstarted && millis()-_lastMessage > ESPAD_PING_INTERVAL_MS && connected())
        {
            ping();
            _lastMessage = millis();
        }

        if((!_lastreconn || millis() - _lastreconn > 1000l*5) && !connected())
        {
            //todo reconnect
            
        }

    }


   return 1;
}

// Queue handle
QueueHandle_t dataQueue;

#define ESPAD_TASK_CONNECT 1
#define ESPAD_TASK_DISCONN 2
#define ESPAD_TASK_LOG 3
#define ESPAD_TASK

//begin(url, deviceKey, autoConnect=5000)
//status

void TaskProcessData(void *pvParameters) {
    int receivedValue;
    while (1) {
        if (xQueueReceive(dataQueue, &receivedValue, portMAX_DELAY)) {
            Serial.print("Received Sensor Data: ");
            Serial.println(receivedValue);
        }
    }
}


























































