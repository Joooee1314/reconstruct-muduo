#include <mymuduo/TcpServer.h>
#include <mymuduo/Logger.h>
#include <mymuduo/AsyncLogging.h> 
#include <iostream>
#include <string>

using namespace std;
using namespace placeholders;

static AsyncLogging* g_asyncLog = nullptr;

void asyncOutput(const char* msg, int len) {
    if (g_asyncLog) {
        g_asyncLog->append(msg, len);
    }
}

class EchoServer{
public:
    EchoServer(EventLoop* loop,const InetAddress &listenAddr,const string nameArg)
        :server_(loop,listenAddr,nameArg)
        ,loop_(loop)
    {
        server_.setConnectionCallback(bind(&EchoServer::onConnection,this,_1));

        server_.setMessageCallback(bind(&EchoServer::onMessage,this,_1,_2,_3));

        server_.setWriteCompleteCallback(bind(&EchoServer::onWriteComplete, this, _1)
    );

        //设置合适的loop线程数量，一般等于cpu核数
        server_.setThreadNum(3);
    }

    void start(){
        server_.start();
    }
private:
    void onConnection(const TcpConnectionPtr& conn){
        if(conn->connected()){
            LOG_INFO("Client %s -> Server %s status: ONLINE", 
                     conn->peerAddress().toIpPort().c_str(),
                     conn->localAddress().toIpPort().c_str());
        }
        else{
            LOG_INFO("Client %s status: OFFLINE", conn->peerAddress().toIpPort().c_str());
            conn->shutdown();
        }
    }

    void onMessage(const TcpConnectionPtr& conn,Buffer* buf,Timestamp time){
        string ret=buf->retrieveAllAsString();
        LOG_INFO("Received data from %s: %s, Time: %s", conn->peerAddress().toIpPort().c_str(), ret.c_str(), time.toString().c_str());
        conn->send(ret);
    }

    void onWriteComplete(const TcpConnectionPtr& conn) {
        LOG_INFO("Write complete to %s, shutting down write side", conn->peerAddress().toIpPort().c_str());
        conn->shutdown(); // 数据发完后再关闭写端
    }

    TcpServer server_;
    EventLoop* loop_;
};

int main(){

    AsyncLogging log("testserver.log");
    g_asyncLog = &log;
    Logger::setOutput(asyncOutput); // 设置 Logger 的输出回调为 asyncOutput
    log.start();//启动异步日志
    
    EventLoop loop;
    InetAddress addr(8000,"127.0.0.1");
    EchoServer server(&loop,addr,"muduoserver");

    LOG_INFO("EchoServer is running...");

    server.start();
    loop.loop();


}