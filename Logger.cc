#include "Logger.h"

#include <iostream>

//获取日志唯一的实例对象
Logger& Logger::getinstance(){
    static Logger logger;
    return logger;
}

//设置日志级别
void Logger::setloglevel(int level){
    loglevel_=level;
}

//写日志 [级别信息] time : Msg
void Logger::log(std::string Msg){
    switch (loglevel_)
    {
    case INFO:
        std::cout<<"[INFO]";
        break;
    case ERROR:
        std::cout<<"[ERROR]";
        break;
    case FATAL:
        std::cout<<"[FATAL]";
        break;
    case DEBUG:
        std::cout<<"[DEBUG]";
        break;
    default:
        break;
    }
    std::cout<<"time"<<" : "<<Msg<<std::endl;
}