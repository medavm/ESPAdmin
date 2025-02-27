


#include <WiFi.h>
#include <lwip/sockets.h>
#include <errno.h>

#include "TCPClient.h"

TCPClient::TCPClient()
{
    setTimeout(TCPCLIENT_TIMEOUT_DEFAULT);
    _sockfd = -1;
    _host[0] = '\0';
    _state = TCPCLIENT_DISCONNECTED;
}

TCPClient::~TCPClient()
{
    stop();
}

int TCPClient::connectAsync(IPAddress ip, uint16_t port)
{
    if(_sockfd>-1)
        stop();

    _sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (_sockfd < 0) 
    {
        log_e("failed to create socket errno %d", errno);
        stop();
        return 0;
    }

    if(_host[0]=='\0')
        log_d("fd %d connecting to %s:%d", _sockfd, ip.toString().c_str(), port);
    else
        log_d("fd %d connecting to %s:%d", _sockfd, _host, port);

    fcntl( _sockfd, F_SETFL, fcntl( _sockfd, F_GETFL, 0 ) | O_NONBLOCK );
    uint32_t ip_addr = ip;
    struct sockaddr_in serveraddr;
    memset((char *) &serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    memcpy((void *)&serveraddr.sin_addr.s_addr, (const void *)(&ip_addr), 4);
    serveraddr.sin_port = htons(port);

    int res = lwip_connect(_sockfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
    if (res < 0 && errno != EINPROGRESS) 
    {
        log_e("fd %d lwip_connect() errno: %d, \"%s\"", _sockfd, errno, strerror(errno));
        stop();
        return 0;
    }

    _conn_started = millis();
    _state = TCPCLIENT_CONNECTING;
    return 1;
}

int TCPClient::connectAsync(const char* host, uint16_t port)
{
    if(host==NULL)
    {
        log_e("cannot connect to null host");
        return 0;
    }

    IPAddress srv((uint32_t)0);
    if(!WiFiGenericClass::hostByName(host, srv)){ //todo first query blocks?
        return 0;
    }

    strncpy(_host, host, sizeof(_host));
    return connectAsync(srv, port);
}

int TCPClient::connect(IPAddress ip, uint16_t port)
{

    if(!connectAsync(ip, port))
        return 0;

    while (true)
    {
        int stat = status(); 
        if(stat == TCPCLIENT_CONNECTED)
        {
            log_d("fd %d connected %dms", _sockfd, millis()-_conn_started);
            return 1;
        }
        else if(stat == TCPCLIENT_CONNECTING)
        {
            if(millis()-_conn_started < _timeout)
            {
                //log_v("fd %d waiting for connection", _sockfd);
                delay(100);
            }
            else
            {
                log_e("fd %d connection failed due to timeout", _sockfd);
                stop();
                return 0;
            }
        }
        else
        {
            log_e("fd %d connection failed", _sockfd);
            return 0;
        }
    }
        
}

int TCPClient::connect(const char* host, uint16_t port)
{
    if(host==NULL)
    {
        log_e("cant connect to null host");
        return 0;
    }

    IPAddress srv((uint32_t)0);
    if(!WiFiGenericClass::hostByName(host, srv)){
        return 0;
    }

    strncpy(_host, host, sizeof(_host));
    return connect(srv, port);
}

int TCPClient::status()
{
    if(_state == TCPCLIENT_CONNECTED)
    {
        int res = recv(_sockfd, NULL, 0, MSG_DONTWAIT); //detect conn lost?
        if (res <= 0)
        {
            int err = errno;
            if(err != EWOULDBLOCK && err!=ENOENT)
            {
                log_e("fd %d recv() errno %d, \"%s\"", _sockfd, err, strerror(err));
                stop();
            }
        }
    }
    else if(_state == TCPCLIENT_CONNECTING)
    {
        fd_set fdset;
        struct timeval tv;
        FD_ZERO(&fdset);
        FD_SET(_sockfd, &fdset);
        tv.tv_sec = 0;
        tv.tv_usec = 0;
        
        int res = select(_sockfd + 1, nullptr, &fdset, nullptr, &tv);
        if (res < 0) 
        {
            log_e("fd %d select() errno %d, \"%s\"", _sockfd, errno, strerror(errno));
            stop();
        }
        else if (res == 0) 
        {
            if(_timeout > 0 && millis()-_conn_started > _timeout)
            {
                log_e("fd %d select() 0, connection timeout %dms", _sockfd, millis()-_conn_started);
                stop();
            }
        }
        else 
        {
            int sockerr;
            socklen_t len = (socklen_t)sizeof(int);
            int res2 = getsockopt(_sockfd, SOL_SOCKET, SO_ERROR, &sockerr, &len);

            if (res2 < 0) 
            {
                log_e("fd %d getsockopt() errno %d, \"%s\"", _sockfd, errno, strerror(errno));
                stop();
            }

            if (sockerr != 0) 
            {
                log_e("fd %d socket error errno %d, \"%s\"", _sockfd, sockerr, strerror(sockerr));
                stop();
            }

            log_v("fd %d select() %d, connection ready %dms", _sockfd, res, millis()-_conn_started);
            if(_state == TCPCLIENT_CONNECTING)
                _state = TCPCLIENT_CONNECTED;
        }
    }

    return _state;
}

size_t TCPClient::write(uint8_t b)
{
    return write(&b, 1);
}

size_t TCPClient::write(const uint8_t *buf, size_t size)
{
    return write(buf, size, false);
}

size_t TCPClient::write(const uint8_t *buf, size_t size,  bool fulfill_size)
{
    uint32_t start = millis();
    size_t count = 0;

    if(status() != TCPCLIENT_CONNECTED)
        return 0;
    
    while (_sockfd > -1 && count < size)
    {

        int res = send(_sockfd, (void*) (buf+count), size-count, MSG_DONTWAIT);
        if(res > 0)
        {
            if(res < size-count)
                log_w("fd %d send() %d expected %d", _sockfd, res, size-count);
            count+= res;
        }
        else if(res == 0)
        {
            log_w("fd %d send() 0 error?", _sockfd);
        }
        else
        {
            int err = errno;
            if(err == EAGAIN || err == EWOULDBLOCK)
            {
                if(!fulfill_size && count>0)
                    break;

                if(_timeout==0 || millis()-start > _timeout)
                {
                    log_w("fd %d send() EWOULDBLOCK, aborting write due to timeout", _sockfd);
                    break;
                }
                else
                {
                    // log_v("fd %d send() EWOULDBLOCK, trying again in 10ms", _sockfd);
                    delay(10);
                }
            }      
            else
            {
                log_e("send() fd %d errno %d, \"%s\"", _sockfd, err, strerror(err));
                stop();
                return count;
            }
        }
    }

    // uint32_t elap = millis()-start;
    // log_v("fd %d sent %d/%d bytes %dms", _sockfd, count, size, elap);
    return count;
}

int TCPClient::available()
{
    if(_sockfd < 0)
        return 0;

    if(status() != TCPCLIENT_CONNECTED)
        return 0;
    
    int count = 0;
    int res = lwip_ioctl(_sockfd, FIONREAD, &count);
    if(res < 0) 
    {
        log_e("fd %d lwip_ioctl() error %d", _sockfd, res);
        stop();
        return 0;
    }

    return _rxb_tail - _rxb_head + count;
}

int TCPClient::fillRxBuffer()
{
    if(_sockfd < 0)
        return -1;

    if(status() != TCPCLIENT_CONNECTED)
        return -2;

    _rxb_head = 0;
    _rxb_tail = 0;

    uint32_t start = millis();
    int res = recv(_sockfd, _rxb, TCPCLIENT_RXB_LEN, MSG_DONTWAIT);
    
    if(res > 0)
    {
        uint32_t elap = millis() - start;
        // log_v("fd %d recv() loaded %d bytes %dms", _sockfd, res, elap);
        _rxb_tail = res;
        return res;
    }
    else if(res == 0)
    {
        log_e("fd %d recv() 0", _sockfd);
        stop();
        return -3;
    }
    else
    {
        int err = errno;
        if(err == EAGAIN || err == EWOULDBLOCK)
        {
            // log_v("fd %d recv() EWOULDBLOCK", _sockfd);
            return 0;
        }
        else
        {
            log_e("fd %d recv() errno %d, \"%s\"", _sockfd, err, strerror(err));
            stop();
            return -4;
        }
    }

}

int TCPClient::read()
{
    uint8_t n = 0;
    if(_rxb_tail - _rxb_head > 0 || fillRxBuffer()>0) //if this read(&n, 1) wont block
    {
        read(&n, 1);
        return n;
    }
        
    return -1;
}

int TCPClient::read(uint8_t *buf, size_t size)
{
    return read(buf, size, false);
}

int TCPClient::read(uint8_t *buf, size_t size, bool fulfill_size)
{
    uint32_t start = millis();

    size_t count = _rxb_tail - _rxb_head;
    if(count >= size)
    {
        memcpy(buf, _rxb + _rxb_head, size);
        _rxb_head += size;
        count = size;
    }
    else if(count > 0)
    {
        memcpy(buf, _rxb + _rxb_head, count); //empty buffer into dest
    }

    while (_sockfd > -1 && count < size)
    {
        int res = fillRxBuffer();
        if(res >= size-count)
        {
            memcpy(buf+count, _rxb, size-count);
            _rxb_head = size-count;
            count += size-count;
        }
        else if(res>0) //
        {
            memcpy(buf+count, _rxb, res);
            _rxb_head = res;
            count += res;
        }
        else if(res==0) 
        {
            if(!fulfill_size && count>0)
                break;

            if(_timeout==0 || millis()-start > _timeout)
            {
                log_w("fd %d fillRxBuffer() 0 aborting read due to timeout", _sockfd);
                break;
            }
            else
            {
                // log_v("fd %d fillRxBuffer() 0, trying again in 10ms", _sockfd);
                delay(10);
            }
        }
        else //fillRxBuffer() error
        {
            break;
        }
    }

    // uint32_t elap = millis()-start;
    // log_v("fd %d read %d bytes %dms", _sockfd, count, elap);
    return count;
}

int TCPClient::peek()
{
    if(_rxb_tail-_rxb_head > 0 || fillRxBuffer()>0)
    {
        return _rxb[_rxb_head];
    }

    return -1;
}

void TCPClient::flush()
{
    log_w("fd %d flush() not implemented", _sockfd);
}

void TCPClient::stop()
{
    if(_sockfd > -1)
    {
        log_d("fd %d cleaning tcp session", _sockfd);
        close(_sockfd);
        _sockfd = -1;
        _rxb_head = 0;
        _rxb_tail = 0;
        _host[0] = '\0';
    }
    
    _state = TCPCLIENT_DISCONNECTED;
}

uint8_t TCPClient::connected()
{
    return status() == TCPCLIENT_CONNECTED;
}


















