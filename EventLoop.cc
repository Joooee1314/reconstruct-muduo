#include "EventLoop.h"
#include "Logger.h"
#include "Poller.h"
#include "Channel.h"

#include <sys/eventfd.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <memory>
#include <chrono>

//防止一个线程创建多个EventLoop
__thread EventLoop *t_loopInThisThread = nullptr;

//定义默认的poller io复用接口超时时间
const int kPollTimeMs=10000;

//创建wakeupfd，用来notify唤醒subReactor处理新来的channel
int creatEventfd(){
    int evtfd=eventfd(0,EFD_NONBLOCK | EFD_CLOEXEC);
    if(evtfd < 0){
        LOG_FATAL("eventfd error:%d \n",errno);
    }
    return evtfd;
}

EventLoop::EventLoop()
    : looping_(false)
    , quit_(false)
    , callingPendingFunctors_(false)
    , threadId_(CurrentThread::tid())
    , poller_(Poller::newDefaultPoller(this))
    , wakeupFd_(creatEventfd())
    , wakeupChannel_(new Channel(this,wakeupFd_))
{
    LOG_DEBUG("EventLoop created %p in thread %d \n",this,threadId_);
    if(t_loopInThisThread){
        LOG_FATAL("Another EventLoop %p exists in this thread!%d \n",t_loopInThisThread,threadId_);
    }
    else{
        t_loopInThisThread=this;
    }

    //设置wakeup事件类型以及发生事件后的回调
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead,this));
    //每一个eventloop都将监听wakeupchannel的EPOLLIN事件
    wakeupChannel_->enableReading();
}

EventLoop::~EventLoop(){
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    close(wakeupFd_);
    t_loopInThisThread=nullptr;
}

//开启事件循环
void EventLoop::loop(){
    looping_=true;
    quit_=false;

    LOG_INFO("EventLoop %p start looping \n",this);
    //获取单调时钟当前的时间
    lastTimerTime_ = std::chrono::steady_clock::now();
    while(!quit_){
        activeChannels_.clear();
        pollReturnTime_ = poller_->poll(kPollTimeMs,&activeChannels_);
        for(Channel* channel:activeChannels_){
            //Poller监听哪些channel发生事件，然后上报给eventloop，通知channel处理事件
            channel->handleEvent(pollReturnTime_);
        }
        //执行当前eventloop事件循环需要处理的回调操作
        /**
         * IO线程 mainLoop accept fd => connfd封装到channle => wakeup subloop
         * mainloop率先注册一个回调cb(需要subloop来执行) wakeup subloop执行下面的方法
         */
        doPendingFunctors();

        if (timerCallback_) {
            //获取单调时钟当前时间，判断是否需要执行定时器回调函数
            auto now = std::chrono::steady_clock::now();
            if (now - lastTimerTime_ >= std::chrono::seconds(1)) {
                lastTimerTime_ = now;
                timerCallback_();
            }
        }
    }

    LOG_INFO("EventLoop %p stop looping \n",this);
}

//退出事件循环
void EventLoop::quit(){
    quit_=true;

    //如果在其他线程调用quit，在一个subloop中调用了mainloop的quit
    if(!isInLoopThread()){
        wakeup();
    }
}

//在当前loop执行cb
void EventLoop::runInLoop(Functor cb){
    if(isInLoopThread()){
        cb();
    }
    else{
        queueInLoop(cb);
    }
}

//把cb放入队列中，唤醒loop所在线程执行cb
void EventLoop::queueInLoop(Functor cb){
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(cb);
    }

    //唤醒相应需要执行上述回调操作的loop线程
    //callingPendingFunctors_意思是：当前loop正在执行回调，但是loop又有了新的回调
    if(!isInLoopThread() || callingPendingFunctors_){
        wakeup();
    }
}

void EventLoop::handleRead(){
    uint64_t one=1;
    ssize_t n= read(wakeupFd_,&one,sizeof(one));
    if(n!=sizeof(one)){
        LOG_ERROR("EventLoop::handleRead() reads %lu bytes instead of 8",n);
    }
}

//用来唤醒loop所在线程
void EventLoop::wakeup(){
    uint64_t one =1;
    ssize_t n = write(wakeupFd_,&one,sizeof(one));
    if(n!=sizeof(one)){
        LOG_ERROR("EventLoop::wakeup() writes %lu bytes instead of 8 \n",n);
    }
}

//EventLoop的方法 => Poller的方法
void EventLoop::updateChannel(Channel* channel){
    poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel* channel){
    poller_->removeChannle(channel);
}

bool EventLoop::hasChannel(Channel* channel){
    return poller_->hasChannel(channel);
}

//执行回调
void EventLoop::doPendingFunctors(){
    std::vector<Functor> functors;
    callingPendingFunctors_=true;

    {
        std::lock_guard<std::mutex>lock(mutex_);
        functors.swap(pendingFunctors_);
    }

    for(const Functor &functor:functors){
        functor(); //执行当前loop需要执行的回调操作
    }

}