#include "Logger.h"
#include "Timestamp.h"

#include <iostream>
#include <cstdio>

//默认输出函数，往屏幕打印
void defaultOutput(const char* msg, int len){
    fwrite(msg, 1, len, stdout);

}

//默认刷新函数
void defaultFlush(){
    fflush(stdout);
}

// 初始化静态函数指针，默认指向屏幕输出
Logger::OutputFunc Logger::g_output = defaultOutput;
Logger::FlushFunc Logger::g_flush = defaultFlush;

//获取日志唯一的实例对象
Logger& Logger::getinstance(){
    static Logger logger;
    return logger;
}

//设置日志级别
void Logger::setloglevel(int level){
    loglevel_=level;
}

// 静态方法设置回调：给外部（如 main 函数）接入 AsyncLogging 使用
void Logger::setOutput(OutputFunc out) {
    g_output = out;
}

void Logger::setFlush(FlushFunc flush) {
    g_flush = flush;
}

//写日志 [级别信息] time : Msg
void Logger::log(std::string Msg){
    std::string levelStr;
    switch (loglevel_)
    {
    case INFO:
        levelStr = "[INFO]";
        break;
    case ERROR:
        levelStr = "[ERROR]";
        break;
    case FATAL:
        levelStr = "[FATAL]";
        break;
    case DEBUG:
        levelStr = "[DEBUG]";
        break;
    default:
        break;
    }
    // 1. 组装整行日志字符串
    // 格式：[LEVEL] time : Msg \n
    std::string finalMsg = levelStr + " " + Timestamp::now().toString() + " : " + Msg;
    
    // 2. 调用全局回调函数
    // 如果你在 main 里设置了异步日志，这里就会自动调用 AsyncLogging::append
    // 如果没设置，就走默认的 defaultOutput (fwrite 到屏幕)
    g_output(finalMsg.c_str(), finalMsg.size());
    g_flush();
}