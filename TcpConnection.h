#pragma once

#include "noncopyable.h"
#include "InetAddress.h"
#include "Callbacks.h"
#include "Buffer.h"
#include "Timestamp.h"

#include <memory>
#include <string>
#include <atomic>

class Channel;
class EventLoop;
class Socket;

/**
 * TcpServer =>Acceptor =>新用户连接拿到connfd =>打包成TcpConnection 设置回调
 *  => Channel => Poller监听 => Channel回调操作
 */
class TcpConnection : noncopyable , public std::enable_shared_from_this<TcpConnection>{
public:

    TcpConnection(EventLoop* loop ,
                const std::string &nameArg,
                int sockfd,
                const InetAddress& localAddr,
                const InetAddress& peerAddr);
    ~TcpConnection();

    EventLoop* getLoop() const {return loop_;}
    const std::string& name() const {return name_;}
    const InetAddress& localAddress() const {return localAddr_;}
    const InetAddress& peerAddress() const { return peerAddr_;}

    bool connected() const {return state_ == kConnected;}

    //发送数据
    void send(const std::string &buf);
    //关闭连接
    void shutdown();

    void setConnectionCallback(const ConnectionCallback &cb){ connectionCallback_ = std::move(cb); }
    void setMessageCallback(const MessageCallback &cb){ messageCallback_ = std::move(cb); }
    void setWriteCompleteCallback(const WriteCompleteCallback &cb){ writeCompleteCallback_ = std::move(cb); }
    void setHighWaterMarkCallback(const HighWaterMarkCallback &cb){ highWaterMarkCallback_ = std::move(cb); }
    void setCloseCallback(const CloseCallback &cb){ closeCallback_ = std::move(cb); }
    
    //建立连接
    void connectEstablished();
    //销毁链接
    void connectDestroyed();
private:
    enum StateE{kDisconnected, kConnecting, kConnected, kDisconnecting};
    void setState(StateE state){state_=state;}

    void handleRead(Timestamp receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();

    void sendInLoop(const void* data,size_t len);
    void shutdownInLoop();

    EventLoop* loop_; //绝对不是baseloop TcpConnection的channel都是给subloopd的
    const std::string name_;
    std::atomic_int state_;
    bool reading_;

    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;

    const InetAddress localAddr_;
    const InetAddress peerAddr_;

    ConnectionCallback connectionCallback_; //有新连接时的回调
    MessageCallback messageCallback_; //有读写消息时的回调
    WriteCompleteCallback writeCompleteCallback_; //消息发送完成以后的回调
    HighWaterMarkCallback highWaterMarkCallback_;
    CloseCallback closeCallback_;

    size_t highWaterMark_;

    Buffer inputBuffer_; // 接收数据缓冲区
    Buffer outputBuffer_;// 发送数据缓冲区

};