#include "Thread.h"
#include "Current_Thread.h"

#include <semaphore.h>

std::atomic_int32_t Thread::numCreated_{0};

Thread::Thread(ThreadFunc func,const std::string &name)
    : started_(false)
    , joined_(false)
    , tid_(0)
    , func_(std::move((func)))
    , name_(name)
{
    setDefaultName();
}

Thread::~Thread(){
    if(started_ && !joined_){
        thread_->detach();
    }
}

void Thread::start(){
    started_=true;
    sem_t sem;
    sem_init(&sem,false,0);

    thread_= std::shared_ptr<std::thread>(new std::thread([&](){
        //获取线程的tid
        tid_=CurrentThread::tid();
        sem_post(&sem);
        //开启一个新线程,专门执行该线程函数
        func_(); 
    }));
    
    //这里必须等待获取上面新创建线程的tid值
    sem_wait(&sem);
}

void Thread::join(){
    joined_=true;
    thread_->join();
}

void Thread::setDefaultName(){
    int num=++numCreated_;
    if(name_.empty()){
        char buf[32]={0};
        snprintf(buf,sizeof(buf),"Thread%d",num);
        name_=buf;
    }
}