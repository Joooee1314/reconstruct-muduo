#include "AsyncLogging.h"

#include <chrono>

AsyncLogging::AsyncLogging(const std::string& filename)
    : filename_(filename), running_(false),
      currentBuffer_(std::make_unique<Buffer>()),
      nextBuffer_(std::make_unique<Buffer>()) {}

AsyncLogging::~AsyncLogging() {
    if (running_) stop();
}

void AsyncLogging::append(const char* logline, size_t len) {
    //上锁避免日志输出混乱
    std::lock_guard<std::mutex> lock(mutex_);
    if (currentBuffer_->avail() > len) {
        currentBuffer_->append(logline, len);
    } else {
        /**
         * buffers_类似于传输带，currentBuffer_与nextBuffer_是装日志的桶，一个当前桶，一个备用桶
         * 当前桶的可写空间小于日志长度时，说明这个内存块写满了可以放到传送带里了
         * 则把已有日志放入传输带里，转移当前桶的所有权，使其变为 nullptr，避免内存拷贝
         */
        buffers_.push_back(std::move(currentBuffer_));
        if (nextBuffer_) {
            //若是有备用桶，转移当前桶的所有权，使其变为 nullptr，避免内存拷贝
            currentBuffer_ = std::move(nextBuffer_);
        } else {
            //没有备用桶，当前桶直接创建指针
            currentBuffer_ = std::make_unique<Buffer>();
        }
        currentBuffer_->append(logline, len);
        //唤醒后端消费者线程，提醒可以输出日志到log文件里
        cond_.notify_one();
    }
}

void AsyncLogging::start() {
    running_ = true;
    thread_ = std::thread(&AsyncLogging::threadFunc, this);
}

void AsyncLogging::stop() {
    running_ = false;
    cond_.notify_one();
    if (thread_.joinable()) thread_.join();
}

void AsyncLogging::threadFunc() {
    //先创建两个空桶，等前端生产者交付数据后，可以直接把buffer1给当前桶，buffer2给备用桶
    BufferPtr buffer1 = std::make_unique<Buffer>();
    BufferPtr buffer2 = std::make_unique<Buffer>();
    std::vector<BufferPtr> buffersToWrite;
    FILE* fp = fopen(filename_.c_str(), "a");
    if (!fp) {
        fprintf(stderr, "打开日志失败！文件名: %s, 错误原因: %s\n", 
                filename_.c_str(), strerror(errno));
        exit(1); 
    }
    while (running_) {
        {
            //上锁避免日志输出混乱
            std::unique_lock<std::mutex> lock(mutex_);
            /**
             * 如果传送带中没数据，那就条件变量等待三秒或者被生产者唤醒
             * 此时即使虚假唤醒也没关系，下面会强制换桶把当前桶数据放入buffers中
             */ 
            if (buffers_.empty()) {
                cond_.wait_for(lock, std::chrono::seconds(3));
            }
            buffers_.push_back(std::move(currentBuffer_));
            currentBuffer_ = std::move(buffer1);
            if (!nextBuffer_) {
                nextBuffer_ = std::move(buffer2);
            }
            //大大减小了锁的粒度和性能开销，在锁内只做指针交换，磁盘 I/O 操作完全在锁外进行
            buffersToWrite.swap(buffers_);
        }
        for (const auto& buffer : buffersToWrite) {
            fwrite(buffer->data(), 1, buffer->length(), fp);
        }
        fflush(fp);
        //避免日志风暴导致buffersToWrite占用内存巨大，resize操作节省内存空间，并且创建两个桶来分配给buffer1和2
        if (buffersToWrite.size() > 2) {
            buffersToWrite.resize(2);
        }
        //假如buffer1空的话，那就从buffersToWrite拿来一个内存块重置，之后便可直接给currentBuufer_
        if (!buffer1) {
            buffer1 = std::move(buffersToWrite.back());
            buffer1->reset();
            buffersToWrite.pop_back();
        }
        if (!buffer2) {
            buffer2 = std::move(buffersToWrite.back());
            buffer2->reset();
            buffersToWrite.pop_back();
        }
        buffersToWrite.clear();
    }

    // 停止逻辑 (Final Flush): 退出循环后的最后一次扫尾
    {
        std::unique_lock<std::mutex> lock(mutex_);
        // 将最后还没满的 currentBuffer_ 强制放入传送带
        if (currentBuffer_ && currentBuffer_->length() > 0) {
            buffers_.push_back(std::move(currentBuffer_));
        }
        buffersToWrite.swap(buffers_);
    }

    // 将最后这点残余数据也写进文件
    for (const auto& buffer : buffersToWrite) {
        fwrite(buffer->data(), 1, buffer->length(), fp);
    }
    fflush(fp);
    fclose(fp);
}
