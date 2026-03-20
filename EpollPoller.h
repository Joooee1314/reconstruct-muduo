#pragma once

#include "Poller.h"
#include "Timestamp.h"
#include "Channel.h"

#include <vector>
#include <sys/epoll.h>
#include <unistd.h>

class Channel;

class EpollPoller:public Poller{
public:
    EpollPoller(EventLoop *Loop);
    ~EpollPoller()override;

    //重写基类的抽象方法
    Timestamp poll(int timeoutMs,ChannelList* activeChannels) override;
    void updateChannel(Channel* channel) override;
    void removeChannle(Channel* channel) override;

private:
    static const int KInitEventListSize=16;

    //填写活跃的连接
    void fillActiveChannels(int numEvents,ChannelList *activeChannels)const;
    //更新channel通道
    void update(int operation,Channel *channel);

    using EventList = std::vector<epoll_event>;
    
    int epollfd_;
    EventList events_;
};