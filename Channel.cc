#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"

#include <sys/epoll.h>

const int Channel::KNoneEvent=0;
const int Channel::KReadEvent=EPOLLIN | EPOLLPRI;
const int Channel::KWriteEvent=EPOLLOUT;

Channel::Channel(EventLoop* Loop,int fd)
    : loop_(Loop)
    , fd_(fd)
    , events_(0)
    , revents_(0)
    , index_(-1)
    , tied_(false){}

Channel::~Channel(){}

void Channel::tie(const std::shared_ptr<void>&obj){
    tie_=obj;
    tied_=true;
}

/**
 * 当改变channel所表示fd的events事件后，update负责在poller中更改fd相应的事件epoll_ctl
 * 虽然channel中没有epoll的功能，channel和poller都属于eventloop，可以通过eventloop来调用poller
 */
void Channel::update(){
    loop_->updateChannel(this);
}

//在channel所属的eventloop中，把当前的channel删掉
void Channel::remove(){
    loop_->removeChannel(this);
}

void Channel::handleEvent(Timestamp receiveTime){
    if(tied_){
        std::shared_ptr<void>guard;
        guard=tie_.lock();
        if(guard){
            handleEventWithGuard(receiveTime);
        }
    }
    else{
        handleEventWithGuard(receiveTime);
    }
}

void Channel::handleEventWithGuard(Timestamp receiveTime){
    LOG_INFO("channel handleEvent revents:%d\n",revents_);

    if((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)){
        if(closeCallback_){
            closeCallback_();
        }
    }

    if(revents_ & EPOLLERR){
        if(errorCallback_){
            errorCallback_();
        }
    }

    if(revents_ & (EPOLLIN | EPOLLPRI)){
        if(readCallback_){
            readCallback_(receiveTime);
        }
    }

    if(revents_ & EPOLLOUT){
        if(writeCallback_){
            writeCallback_();
        }
    }
}

