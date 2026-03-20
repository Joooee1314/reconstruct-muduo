#include "TcpConnection.h"
#include "Logger.h"
#include "Socket.h"
#include "Channel.h"
#include "EventLoop.h"

#include <functional>
#include <errno.h>
#include <unistd.h>

static EventLoop* CheckLoopNotNull(EventLoop *loop){
    if(loop==nullptr){
        LOG_FATAL("%s:%s:%d TcpConnection Loop is null!\n",__FILE__, __FUNCTION__ ,__LINE__);
    }
    return loop;
}

TcpConnection::TcpConnection(EventLoop* loop ,
            const std::string &nameArg,
            int sockfd,
            const InetAddress& localAddr,
            const InetAddress& peerAddr)
            : loop_(CheckLoopNotNull(loop))
            , name_(nameArg)
            , state_(kConnecting)
            , reading_(true)
            , socket_(new Socket(sockfd))
            , channel_(new Channel(loop,sockfd))
            , localAddr_(localAddr)
            , peerAddr_(peerAddr)
            , highWaterMark_(64*1024*1024)//64M
{
    //下面是给channel设置相应的回调函数，poller给channel通知感兴趣事件发生，channel回调
    channel_->setReadCallback(std::bind(&TcpConnection::handleRead,this,std::placeholders::_1));
    channel_->setWriteCallback(std::bind(&TcpConnection::handleWrite,this));
    channel_->setCloseCallback(std::bind(&TcpConnection::handleClose,this));
    channel_->setErrorCallback(std::bind(&TcpConnection::handleError,this));

    LOG_INFO("TcpConnection::ctor[%s] at fd=%d\n",name_.c_str(),sockfd);
    socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection(){
    LOG_INFO("TcpConnection::dtor[%s] at fd=%d state=%d\n",name_.c_str(),channel_->fd(),(int)state_);
}

void TcpConnection::send(const std::string &buf){
    if(state_==kConnected){
        if(loop_->isInLoopThread()){
            sendInLoop(buf.c_str(),buf.size());
        }
        else{
            loop_->runInLoop(std::bind(
                &TcpConnection::sendInLoop,
                this,
                buf.c_str(),
                buf.size()
            ));
        }
    }
}

/**
 * 发送数据 应用写的快，而内核发送数据慢,需要把带发送数据写入缓冲区，而且设置水位回调
 */
void TcpConnection::sendInLoop(const void* data,size_t len){
    ssize_t nwrote = 0;
    size_t remaining = len;
    bool faultError= false;

    //之前调用过shutdown不能再发送了
    if(state_==kDisconnected){
        LOG_ERROR("disconnected,give up writing!");
        return;
    }

    //表示channel第一次开始写数据，并且缓冲区没有待发送数据
    if(!channel_->isWriting() && outputBuffer_.readableBytes()==0){
        nwrote = write(channel_->fd(),data,len);
        if(nwrote>=0){
            remaining = len-nwrote;
            if(remaining==0 && writeCompleteCallback_){
                //既然在这里数据一次性发送完，就不用再给channel设置epollout事件
                loop_->queueInLoop(std::bind(writeCompleteCallback_,shared_from_this()));
            }
        }
        else{
            nwrote=0;
            if(errno != EWOULDBLOCK){
                LOG_ERROR("TcpConnection::sendInLoop error");
                if(errno == EPIPE || errno == ECONNRESET){//SIGPIPE RESET
                    faultError=true;
                }
            }
        }
    }

     /**
      * 说明当前这一次write，并没有把数据全部发送出去，剩余的数据需要保存到缓冲区中
      * 然后给channel注册epollout事件,poller发现tcp发送缓冲区有空间，会通知相应的channel
      * 调用writeCallback_回调，也就是TcpConnection::handleWrite()把剩余发送缓冲区数据全部发送
      */
    if(!faultError && remaining>0){
        //目前发送缓冲区剩余的待发送数据的长度
        size_t oldlen = outputBuffer_.readableBytes();
        if(oldlen+remaining>=highWaterMark_
            && oldlen <highWaterMark_
            && highWaterMarkCallback_){
            loop_->queueInLoop(std::bind(highWaterMarkCallback_,shared_from_this(),oldlen+remaining));
        }
        outputBuffer_.append((char*)data + nwrote , remaining);
        if(!channel_->isWriting()){
            channel_->enableWriting(); //这里必须得注册
        }
    }
}


void TcpConnection::shutdown(){
    if(state_==kConnected){
        setState(kDisconnecting);
        loop_->runInLoop(
            std::bind(&TcpConnection::shutdownInLoop,this)
        );
    }
}

void TcpConnection::shutdownInLoop(){
    if(!channel_->isWriting()){ //说明outputbuffer数据已经全部发完
        socket_->shutdownWrite();//关闭写端
    }
}

//建立连接
void TcpConnection::connectEstablished(){
    setState(kConnected);
    channel_->tie(shared_from_this());
    channel_->enableReading();//向poller注册channel的epollin事件

    //新连接建立 执行回调
    connectionCallback_(shared_from_this());
}

//销毁链接
void TcpConnection::connectDestroyed(){
    if(state_ == kConnected){
        setState(kDisconnected);
        channel_->disableAll(); //把channel所有感兴趣事件从poller中del掉
        connectionCallback_(shared_from_this());
    }
    channel_->remove(); //把channel从poller中删掉
}

void TcpConnection::handleRead(Timestamp receiveTime){
    int savedErrno=0;
    ssize_t n =inputBuffer_.readFd(channel_->fd(),&savedErrno);
    if(n>0){
        //已建立连接的用户，有可读事件发生，调用用户传入的回调操作onMessage
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    }
    else if(n==0){
        handleClose();
    }
    else{
        errno = savedErrno;
        LOG_ERROR("TcpConnection::handleRead error!");
        handleError();
    }
}

void TcpConnection::handleWrite(){
    if(channel_->isWriting()){
        int savedErrno=0;
        ssize_t n =outputBuffer_.writeFd(channel_->fd(),&savedErrno);
        if(n>0){
            outputBuffer_.retrieve(n);
            if(outputBuffer_.readableBytes()==0){
                channel_->disableWriting();
                if(writeCompleteCallback_){
                    //唤醒loop_对应的thread线程执行回调
                    loop_->queueInLoop(
                        std::bind(writeCompleteCallback_,shared_from_this()));
                }
                if(state_==kDisconnecting){
                    shutdownInLoop();
                }
            }
        }
        else{
            LOG_ERROR("TcpConnection::handleWrite error!");
        }
    }
    else{
        LOG_ERROR("TcpConnection fd=%d is down,no more writing \n",channel_->fd());
    }
}

//poller =>channel::closeCallback => TcpConnection::handleClose()
void TcpConnection::handleClose(){
    LOG_INFO("TcpConnection::handleClose() fd=%d state=%d \n",channel_->fd(),(int)state_);
    setState(kDisconnected);
    channel_->disableAll();

    TcpConnectionPtr connPtr(shared_from_this());
    connectionCallback_(connPtr); //执行连接关闭的回调
    closeCallback_(connPtr); //关闭连接的回调 执行的是TcpServer给的回调方法
}

void TcpConnection::handleError(){
    int optval;
    socklen_t optlen = sizeof(optval);
    int err=0;
    if(getsockopt(channel_->fd(),SOL_SOCKET,SO_ERROR,&optval,&optlen)<0){
        err = errno;
    }
    else{
        err = optval;
    }
    LOG_ERROR("TcpConnection::handleError() name:%s - SO_ERROR:%d \n",name_.c_str(),err);
}

