



#pragma once


#include <Arduino.h>



enum LogType{ //todo add notice
    CRITICAL = 0,
    ERROR = 1,
    WARNING = 2,
    INFO = 3,
    DEBUG = 4,
    VERBOSE = 5
};

class ESPAdmin
{

public:

    static int begin(const char* host, int port, const char* deviceKey);

    static int setUpdateClient(Client* client);
    static int setWSClient(Client* client);

    static int connect(int reconnectInterval = 5000);
    static int connected();
    static int status();

    static int updating();

    static int disconnect();

    
    static int log(LogType type, const char* format, ...);
    static uint32_t unixtime();


private:

};


#define ESPAD_LOG(type, letter, format, ...) ESPAdmin::log(type, "[%6u][" #letter "][%s:%u] %s(): " format, (unsigned long) (esp_timer_get_time() / 1000ULL), pathToFileName(__FILE__), __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define ESPAD_LOG_SHORT(type, format, ...) ESPAdmin::log(type, "[%s:%u] %s(): " format, pathToFileName(__FILE__), __LINE__, __FUNCTION__, ##__VA_ARGS__)

#define ESPAD_LOG_CRITICAL(format, ...) ESPAD_LOG_SHORT(LogType::CRITICAL, format, ##__VA_ARGS__)
#define ESPAD_LOG_ERROR(format, ...)    ESPAD_LOG_SHORT(LogType::ERROR, format, ##__VA_ARGS__)
#define ESPAD_LOG_WARN(format, ...)     ESPAD_LOG_SHORT(LogType::WARNING, format, ##__VA_ARGS__)
#define ESPAD_LOG_INFO(format, ...)     ESPAD_LOG_SHORT(LogType::INFO, format, ##__VA_ARGS__)
#define ESPAD_LOG_DEBUG(format, ...)    ESPAD_LOG_SHORT(LogType::DEBUG, format, ##__VA_ARGS__)
#define ESPAD_LOG_VERBOSE(format, ...)  ESPAD_LOG_SHORT(LogType::VERBOSE, format, ##__VA_ARGS__)

