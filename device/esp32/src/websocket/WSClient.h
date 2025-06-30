


#pragma once




#include <Arduino.h>

#define WS_OPCODE_CONT   0
#define WS_OPCODE_TEXT   1
#define WS_OPCODE_BIN    2
#define WS_OPCODE_CLOSE  8
#define WS_OPCODE_PING   9
#define WS_OPCODE_PONG   10


class WSClient : public Client
{

public:

    WSClient(Client* client, const char* path, const char* subprotocol);
    WSClient(Client* client, const char* path);
    WSClient(Client* client);
    WSClient(/* args */);
    ~WSClient();

    int setClient(Client* client);
    int setPath(const char* path);
    int setProtocol(const char* subprotocol);

    int connect(IPAddress ip, uint16_t port);
    int connect(const char* host, uint16_t port);
    int connect(const char* url);
    size_t write(uint8_t b);
    size_t write(const uint8_t* data, size_t len);
    int available();
    int read() ;
    int read(uint8_t* data, size_t len);
    int peek();
    void flush();
    void stop();
    uint8_t connected();
    operator bool() {return connected();};

    int readFrame(uint8_t* opcode, uint8_t* dest, uint16_t len);
    int writeFrame(uint8_t opcode, uint8_t* payload, uint16_t len);

    int allocBuffer();

private:

    Client* _client = NULL;
    int _clientNew = 0;


    char _path[256];
    char _protocol[256];

    uint8_t _fin;
    uint8_t _rsv;
    uint8_t _opcode;
    uint8_t _mask;
    uint8_t _maskkey[4];
    uint16_t _paylen;
    int _status = 0;
    int _recvstate = 0;

    //uint8_t _buffer[1024*2];
    //int _bufferpos = 0;

    uint8_t* _buffer = NULL;
    int _bufferHead = 0;
    int _bufferTail = 0;
    int _bufferUse = 0;

    uint32_t _connstarted = 0;
    uint32_t _recvstarted = 0;

    int startUpgrade(const char* host, int port);
    int upgradeComplete();

    int readHeader(uint8_t* opcode, uint16_t* paylen);
    int readPayload(uint8_t* dest, uint16_t len);
    int readPayload(uint8_t* dest, uint16_t destsize, uint16_t start, uint16_t len);

    int processFrame(uint8_t opcode, uint16_t paylen);
    int fill();

};

