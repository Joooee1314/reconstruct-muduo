#pragma once

#include "Timestamp.h"
#include "noncopyable.h"

#include <functional>
#include <memory>

class EventLoop;

/**
 * Channel理解为通道，封装了sockfd和感兴趣的event,如EPOLLIN,EPOLLOUT
 * 还绑定了Poller返回的具体事件
 */

class Channel:noncopyable{
public:
    using EventCallback=std::function<void()>;
    using ReadEventCallback=std::function<void(Timestamp)>;

    Channel(EventLoop* Loop,int fd);
    ~Channel();

    //fd得到poller通知后处理事件
    void handleEvent(Timestamp receiveTime);

    //设置回调函数对象
    void setReadCallback(ReadEventCallback cb){ readCallback_=std::move(cb); }
    void setWriteCallback(EventCallback cb){ writeCallback_=std::move(cb); }
    void setCloseCallback(EventCallback cb){ closeCallback_=std::move(cb); }
    void setErrorCallback(EventCallback cb){ errorCallback_=std::move(cb); }

    //防止当channel被手动remove掉，channel还在执行回调操作
    void tie(const std::shared_ptr<void>&);

    int fd() const{return fd_;}
    int events() const{return events_;}
    void set_revents(int revt){revents_=revt;}

    //设置fd相应的事件状态
    void enableReading() {events_ |= KReadEvent;update();}
    void disableReading() {events_ &= ~KReadEvent;update();}
    void enableWriting() {events_ |= KWriteEvent;update();}
    void disableWriting() {events_ &= ~KWriteEvent;update();}
    void disableAll() {events_ = KNoneEvent;update();}

    //返回fd当前状态
    bool isNoneEvent() const{ return events_ == KNoneEvent ;}
    bool isReading() const{ return events_ == KReadEvent ;}
    bool isWriting() const{ return events_ == KWriteEvent ;}

    int index(){return index_;}
    void set_index(int idx){index_=idx;}
    
    EventLoop* ownerLoop(){return loop_;}
    void remove();

private:

    void update();
    void handleEventWithGuard(Timestamp receiveTime);

    static const int KNoneEvent;
    static const int KReadEvent;
    static const int KWriteEvent;

    EventLoop* loop_; //事件循环
    const int fd_; //fd,Poller监听的对象
    int events_; //注册fd感兴趣的事件
    int revents_; //Poller返回具体发生的事件
    int index_;

    //可以跨线程监听资源是否释放成功
    std::weak_ptr<void>tie_;
    bool tied_;

    //因为channle里有能具体获得fd事件的revents 所以由channle负责调用具体事件的回调函数
    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
};