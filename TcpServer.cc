#include "TcpServer.h"
#include "Logger.h"
#include "TcpConnection.h"

static EventLoop* CheckLoopNotNull(EventLoop *loop){
    if(loop==nullptr){
        LOG_FATAL("%s:%s:%d mainLoop is null!\n",__FILE__, __FUNCTION__ ,__LINE__);
    }
    return loop;
}

TcpServer::TcpServer(EventLoop* loop,
            const InetAddress &listenAddr,
            const std::string nameArg,
            size_t timeout,
            Option option)
            : loop_(CheckLoopNotNull(loop))
            , ipPort_(listenAddr.toIpPort())
            , name_(nameArg)
            , acceptor_(new Acceptor(loop, listenAddr, option=kReusePort))
            , threadPool_(new EventLoopThreadPool(loop,name_))
            , connectionCallback_()
            , messageCallback_()
            , nextConnId_(1)
            , started_(0)
            , timerWheel_(timeout)
            , idleTimeout_(timeout)
{
    //当有新用户连接的时候，会执行TcpServer::newConnection回调
    acceptor_->setNewConnectionCallback([this](int sockfd,const InetAddress& peerAddr){
        this->newConnection(sockfd,peerAddr);
    });
}

TcpServer::~TcpServer(){
    for(auto& item : connections_){
        //局部shared_ptr智能指针对象，出右括号可以自动释放new出来的TcpConnectionPtr对象
        TcpConnectionPtr conn(item.second); 
        //指针不再指向原来的对象 将会指向局部对象conn 如果直接reset而没有局部对象conn，则无法使用conn->getLoop()->runInLoop这些方法
        item.second.reset(); 
        conn->getLoop()->runInLoop(
            std::bind(&TcpConnection::connectDestroyed,conn)
        );

    }
}

void TcpServer::setThreadNum(int numThreads){
    threadPool_->setThreadNum(numThreads);
}

void TcpServer::start(){
    if(started_++==0){ //防止一个TcpServer对象被start多次
        threadPool_->start(threadInitCallback_); //启动底层的loop线程池
        if(idleTimeout_>0){
            loop_->setTimerCallback(std::bind(&TimerWheel::onTimerTick, &timerWheel_));
        }
        loop_->runInLoop((std::bind(&Acceptor::listen,acceptor_.get())));
    }

}

//有一个新的客户端连接，acceptor就会执行这个回调
void TcpServer::newConnection(int sockfd,const InetAddress &peerAddr){
    //轮询算法，选择一个subloop来管理channel
    EventLoop* ioLoop = threadPool_->getNextLoop();
    char buf[64]={0};
    snprintf(buf,sizeof(buf),"-%s#%d",ipPort_.c_str(),nextConnId_);
    ++nextConnId_;
    std::string connName=name_+buf;

    LOG_INFO("TcpServer::newConnection [%s] - new connection [%s] from %s \n",
        name_.c_str(),connName.c_str(),peerAddr.toIpPort().c_str());

    //通过sockfd获取其绑定的本机的ip地址和端口
    sockaddr_in local{};
    socklen_t addrlen=sizeof(local);
    if(getsockname(sockfd,(sockaddr*)&local,&addrlen)<0){
        LOG_ERROR("sockets::getLocalAddr error");
    }
    InetAddress localAddr(local);

    //根据连接成功的connfd创建TcpConnection连接对象
    TcpConnectionPtr conn(new TcpConnection(
                        ioLoop,
                        connName,
                        sockfd,
                        localAddr,
                        peerAddr));
    connections_[connName]=conn;
    //下面的回调都是用户设置给TcpServer => TcpConnection =>Channel =>Poller =>notify Channel回调
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback([this](const TcpConnectionPtr& conn, Buffer* buf, Timestamp time){
        if(idleTimeout_>0){
            timerWheel_.updateConnection(conn);
        }
        if (messageCallback_) {
            messageCallback_(conn, buf, time);
        }
    });
    conn->setWriteCompleteCallback(writeCompleteCallback_);

    //设置了如何关闭连接的回调
    conn->setCloseCallback(
        std::bind(&TcpServer::removeConnection,this,std::placeholders::_1)
    );

    if(idleTimeout_>0){
        // 连接初次激活时加入时间轮
        timerWheel_.updateConnection(conn);
    }
    //直接调用TcpConnection::connecEstablished
    ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished,conn));

}

void TcpServer::removeConnection(const TcpConnectionPtr &conn){
    loop_->runInLoop(
        std::bind(&TcpServer::removeConnectionInLoop,this,conn)
    );
}
void TcpServer::removeConnectionInLoop(const TcpConnectionPtr &conn){
    LOG_INFO("TcpServer::removeConnectionInLoop [%s] - connection %s\n",
        name_.c_str(),conn->name().c_str());
    connections_.erase(conn->name());
    EventLoop* ioLoop = conn->getLoop();
    ioLoop->queueInLoop(
        std::bind(&TcpConnection::connectDestroyed,conn)
    );
}