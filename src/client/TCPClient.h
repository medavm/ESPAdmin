



#pragma once

#include <Arduino.h>

#define TCPCLIENT_TIMEOUT_DEFAULT 1000l*5
#define TCPCLIENT_RXB_LEN 1024*2

#define TCPCLIENT_CONNECTING 0
#define TCPCLIENT_CONNECTED 1
#define TCPCLIENT_DISCONNECTED 2

class TCPClient : public Client
{
public:

    TCPClient();
    ~TCPClient();

    int connect(IPAddress ip, uint16_t port);
    int connect(const char *host, uint16_t port);

    int connectAsync(IPAddress ip, uint16_t port);
    int connectAsync(const char* host, uint16_t port);

    /**
     * if cant write immediately will block until timeout
     * 
     */
    size_t write(uint8_t);

    /**
     * if cant write immediately will block until timeout
     * 
     */
    size_t write(const uint8_t *buf, size_t size);

    int available();

    /**
     *  never blocks, -1 if no bytes available
     * 
     */
    int read();

    /**
     * if cant read immediately will block until timeout
     * 
     */
    int read(uint8_t *buf, size_t size);

    int peek();

    void flush();

    void stop();

    uint8_t connected();

    operator bool() {return connected();}

    /**
     * 'fulfill_size' true will block until is able to fully write 'size' amount of bytes
     * 'fulfill_size' false will return once it can write at least 1 byte
     * 
     */
    size_t write(const uint8_t *buf, size_t size, bool fulfill_size);
    

    /**
     * 'fulfill_size' true will block until it has received 'size' amount of bytes
     * 'fulfill_size' false will return when 1 or more bytes available
     * 
     */
    int read(uint8_t *dst, size_t size, bool fulfill_size);

    /**
     * TCPCLIENT_CONNECTING -> waiting to establish connection
     * TCPCLIENT_CONNECTED -> connected
     * TCPCLIENT_DISCONNECTED -> disconnected
     * 
     */
    int status();


private:
    //

    int _sockfd = -1;
    int _state = TCPCLIENT_DISCONNECTED;
    char _host[256];

    uint8_t _rxb[TCPCLIENT_RXB_LEN];
    size_t _rxb_head = 0;
    size_t _rxb_tail = 0;
    
    uint32_t _conn_started = 0;

    int fillRxBuffer();
    
};