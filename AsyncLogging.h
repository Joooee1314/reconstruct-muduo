#pragma once

#include "noncopyable.h"

#include <vector>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>



/**
 * data_ (起始地址)
    ↓
    ┌─────────────────────────────────────┐
    │  已写入数据  │    剩余可用空间        │
    │  [data_, cur_)    [cur_, end())     │
    └─────────────────────────────────────┘
    ↑              ↑                     ↑
   data_           cur_                 end()
   (0x1000)      (0x1008)              (0x401000)
   
   kBufferSize = 4MB = 0x400000

   预分配内存的方式避免频繁调用new/delete产生的内存碎片，极其高效
 */

class FixedBuffer {
public:
    static const size_t kBufferSize = 4 * 1024 * 1024; // 4MB
    FixedBuffer() : cur_(data_) {}
    size_t avail() const { return end() - cur_; }
    void append(const char* buf, size_t len) {
        if (avail() > len) {
            memcpy(cur_, buf, len);
            cur_ += len;
        }
    }
    const char* data() const { return data_; }
    size_t length() const { return cur_ - data_; }
    void reset() { cur_ = data_; }
private:
    const char* end() const { return data_ + sizeof(data_); }
    char data_[kBufferSize];
    char* cur_;
};

/**
 * @brief 异步日志模块 (后端)
 * * 核心原理：双缓冲机制 (Double Buffering)
 * 1. 前端线程 (多个生产者) 调用 append 将日志写入 currentBuffer_ (加锁)。
 * 2. 当 currentBuffer_ 写满或到达定时时间 (3s)，后端线程 (单个消费者) 将其交换 (swap) 到本地。
 * 3. 后端线程在锁外执行磁盘写入操作 (fwrite/fflush)，保证前端不被磁盘 I/O 阻塞。
 * * @note 线程安全：append() 方法通过 std::mutex 保证多线程安全。
 * * @usage 使用说明：
 * * 1. 包含头文件：
 * #include <mymuduo/Logger.h>
 * #include <mymuduo/AsyncLogging.h> 
 * * 2. 在 main 函数或全局作用域创建一个 AsyncLogging 对象：
 * AsyncLogging log("Server.log");
 * * 3. 定义一个全局回调函数，并将其注入到 Logger 模块：
 * void asyncOutput(const char* msg, int len) {
 * log.append(msg, len);
 * }
 * * 4. 初始化并启动：
 * Logger::setOutput(asyncOutput); // 注入回调
 * log.start();                   // 开启后端落盘线程
 */

class AsyncLogging : noncopyable{
public:
    AsyncLogging(const std::string& filename);
    ~AsyncLogging();
    void append(const char* logline, size_t len);
    void start();
    void stop();
private:
    void threadFunc();
    using Buffer = FixedBuffer;
    using BufferPtr = std::unique_ptr<Buffer>;
    std::string filename_;
    std::atomic<bool> running_;
    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable cond_;
    BufferPtr currentBuffer_;
    BufferPtr nextBuffer_;
    std::vector<BufferPtr> buffers_;
};
