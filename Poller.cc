#include "Poller.h"
#include "Channel.h"

Poller::Poller(EventLoop* Loop)
    :ownerLoop_(Loop)
{
}

bool Poller::hasChannel(Channel *channel) const{
    auto it=channels_.find(channel->fd());
    return it!=channels_.end() && it->second ==channel;
}

/**
 * 在语法上可以在poller.cc实现,但需要包含EpollPoller.h和PollPoller.h
 * 这就倒反天罡了，子类可以include基类，但是基类include子类的设计不好
 * 因此单独搞个DefaultPoller.cc来实现newDefaultPoller
 * static Poller* newDefaultPoller(EventLoop* Loop);
 */

