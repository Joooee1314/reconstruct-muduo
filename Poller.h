#pragma once

#include "noncopyable.h"
#include "Timestamp.h"

#include <vector>
#include <unordered_map>

class Channel;
class EventLoop;

// muduo库中多路事件分发器的核心io复用模块
class Poller:noncopyable{
public:
    using ChannelList=std::vector<Channel*>;

    Poller(EventLoop *Loop);
    virtual ~Poller()=default;

    //给所有IO复用保留统一可重写接口
    virtual Timestamp poll(int timeoutMs,ChannelList* activeChannels)=0;
    virtual void updateChannel(Channel* channel)=0;
    virtual void removeChannle(Channel* channel)=0;

    //判断参数channel是否在当前poller中
    bool hasChannel(Channel* channel)const;

    //EventLoop可以通过该接口获取默认的IO复用具体实现
    static Poller* newDefaultPoller(EventLoop* Loop);
protected:
    //map的key:sockfd  value:sockfd所属的channel
    using ChannelMap = std::unordered_map<int,Channel*>;
    ChannelMap channels_;
private:
    EventLoop *ownerLoop_;
};