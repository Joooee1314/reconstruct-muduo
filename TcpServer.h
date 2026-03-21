#pragma once

#include "noncopyable.h"
#include "EventLoop.h"
#include "Acceptor.h"
#include "InetAddress.h"
#include "EventLoopThreadPool.h"
#include "Callbacks.h"
#include "TcpConnection.h"
#include "TimerWheel.h"
#include "Buffer.h"

#include <functional>
#include <string>
#include <memory>
#include <atomic>
#include <unordered_map>

class TcpServer:noncopyable{
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    enum Option{
        kNoReusePort,
        kReusePort,
    };

    TcpServer(EventLoop* loop,
                const InetAddress &listenAddr,
                const std::string nameArg,
                Option option=kNoReusePort);
    ~TcpServer();

    void setThreadInitCallback(const ThreadInitCallback &cb){ threadInitCallback_ = std::move(cb); }
    void setConnectionCallback(const ConnectionCallback &cb){ connectionCallback_ = std::move(cb); }
    void setMessageCallback(const MessageCallback &cb){ messageCallback_ = std::move(cb); }
    void setWriteCompleteCallback(const WriteCompleteCallback &cb){ writeCompleteCallback_ = std::move(cb); }
    //设置底层subloop的个数
    void setThreadNum(int numThreads);
    
    //开启服务器监听
    void start();
private:
    void newConnection(int sockfd,const InetAddress &peerAddr);
    void removeConnection(const TcpConnectionPtr &conn);
    void removeConnectionInLoop(const TcpConnectionPtr &conn);

    using ConnectionMap = std::unordered_map<std::string,TcpConnectionPtr>;

    EventLoop* loop_; //baseloop用户自己定义的loop
    const std::string ipPort_;
    const std::string name_;

    std::unique_ptr<Acceptor> acceptor_; //运行在mainloop，任务就是监听新连接事件

    std::shared_ptr<EventLoopThreadPool>threadPool_; // one loop per thread

    ConnectionCallback connectionCallback_; //有新连接时的回调
    MessageCallback messageCallback_; //有读写消息时的回调
    WriteCompleteCallback writeCompleteCallback_; //消息发送完成以后的回调

    ThreadInitCallback threadInitCallback_; //loop线程初始化的回调
    std::atomic_int started_;

    int nextConnId_;
    ConnectionMap connections_;

    TimerWheel timerWheel_;  // 心跳超时检测时间轮
};