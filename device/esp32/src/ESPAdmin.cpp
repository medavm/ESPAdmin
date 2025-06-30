

#include <ESPAdmin.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <Update.h>
#include <EEPROM.h>
#include "client/TCPClient.h"
#include "websocket/WSClient.h"
#include "base64.hpp"


#define ESPAD_MAX_MESSAGE_LEN   1024*4


#define ESPAD_CLIENT_TIMEOUT_MS 1000l*5
#define ESPAD_PING_INTERVAL_MS  1000l*60*1
#define ESPAD_PING_TIMEOUT_MS   1000l*10
#define ESPAD_RECONNECT_INTERVAL_MS 1000l*30
#define ESPAD_UPDATE_TIMEOUT 1000l*60*2
#define ESPAD_UPDATE_CHUNK_TIMEOUT 1000l*60*5
#define ESPAD_MAX_UPDATE_CHUCK_SIZE ESPAD_MAX_MESSAGE_LEN - 256
#define ESPAD_UPDATE_ACK_INTERVAL_MS 1000l*1
#define ESPAD_UPDATE_STATUS_NOT_RUNNING 0
#define ESPAD_UPDATE_STATUS_ABORT -1
#define ESPAD_UPDATE_STATUS_ACK -2
#define ESPAD_UPDATE_STATUS_ERROR -3

#define ESPAD_UPDATE_REQUEST    "GET /device/%s/update HTTP/1.1\r\nHost: %s:%d\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n"




#define ESPAD_MSG_CONNECT           "connect"
#define ESPAD_MSG_PING              "ping"
#define ESPAD_MSG_PING_ACK          "ping-ack"
#define ESPAD_MSG_CLOSE             "close"
#define ESPAD_MSG_SETTINGS          "settings"
#define ESPAD_MSG_SETTINGS_ACK      "settings-ack"
#define ESPAD_MSG_LIVEDATA          "livedata"
#define ESPAD_MSG_LOG               "log"
#define ESPAD_MSG_UPDATE            "update"
#define ESPAD_MSG_UPDATE_ACK        "update-ack"
#define ESPAD_MSG_RESTART           "restart"
#define ESPAD_MSG_RESTART_ACK       "restart-ack"

#define ESPAD_STATUS_CONNECTING 1
#define ESPAD_STATUS_CONNECTED 2
#define ESPAD_STATUS_CONNECTION_ERROR 3
#define ESPAD_STATUS_DISCONNECTED 4

#define ESPAD_TASK_CONNECT 1 
#define ESPAD_TASK_DISCONNECT 2
#define ESPAD_TASK_LIVEDATA 3
#define ESPAD_TASK_LOG 4

typedef struct{
    char host[220];
    int port;
} TaskConnect;

typedef struct{
    char msg[220];
    int level;
    uint32_t timestamp;
} TaskLog;

#define ESPAD_LIVEDATA_QUEUE_SIZE 10
uint8_t _livedataQueueBuffer[64];
QueueHandle_t _livedataQueue = NULL;

#define ESPAD_LOG_QUEUE_SIZE 10 
uint8_t _logQueueBuffer[128];
QueueHandle_t _logQueue = NULL;


static int _status = ESPAD_STATUS_DISCONNECTED; 
static int _begin = 0;
static int _reconnectInterval = 0;
static int _lastConnect = 0;

static Client* _client;
static Client* _updateclient;
static WSClient* _wsclient;
static int _newclient = 0;
static int _newupdateclient = 0;

static StaticJsonDocument<ESPAD_MAX_MESSAGE_LEN> _doc;
static uint8_t _buffer[ESPAD_MAX_MESSAGE_LEN];

static char _devicekey[128];
static char _host[128];
static int _port;

static int _cancelupdate = 0;

TaskHandle_t _loopTaskHandle = NULL;

static uint32_t _timestamp;
static uint32_t _lasttimestamp;
static uint32_t _pingstarted = 0;
static uint32_t _lastMessage = 0;

static uint32_t _update_started = 0;
static uint32_t _update_status = 0;
static uint32_t _update_size = 0;
static uint32_t _update_last_chunk = 0;
static uint32_t _update_last_ack = 0;
static char _update_md5[128];
static uint8_t* _update_buffer = NULL;
static uint32_t _update_buffer_size = 0;

static uint32_t _settingsVersion = 0;
static uint32_t _pingInterval = ESPAD_PING_INTERVAL_MS;



static int _disconnect(int allowReconnect, int cleanQueues, int statusError){

    if(_wsclient != NULL){
        _wsclient->stop(); //will trigger error on running task using client?
    }
        
    if(cleanQueues){
        //clear data
        uint8_t buffer[256]; //TODO 256 enough?
        while (_livedataQueue != NULL && xQueueReceive(_livedataQueue, &buffer, 0) == pdTRUE);
        while (_logQueue != NULL && xQueueReceive(_logQueue, &buffer, 0) == pdTRUE);
    }
    
    if(_status != ESPAD_STATUS_DISCONNECTED)
        log_i("disconnected from the server");
    _status = statusError ? ESPAD_STATUS_CONNECTION_ERROR : ESPAD_STATUS_DISCONNECTED;
    if(!allowReconnect)
        _reconnectInterval = 0;
    
    _pingstarted = 0;
    return 1;
}

static int _sendMessage(const char* type, JsonObject& data){

    _doc.clear();
    _doc["message_type"] = type;
    _doc["message_data"] = data;
    if(WiFi.isConnected())
        _doc["rssi"] =  WiFi.RSSI();
    _doc["timestamp"] = ESPAdmin::unixtime();

    int res = serializeJson(_doc, _buffer, sizeof(_buffer));
    if(res < 1){
        log_e("failed to serialize json %d", res);
        return 0;
    } else if(res+1 > sizeof(_buffer)){
        log_e("failed to serialize json, message too long");
        return 0;
    }

    if(_wsclient == NULL){
        log_e("failed to send message, ws null");
        return 0;
    }

    res = _wsclient->writeFrame(WS_OPCODE_TEXT, _buffer, res);
    if(res < 1){
        log_e("failed to write ws frame %d", res);
        _disconnect(true, false, true);
        return 0;
    }

    log_d("sent '%s' message (%d bytes)", type, res);
    _lastMessage = millis();
    return res;

}

static int _processPingAck(JsonObject &data){
    _pingstarted = 0;
    return 1;
}

static int _processPing(JsonObject &data) {

    StaticJsonDocument<128> doc;
    JsonObject data_ = doc.to<JsonObject>();
    _sendMessage(ESPAD_MSG_PING_ACK, data_);
    return 1;
}

static int _processRestart(JsonObject &data) {
    StaticJsonDocument<128> doc;
    JsonObject data_ = doc.to<JsonObject>();
    _sendMessage(ESPAD_MSG_RESTART_ACK, data_);
    log_d("device restarting in 3s");
    delay(3000);
    ESP.restart();
    return 1;
}

static int _processClose(JsonObject &data){

    if(data.containsKey("reason")){
        const char* reason = data["reason"];
        log_w("server is closing connection, reason: %d", reason);
    } else {
         log_w("server is closing connection, unknown reason");
    }

    _disconnect(false, true, true); //TODO try to connect again?
    return 1;
}

static int _processSettings(JsonObject &data){
    //TODO
    log_w("not implemented");
    return 0;
}

static int _updateError(const char* error_message){
    StaticJsonDocument<128> doc;
    JsonObject data = doc.to<JsonObject>();
    data["status"] = ESPAD_UPDATE_STATUS_ERROR;
    data["error"] = error_message;
    _sendMessage(ESPAD_MSG_UPDATE_ACK, data);
    Update.abort();
    _update_started = 0;
    _update_status = 0;
    return 0;
}

static int _updateAck(){
    StaticJsonDocument<128> doc;
    JsonObject obj = doc.to<JsonObject>();
    obj["status"] = _update_status;
    if(_update_status > 0){
        obj["size"] = _update_size;
        obj["md5"] = _update_md5;
        int progress = (100.0 / _update_size ) *_update_status;
        log_d("update progress %d%", progress);
    }
    _sendMessage(ESPAD_MSG_UPDATE_ACK, obj);
    _update_last_ack = millis();
    return 1;
}

static int _processUpdate(JsonObject &data){



    if(!data.containsKey("status")){
        log_e("update message missing 'status' key");
        return _updateError("invalid data, missing 'status' key");
    }

    int status = data["status"];
    if(status < 0){
        if(status == ESPAD_UPDATE_STATUS_ABORT){
            log_e("update canceled");
            return _updateError("update canceled");
        } else if(status == ESPAD_UPDATE_STATUS_ACK){
            return _updateAck();
        } else {
            log_e("invalid update status %d", status);
            return _updateError("invalid update status");
        }
    }

    if(status != _update_status){
        log_e("unexpected update status (expected %d got %d)", _update_status, status);
        //return _updateError("unexpected update data index");
        return 0; //ignore
    }

    if (_update_status == 0 && !data.containsKey("size")){
        log_e("update message missing 'size' key");
        return _updateError("invalid data, missing size key");
    }

    if (_update_status == 0 && !data.containsKey("md5")){
        log_e("update message missing 'md5' key");
        return _updateError("invalid data, missing md5 key");
    }

    if (!data.containsKey("data_size")){
        log_e("update message missing 'data_size' key");
        return _updateError("invalid data, missing data_size key");
    }

    if (!data.containsKey("data")){
        log_e("update message missing 'data' key");
        return _updateError("invalid data, missing data key");
    }

    if(_update_status == 0){
        uint32_t size = data["size"];
        if(size <= 0){
            log_e("invalid update size %d", size);
            return _updateError("invalid update size");
        }

        const char* md5 = data["md5"];
        if(md5 == NULL || strlen(md5) != 32){
            if(md5 != NULL)
                log_e("invalid update md5 %s", md5);
            else
                log_e("invalid update md5 NULL");
            return _updateError("invalid update md5");
        }

        _update_size = size;
        strncpy(_update_md5, md5, sizeof(_update_md5)-1);
    }
    
    uint32_t chuck_size = data["data_size"];
    if(chuck_size <= 0 || chuck_size > ESPAD_MAX_UPDATE_CHUCK_SIZE){
        log_e("invalid update chuck_size %d", chuck_size);
        return _updateError("invalid update chuck_size");
    }

    const char *update_data = data["data"];
    if(_update_buffer == NULL || chuck_size > _update_buffer_size){
        if(_update_buffer != NULL)
            _update_buffer = (uint8_t*)realloc(_update_buffer, chuck_size);
        else
            _update_buffer = (uint8_t*)malloc(chuck_size);
        if(_update_buffer == NULL){
            log_e("failed to allocate update buffer");
            return _updateError("failed to allocate update buffer");
        }
        _update_buffer_size = chuck_size;
        log_d("update buffer allocated (%d bytes)", _update_buffer_size);
    }

    // int res = _base64Decode(update_data, _update_buffer, _update_buffer_size-1);
    int res = decode_base64((unsigned char*)update_data, (unsigned char*)_update_buffer);
    if(res < chuck_size){
        log_e("failed to decode update data");
        return _updateError("failed to decode update data");
    }

    if(!_update_started){
        log_i("starting device update (%d bytes, %s)", _update_size, _update_md5);
        _update_started = millis();
    }
       
    if(!_update_status && !Update.begin(_update_size, U_FLASH)){
        log_e("update failed, Update.begin() error %d", Update.getError());
        return _updateError("Update.begin() failed");
    }

    if(!_update_status && !Update.setMD5(_update_md5)){
        log_e("update failed, Update.setMD5() error %d", Update.getError());
        return _updateError("Update.setMD5() failed");
    }

    res = Update.write(_update_buffer, chuck_size);
    if(res != chuck_size){
        log_e("update failed, Update.write() error %d", Update.getError());
        return _updateError("Update.write() failed");
    }
    _update_last_chunk = millis();

    _update_status += chuck_size;
    // if(_update_status == chuck_size) //dont send before it asks
    //     _updateAck();

    if(_update_status == _update_size){
        if(!Update.end()) {
            log_e("update failed, Update.end() error %d", Update.getError());
            return _updateError("Update.end() error");
        }

        _updateAck();
        StaticJsonDocument<128> doc;
        JsonObject obj = doc.to<JsonObject>();
        obj["reason"] = "restarting due to update";
        _sendMessage(ESPAD_MSG_CLOSE, obj);
        delay(1500);
        _disconnect(false, true, false);
        log_d("update complete, restarting in 1s");
        delay(1000);
        ESP.restart();
        
    }

    return 1;
}

static int _processMessage(StaticJsonDocument<ESPAD_MAX_MESSAGE_LEN> &doc)
{

    if(doc.containsKey("timestamp")){
        _timestamp = doc["timestamp"];
        _lasttimestamp = millis();
    }
    else{
        log_w("invalid json message, missing key 'timestamp'");
    }

    if (!doc.containsKey("message_type")) {
        log_e("invalid json message, missing key 'message_type'");
        _disconnect(true, false, true);
        return 0;
    }

    if (!doc.containsKey("message_data") || !doc["message_data"].is<JsonObject>()) {
        log_e("invalid json message, 'message_data' missing or not an object");
        _disconnect(true, false, true);
        return 0;
    }

    const char* type = doc["message_type"];
    if(type == NULL){
        log_e("invalid json message type");
        _disconnect(true, false, true);
        return 0;
    }
    JsonObject data = doc["message_data"];
    log_d("processing '%s' message", type);

    if(strcmp(type, ESPAD_MSG_PING) == 0) {
        return _processPing(data);
    } else if(strcmp(type, ESPAD_MSG_PING_ACK) == 0) {
        return _processPingAck(data);
    } else if(strcmp(type, ESPAD_MSG_CLOSE) == 0) {
        return _processClose(data);
    } else if(strcmp(type, ESPAD_MSG_SETTINGS) == 0) {
        return _processSettings(data);
    } else if(strcmp(type, ESPAD_MSG_UPDATE) == 0) {
        return _processUpdate(data);
    } else if(strcmp(type, ESPAD_MSG_RESTART) == 0) {
        return _processRestart(data);
    } else {
        log_w("unknown message type '%s'", type);
        return 0;
    }
    
}

static int _processWSFrame(uint8_t opcode, uint8_t* _payload, size_t len)
{
    if(_wsclient==NULL)
        return 0;
    
    if(opcode==WS_OPCODE_CONT){
        log_e("continuation ws frames not supported");
        _disconnect(true, false, true);

    } else if(opcode==WS_OPCODE_TEXT || opcode==WS_OPCODE_BIN) { //todo check payload len > 0 ?
        _doc.clear();
        auto res = deserializeJson(_doc, _payload, len);
        if(res == DeserializationError::Ok){
            _processMessage(_doc);
        } else {
            log_e("failed to process payload, deserializeJson() error");
            _disconnect(true, false, true);
        }

    } else if(opcode==WS_OPCODE_CLOSE) {
        if(len==2) {
            uint16_t errcode = (_payload[0] << 8) | _payload[1];
            log_w("server closing ws %d", errcode);
        } else {
            log_w("server closing ws, unknown reason");
        }

        _wsclient->writeFrame(WS_OPCODE_CLOSE, NULL, 0);
        delay(1000);
        _disconnect(true, false, true);

    } else if(opcode==WS_OPCODE_PING) {
        
        _wsclient->writeFrame(WS_OPCODE_PONG, _payload, len);
        log_d("sent ws ping response");

    } else if(opcode==WS_OPCODE_PONG) {
        log_d("pong ws frame received");

    } else {
        log_e("unknown ws frame opcode %d", opcode);
        _disconnect(true, false, true);
    }

    return 1;
}

static int _pingMessage(){
    StaticJsonDocument<128> doc;
    JsonObject data_ = doc.to<JsonObject>();
    if(_sendMessage(ESPAD_MSG_PING, data_)){
        _pingstarted = millis();
        return 1;
    }

    return 0;
}

static int _connectMessage(){

    
    StaticJsonDocument<128> doc;
    JsonObject data_ = doc.to<JsonObject>();
    data_["settings_version"] = _settingsVersion;
    return _sendMessage(ESPAD_MSG_CONNECT, data_);

}

static int _logMessage(int level, const char* msg, uint32_t ts){    

    StaticJsonDocument<256> doc;
    JsonObject data_ = doc.to<JsonObject>();

    data_["log_level"] = level;
    data_["log_message"] = msg;
    data_["log_timestamp"] = ts;
    return _sendMessage("log", data_);

    if(level == LogType::VERBOSE)
        Serial.printf("[VERBOSE]%s\n", msg);  
    else if(level == LogType::DEBUG) 
        Serial.printf("[DEBUG]%s\n", msg);
    else if(level == LogType::INFO)
        Serial.printf("[INFO]%s\n", msg); 
    else if(level == LogType::WARNING)
        Serial.printf("[WARNING]%s\n", msg);
    else if(level == LogType::ERROR)
        Serial.printf("[ERROR]%s\n", msg);
    else if(level == LogType::CRITICAL)
     Serial.printf("[CRITICAL]%s\n", msg);
   
}

static void _loopTask(void *pvParameters)
{
    while (true){
        
        static uint32_t _lastReconnect = 0;
        if(_lastReconnect == 0 || (_reconnectInterval > 0 && *_host != '\0' && !ESPAdmin::connected() && millis()-_lastReconnect > _reconnectInterval)){
            ESPAdmin::connect(_reconnectInterval);
            _lastReconnect = millis();
        }

        if(_wsclient!=NULL){

            uint8_t opcode = 0;
            int len = _wsclient->readFrame(&opcode, _buffer, sizeof(_buffer)-1);
            if(len > -1) {
                _processWSFrame(opcode, _buffer, len);
            }

            if (_logQueue != NULL && xQueueReceive(_logQueue, &_logQueueBuffer, 0) == pdTRUE) {
                uint8_t level = _logQueueBuffer[0];
                uint32_t timestamp = *(_logQueueBuffer+1);
                const char* msg = (char*)(_logQueueBuffer+5);
                _logMessage(level, msg, timestamp);
            }

            if(!_pingstarted && millis()-_lastMessage > _pingInterval && ESPAdmin::connected()){
                _pingMessage();
            }

            // if(!_pingstarted && millis()-_lastMessage > 1000l*20 && ESPAdmin::updating()){
            //     _pingMessage();
            // }

            if(_pingstarted && millis()-_pingstarted > ESPAD_PING_TIMEOUT_MS) {
                log_e("ping timeout");
                _disconnect(true, false, true);
            }

            // if(_update_started && millis()-_update_last_chunk > 1000l*20){
            //     log_e("update ack timeout");
            //     _disconnect(true, false, true);
            // }

            if(_update_started && millis()-_update_last_chunk > ESPAD_UPDATE_CHUNK_TIMEOUT){
                log_e("update timeout");
                _updateError("update timeout");
            }

        }
    
        if(!_update_started)
            delay(50);
        delayMicroseconds(100);
    }
    

}

int ESPAdmin::begin(const char* host, int port, const char* deviceKey){
    
    _disconnect(true, true, false);
    _status = ESPAD_STATUS_DISCONNECTED;
    _reconnectInterval = ESPAD_RECONNECT_INTERVAL_MS;
    _port = port;
    strncpy(_host, host, sizeof(_host));
    strncpy(_devicekey, deviceKey, sizeof(_devicekey));
    Update.abort();

    if(_livedataQueue == NULL){
        _livedataQueue = xQueueCreate(ESPAD_LIVEDATA_QUEUE_SIZE, sizeof(_livedataQueueBuffer));
        if(_livedataQueue == NULL){
            log_e("failed to create livedata queue");
            return 0;
        }
    }
    
    if(_logQueue == NULL){
        _logQueue = xQueueCreate(ESPAD_LOG_QUEUE_SIZE, sizeof(_logQueueBuffer));
        if(_logQueue == NULL){
            log_e("failed to create log queue");
            return 0;
        }
    }

    if(_loopTaskHandle == NULL){
        xTaskCreateUniversal(
            _loopTask,      // Task function
            "ESPAdminTask",    // Task name
            8048,           // Stack size (in words, not bytes) 2048 = 8KB
            NULL,           // Task parameters
            1,              // Task priority (higher number = higher priority)
            NULL,            // Task handle (optional, can be used for managing the task)
            ARDUINO_RUNNING_CORE
        );
        log_d("started espad loop task");
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

    _disconnect(false, true, false);
    if(_wsclient!=NULL)
        _wsclient->setClient(client);
    
    if(_newclient && _client!=NULL)
        delete _client;

    _client = client;
    _newclient = 0;

    return 1;
}

int ESPAdmin::connect(int reconnectInterval){

    if(*_host == '\0' || *_devicekey == '\0'){
        log_e("invalid configuration, use begin() first");
        //_disconnect(false, false, false);
        return 0;
    }

    char path[256];
    int res = snprintf(path, sizeof(path), "/device/%s/ws", _devicekey);

    if(_wsclient==NULL){
        if(_client==NULL){
            log_d("creating new tcp client");
            _client = new TCPClient();
            _newclient = 1;
        }

        log_d("creating new ws client");
        _wsclient = new WSClient(_client, path);
    }

    _disconnect(true, false, false);
    _status = ESPAD_STATUS_CONNECTING;
    _reconnectInterval = reconnectInterval;
    log_d("starting connection to %s:%d", _host, _port);
    _wsclient->setTimeout(ESPAD_CLIENT_TIMEOUT_MS);
    if(_wsclient->connect(_host, _port)){
        _status = ESPAD_STATUS_CONNECTED;
        log_i("device successfully connected");
        return _connectMessage();
    } else {
        log_e("wsclient failed to connect");
        return 0;
    }
}

int ESPAdmin::connected(){
    return status() == ESPAD_STATUS_CONNECTED && _wsclient!=NULL && _wsclient->connected(); 
}

int ESPAdmin::disconnect(){
    return _disconnect(false, true, false);
}

int ESPAdmin::status(){
    return _status;
}

int ESPAdmin::log(LogType type, const char* format, ...)
{

    va_list arg;
    va_start(arg, format);
    char msg[128];
    unsigned int len = vsnprintf(msg, sizeof(msg), format, arg);
    va_end(arg);
    if(len > sizeof(msg)-1){
        log_w("log message too large");
        return 0;
    }

    _logQueueBuffer[0] = (uint8_t)type;
    *(_logQueueBuffer+1) = unixtime();
    strncpy((char *)(_logQueueBuffer+5), (const char*) msg, sizeof(_logQueueBuffer)-6);

    if(xQueueSend(_logQueue, _logQueueBuffer, 0) != pdTRUE) { 
        log_w("failed to queue log message");
    }

    return 1;
}

int ESPAdmin::updating(){
    return _update_started > 0;
}

uint32_t ESPAdmin::unixtime()
{
    if(!_timestamp)
        return 0;

    return _timestamp + ((millis()-_lasttimestamp)/1000);
}






























































