#pragma once

#include <string>

#include "noncopyable.h"

#define LOG_INFO(LogmsgFormat,...)\
    do\
    {\
        Logger &logger = Logger::getinstance();\
        logger.setloglevel(INFO);\
        char buf[1024];\
        snprintf(buf,1024,LogmsgFormat, ##__VA_ARGS__);\
        logger.log(buf);\
    }while(0)

#define LOG_ERROR(LogmsgFormat,...)\
    do\
    {\
        Logger &logger = Logger::getinstance();\
        logger.setloglevel(ERROR);\
        char buf[1024];\
        snprintf(buf,1024,LogmsgFormat, ##__VA_ARGS__);\
        logger.log(buf);\
    }while(0)

#define LOG_FATAL(LogmsgFormat,...)\
    do\
    {\
        Logger &logger = Logger::getinstance();\
        logger.setloglevel(FATAL);\
        char buf[1024];\
        snprintf(buf,1024,LogmsgFormat, ##__VA_ARGS__);\
        logger.log(buf);\
        exit(-1);\
    }while(0)

#ifdef DEBUGON
#define LOG_DEBUG(LogmsgFormat,...)\
    do\
    {\
        Logger &logger = Logger::getinstance();\
        logger.setloglevel(DEBUG);\
        char buf[1024];\
        snprintf(buf,1024,LogmsgFormat, ##__VA_ARGS__);\
        logger.log(buf);\
    }while(0)
#else
    #define LOG_DEBUG(LogmsgFormat,...)
#endif


//定义日志的级别
enum LogLevel
{
    INFO, //正常信息
    ERROR, //错误信息
    FATAL, //core信息
    DEBUG //调试信息
};

class Logger:noncopyable{
public:
    //获取日志唯一的实例对象
    static Logger& getinstance();
    //设置日志级别
    void setloglevel(int level);
    //写日志
    void log(std::string Msg);
private:
    int loglevel_;
    Logger(){};
};