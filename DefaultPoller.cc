#include "Poller.h"
#include "EpollPoller.h"

#include <stdlib.h>

Poller* Poller::newDefaultPoller(EventLoop *Loop){
    if(::getenv("MUDUO_USE_POLL")){
        return nullptr; //生成poll的实例
    }
    else{
        return new EpollPoller(Loop); //生成epoll的实例
    }
}