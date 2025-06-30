



#include "WSClient.h"

#define WS_UPGRADE_KEY                      "dGhlIHNhbXBsZSBub25jZQ=="
#define WS_UPGRADE_REQUEST                  "GET %s HTTP/1.1\r\nHost: %s:%d\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: %s\r\nSec-WebSocket-Version: %s\r\n\r\n"
#define WS_UPGRADE_REQUEST_PROTOCOL         "GET %s HTTP/1.1\r\nHost: %s:%d\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Protocol: %s\r\nSec-WebSocket-Key: %s\r\nSec-WebSocket-Version: %s\r\n\r\n"

#define WS_TIMEOUT_DEFAULT 1000l*5
#define WS_RECV_BUFFER_SIZE 1024*5
#define WS_PAYLOAD_MAX_SIZE WS_RECV_BUFFER_SIZE - 256

#define WS_MIN_HEADER_LEN 2
#define WS_MAX_HEADER_LEN 8
#define WS_EXT_PAYLOAD_LEN 2
#define WS_MASK_LEN 4

#define WS_CONNECTING 1
#define WS_UPGRADE_INPROGRESS 3
#define WS_UPGRADE_COMPLETE 4
#define WS_DISCONNECTED 5

#define WS_RECV_STATE_EMPTY 0
#define WS_RECV_STATE_MIN_HEADER 1
#define WS_RECV_STATE_FULL_HEADER 2


WSClient::WSClient(Client* client, const char* path, const char* subprotocol)
{
    _client = client;
    strncpy(_path, path, sizeof(_path));
    strncpy(_protocol, subprotocol, sizeof(_protocol));
    _status = WS_DISCONNECTED;
    _recvstate = WS_RECV_STATE_EMPTY;
    _timeout = WS_TIMEOUT_DEFAULT;
}

WSClient::WSClient(Client* client, const char* path)
{
    _client = client;
    strncpy(_path, path, sizeof(_path));;
    _status = WS_DISCONNECTED;
    _recvstate = WS_RECV_STATE_EMPTY;
    _timeout = WS_TIMEOUT_DEFAULT;
    _protocol[0] = '\0';
}

WSClient::WSClient(Client* client)
{
    _client = client;
    _status = WS_DISCONNECTED;
    _recvstate = WS_RECV_STATE_EMPTY;
    _timeout = WS_TIMEOUT_DEFAULT;
    _path[0] = '\0';
    _protocol[0] = '\0';
}

WSClient::WSClient()
{
    _client = NULL;
    _status = WS_DISCONNECTED;
    _recvstate = WS_RECV_STATE_EMPTY;
    _timeout = WS_TIMEOUT_DEFAULT;
    _path[0] = '\0';
    _protocol[0] = '\0';
}

WSClient::~WSClient()
{
    stop();
    if(_buffer!=NULL)
        delete[] _buffer;
    if(_client != NULL && _clientNew)
        delete _client;
    _client = NULL;
    _clientNew = 0;
}

int WSClient::allocBuffer(){
    if(_buffer==NULL){
        _buffer = new uint8_t[WS_RECV_BUFFER_SIZE];
        if(!_buffer){
            log_e("ws malloc failed %d", _buffer);
            return 0;
        }
    }

    _bufferHead = 0;
    _bufferTail = 0;
    _bufferUse = 0;
    return 1;
}

int WSClient::setClient(Client* client)
{
    stop();
    if(_client != NULL && _clientNew){
        delete _client;
        _clientNew = 0;
    }
    _client = client;
    return 1;
}

int WSClient::setPath(const char* path)
{
    if(strlen(path) > sizeof(_path)-1)
        return 0;
    
    strncpy(_path, path, sizeof(_path));
    return 1;
}

int WSClient::setProtocol(const char* subprotocol)
{
    strncpy(_protocol, subprotocol, sizeof(_protocol));
    return 1;
}

int WSClient::connect(IPAddress ip, uint16_t port)
{
    const char* host = ip.toString().c_str();
    return connect(host, port);    
}

int WSClient::connect(const char *host, uint16_t port)
{
    if(_client==NULL){
        // log_d("creating new tcp client");
        // _client = new TCPClient();
        // _clientNew = 1;
        log_e("connect failed, must set a client first");
        return 0;
    }

    if(_status!=WS_DISCONNECTED)
        stop();

    _connstarted = millis();
    if(_timeout < 3000)
    {
        log_w("min ws timeout 3000ms");
        _timeout = 3000;
    }

    int res = 0;
    _status == WS_CONNECTING;
    _client->setTimeout(_timeout);
    log_d("starting ws connection to %s:%d", host, port);
    res = _client->connect(host, port);
    if(!res)
    {
        log_e("client failed to connect");
        stop();
        return 0;
    }

    log_d("client connected %dms", millis() - _connstarted);
    if(!startUpgrade(host, port))
    {
        stop();
        return 0;
    }
    
    _status = WS_UPGRADE_INPROGRESS;
    while ((res = upgradeComplete()) == 0)
    {
        if(millis()-_connstarted > _timeout)
        {
            log_e("failed to upgrade to ws, timeout");
            stop();
            return 0;
        }

        delay(10);
    }
        
    if(res > 0)
        _status = WS_UPGRADE_COMPLETE;
    else
        stop();
        
    return _status == WS_UPGRADE_COMPLETE;
}

int WSClient::connect(const char* url){
    if (url == NULL) 
        return 0;

    char proto[32];
    char host[256];
    int port = 0; 

    sscanf(url, "%31[^:]://%255[^:/]:%d", proto, host, &port);

    if(strcmp(proto, "ws") != 0 && strcmp(proto, "wss") != 0){
        log_e("ws connect failed, url protocol must be ws or wss");
        return 0;
    }

    if (port <= 0)
        port = strcmp(_protocol, "wss") == 0 ? 443 : 80;
    
    return connect(host, port);

}

int WSClient::startUpgrade(const char* host, int port)
{
    if(_client==NULL || !_client->connected())
    {
        log_e("failed to upgrade, client not connected");
        return 0;
    }

    char buff[256];
    const char* p = "/";
    if(_path[0]!='\0')
        p = _path;

    int res = 0;
    if(_protocol[0]=='\0')
        res = snprintf(buff, sizeof(buff), WS_UPGRADE_REQUEST, p, host, port, WS_UPGRADE_KEY, "13");
    else
        res = snprintf(buff, sizeof(buff), WS_UPGRADE_REQUEST_PROTOCOL, p, host, port, _protocol, WS_UPGRADE_KEY, "13");

    if(res+1 > sizeof(buff))
    {
        log_e("failed to upgrade, snprintf() error");
        return 0;
    }
    
    int len = strlen(buff);
    res = _client->write((uint8_t*)buff, len);
    if(res < len)
    {
        log_e("failed to upgrade, client write() %d expected %d", res, len);
        return 0;
    }

    log_d("upgrading to ws");
    _bufferHead = 0;
    _bufferTail = 0;
    _bufferUse = 0;
    // log_d("\n%s", buff);
    return 1;
    
}

int WSClient::upgradeComplete()
{

    if(_client==NULL || !_client->connected()){
        log_e("failed to upgrade, client is not connected");
        return -1;
    }

    if(!_client->available())
        return 0;

    if(_buffer == NULL && !allocBuffer())
        return 0;

    int c = _client->read();
    while (c > -1){

        if(_bufferTail+10 > WS_RECV_BUFFER_SIZE){
            log_e("failed to upgrade, http response too long");
            return -1;
        }

        _buffer[_bufferTail++] = c;
        if(_bufferTail > 5 && _buffer[_bufferTail-4]=='\r' && _buffer[_bufferTail-3]=='\n' 
            && _buffer[_bufferTail-2]=='\r' && _buffer[_bufferTail-1]=='\n'){
            
            String resp = String(_buffer, _bufferTail);
            resp.toLowerCase();
            if(resp.startsWith("http/1.1") && resp.indexOf("upgrade: websocket") > 0){
                
                _bufferHead = 0;
                _bufferTail = 0;
                _bufferUse = 0;

                //TODO keys?
                log_i("upgrade successful %dms", millis()-_connstarted);
                return 1;
            } else {

                log_e("failed to upgraded to ws, invalid response");
                log_v("\n%s", resp.c_str());
                return -1;
            }
        }
        
        c = _client->read();
    }
    
    return 0;
}

size_t WSClient::write(uint8_t b)
{
    return write(&b, 1);
}

size_t WSClient::write(const uint8_t *data, size_t len)
{
    return writeFrame(WS_OPCODE_BIN, (uint8_t*)data, len);
}

const char* getCloseReason(uint16_t code)
{
    if(code == 1000)
        return "CLOSE_NORMAL (Successful operation / regular socket shutdown)";
    else if(code == 1001)
        return "CLOSE_GOING_AWAY (Client is leaving (browser tab closing))";
    else if(code == 1002)
        return "CLOSE_PROTOCOL_ERROR (Endpoint received a malformed frame)";
    else if(code == 1003)
        return "CLOSE_UNSUPPORTED (Endpoint received an unsupported frame (e.g. binary-only endpoint received text frame))";
    else if(code == 1004)
        return "";
    else if(code == 1005)
        return "CLOSED_NO_STATUS (Expected close status, received none)";
    else if(code == 1006)
        return "CLOSE_ABNORMAL (No close code frame has been receieved)";
    else if(code == 1007)
        return "Unsupported payload (Endpoint received inconsistent message (e.g. malformed UTF-8))";
    else if(code == 1008)
        return "Policy violation (Generic code used for situations other than 1003 and 1009)";
    else if(code == 1009)
        return "CLOSE_TOO_LARGE (Endpoint won't process large frame)";
    else if(code == 1010)
        return "Mandatory extension (Client wanted an extension which server did not negotiate)";
    else if(code == 1011)
        return "Server error (Internal server error while operating)";
    else if(code == 1012)
        return "Service restart (Server/service is restarting)";
    else if(code == 1013)
        return "Try again later (Temporary server condition forced blocking client's request)";
    else if(code == 1014)
        return "Bad gateway (Server acting as gateway received an invalid response)";
    else if(code == 1015)
        return "TLS handshake fail (Transport Layer Security handshake failure)";
    else
        return "";
    
}

const char* getOPTypeStr(uint8_t opcode)
{
    if(opcode==WS_OPCODE_CONT)
        return "'cont'";
    else if(opcode==WS_OPCODE_TEXT)
        return "'text'";
    else if(opcode==WS_OPCODE_BIN)
        return "'bin'";
    else if(opcode==WS_OPCODE_CLOSE)
        return "'close'";
    else if(opcode==WS_OPCODE_PING)
        return "'ping'";
    else if(opcode==WS_OPCODE_PONG)
        return "'pong'";
    else
        return "''";
}

int WSClient::processFrame(uint8_t opcode, uint16_t paylen)
{

    if(opcode==WS_OPCODE_CONT)
    {
        log_e("continuation frames not supported");
        stop();
    }
    else if(opcode==WS_OPCODE_CLOSE)
    {
        log_w("server is closing connection");
        if(paylen==2)
        {
            uint8_t buff[4];
            int res = readPayload(buff, sizeof(buff));
            if(res > -1)
            {
                uint16_t errcode = (buff[0] << 8) | buff[1];
                log_w("code %d, reason %s", errcode, getCloseReason(errcode));
            }
        }
        writeFrame(WS_OPCODE_CLOSE, NULL, 0);
        stop();
    }
    else if(opcode==WS_OPCODE_PING)
    {
        uint8_t buff[32];
        if(paylen < sizeof(buff))
        {   
            int res = readPayload(buff, sizeof(buff));
            if(res > -1)
            {
                writeFrame(WS_OPCODE_PONG, buff, res);
                log_d("sent ws ping");
            }
        }
        else
        {
            log_e("invalid ping frame payload len %d", paylen);
            stop();
        }
    }
    else if(opcode==WS_OPCODE_PONG)
    {
        log_d("ws pong received");
    }
    else
    {
        log_e("unknown opcode %d", opcode);
        stop();
    }

    return 1;
}

int WSClient::fill()
{
    if(_client==NULL)
        return 0;

    if(_buffer==NULL && !allocBuffer())
        return 0;

    int count = 0;
    while (true){
        uint8_t opcode = 0;
        uint16_t paylen = 0;
        if(readHeader(&opcode, &paylen))
        {
            if(opcode==WS_OPCODE_TEXT || opcode==WS_OPCODE_BIN){
                
                if(paylen+1 > WS_RECV_BUFFER_SIZE - _bufferUse){
                    log_w("ws next frame payload too large for buffer free space, read() first", WS_RECV_BUFFER_SIZE - _bufferUse);
                    return 0;
                }

                int res = readPayload(_buffer, WS_RECV_BUFFER_SIZE, _bufferTail, WS_RECV_BUFFER_SIZE - _bufferUse);
                if(res > -1){
                    _bufferTail = (_bufferTail+res) % (WS_RECV_BUFFER_SIZE);
                    _bufferUse += res;
                    count++;
                }
            } 

            else processFrame(opcode, paylen);
        } 

        else break;
    }
    
    return count;
}

int WSClient::available()
{
    fill();
    return _bufferUse;
}

int WSClient::read()
{
    uint8_t b = 0;
    int res = read(&b, 1);
    if(res)
        return b;
    return -1;
}

int WSClient::read(uint8_t *data, size_t len)
{
    fill();

    if(_buffer==NULL)
        return 0;
    
    if(len > _bufferUse)
        len = _bufferUse;
    
    for(int i = 0; i < len; i++){
        data[i] = _buffer[_bufferHead];
        _bufferHead = (_bufferHead+1) % (WS_RECV_BUFFER_SIZE);
        _bufferUse--;
    }

    return len;
}

int WSClient::peek()
{
    fill();
    if(_buffer!=NULL && _bufferUse)
        return _buffer[_bufferHead];
    return -1;
}

void WSClient::flush()
{
    log_w("flush() not implemented");
}

void WSClient::stop()
{

    if(_status != WS_DISCONNECTED)
        log_d("cleaning ws session");     

    if(_client!=NULL)
        _client->stop();
    _bufferHead = 0;
    _bufferTail = 0;
    _bufferUse = 0;
    _recvstate = WS_RECV_STATE_EMPTY;
    _status = WS_DISCONNECTED;
    
}

int WSClient::readHeader(uint8_t* opcode, uint16_t* paylen)
{
    if(_status!=WS_UPGRADE_COMPLETE)
        return 0;

    if(_recvstate==WS_RECV_STATE_EMPTY)
    {
        if(_client->available() >= WS_MIN_HEADER_LEN)
        {
            _recvstarted = millis();

            uint8_t b1 = _client->read();
            uint8_t b2 = _client->read();
            _fin = (b1 & B10000000) >> 7;
            _rsv = (b1 & B01110000) >> 4;
            _opcode  = b1 & B00001111;
            _mask  = (b2 & B10000000) >> 7;
            _paylen  = b2 & B01111111;

            if(_fin < 1)
            {
                log_e("frame fragmentation not supported");
                stop();
                return 0;
            }

            if(_rsv > 0)
            {
                log_e("extensions not supported");
                stop();
                return 0;
            }

            if(_paylen > 126)
            {
                log_e("64bit payloads not supported");
                stop();
                return 0;
            }
            
            // log_v("new header (fin %d, rsv %d, opcode %d, mask %d, paylen %d)",  _fin, _rsv, _opcode, _mask, _paylen);
            _recvstate = WS_RECV_STATE_MIN_HEADER;
        }
    }


    if(_recvstate==WS_RECV_STATE_MIN_HEADER)
    {
        uint8_t ext_header_len = (_paylen > 125 ? WS_EXT_PAYLOAD_LEN : 0) + (_mask ? WS_MASK_LEN : 0);
        if(_client->available() >= ext_header_len)
        {
            if(_paylen > 125)
            {
                uint8_t b1 = _client->read();
                uint8_t b2 = _client->read();
                _paylen = (b1 << 8) | b2;
                // log_v("new paylen %d", _paylen);
            }
            
            if(_mask)
            {
                _maskkey[0] = _client->read();
                _maskkey[1] = _client->read();
                _maskkey[2] = _client->read();
                _maskkey[3] = _client->read();
                // log_v("new mask %d %d %d %d", _maskkey[0], _maskkey[1], _maskkey[2], _maskkey[3]);
            }

            if(_paylen > WS_PAYLOAD_MAX_SIZE)
            {
                log_e("payload is too long, not supported (%d bytes)", _paylen);
                stop();
                return 0;
            }

            _recvstate = WS_RECV_STATE_FULL_HEADER;
        }
    }

    if(_recvstate==WS_RECV_STATE_FULL_HEADER)
    {
        *opcode = _opcode;
        *paylen = _paylen;
        return 1;
    }

    return 0;
}

int WSClient::readPayload(uint8_t* dest, uint16_t len)
{
    return readPayload(dest, len, 0, len);
}

int WSClient::readPayload(uint8_t* dest, uint16_t destsize, uint16_t start, uint16_t len)
{
    if(_status!=WS_UPGRADE_COMPLETE)
        return -1;

    if(_recvstate==WS_RECV_STATE_FULL_HEADER)
    {
        if(_paylen > len)
        {
            log_w("dest buffer does not have enough space to store payload");
            return -1;
        }

        if(_client->available() >= _paylen)
        {
            int pos = 0;
            while(pos < _paylen)
            {
                int b = _client->read();
                if(_mask)
                    b = ((uint8_t)b) ^ _maskkey[pos % 4];
                
                dest[(start+pos) % destsize] = b;
                pos++;
            }

            // log_v("new payload %d bytes %d", _paylen, millis()-_recvstarted);
            _recvstate = WS_RECV_STATE_EMPTY;
            return pos;
        }
    }

    return -1;
}

int WSClient::readFrame(uint8_t* opcode, uint8_t* dest, uint16_t len)
{
    uint16_t paylen = 0;
    if(!readHeader(opcode, &paylen))
        return -1;

    return readPayload(dest, len);
}

int buildHeader(uint8_t* dst, uint8_t fin, uint8_t opcode, uint16_t datalen, uint8_t mask[4])
{
    uint8_t headerlen = WS_MIN_HEADER_LEN;
    uint8_t i = 0;

    dst[i] = 0;
    if(fin)
        dst[i] |= _BV(7);

    dst[i++] |= opcode;

    dst[i] = 0;
    if(mask!=NULL)
    {
        headerlen += WS_MASK_LEN;
        dst[i] |= _BV(7);
    }

    if(datalen < 126)
    {
        dst[i++] |= datalen;
    } 
    else if(datalen < 0xFFFF) 
    {
        headerlen += WS_EXT_PAYLOAD_LEN;
        dst[i++] |= 126;
        dst[i++] = ((datalen >> 8) & 0xFF);
        dst[i++] = datalen & 0xFF;
    }
    else 
    {
        log_e("64bit payloads not supported");
        return -1;
    }

    if(mask!=NULL)
    {
        dst[i++] = mask[0];
        dst[i++] = mask[1];
        dst[i++] = mask[2];
        dst[i++] = mask[3];
    }

    return headerlen;
}

int WSClient::writeFrame(uint8_t opcode, uint8_t* payload, uint16_t len)
{
    if(_status!=WS_UPGRADE_COMPLETE) //
        return -1;
    
    uint32_t started = millis();

    uint8_t mask[4] = {1, 2, 3, 4};
    uint8_t header[10];
    int headerlen = buildHeader(header, true, opcode, len, mask); //todo test without mask
    if(headerlen < WS_MIN_HEADER_LEN)
    {
        log_e("failed to build header");
        return -1;
    }

    if(len > WS_PAYLOAD_MAX_SIZE)
    {
        log_e("payload is too long, not supported (%d bytes)", len);
        return -1;
    }

    int res = _client->write(header, headerlen);
    if(res < headerlen)
    {
        log_e("failed to write header, write() %d expected %d", res, headerlen);
        stop();
        return -1;
    }
    
    int sent = 0;
    uint8_t chunk[256];
    while (sent < len)
    {
        int chunklen = len-sent;
        if(chunklen > sizeof(chunk))
            chunklen = sizeof(chunk);
        
        for (int i = 0; i < chunklen; i++)
            chunk[i] = payload[sent+i] ^ mask[i % 4];

        res = _client->write(chunk, chunklen);
        sent+= res;
        if(res < chunklen)
        {
            log_e("failed to write payload, write() %d expected %d", res, chunklen);
            stop();
            return -1;
        }

        if(_timeout > 0 && millis() - started > _timeout)
        {
            log_e("failed to write payload, timeout");
            stop();
            return -1;
        } 
    }

    uint32_t elap = millis() - started;
    // log_v("sent %d bytes frame in %dms", sent+headerlen, elap);
    return sent;
}

uint8_t WSClient::connected()
{
    if(_status == WS_UPGRADE_COMPLETE) //test connection
    {
        if(_client && _client->connected())
            return 1;
        else
            stop();
    }
    
    return 0;
}








