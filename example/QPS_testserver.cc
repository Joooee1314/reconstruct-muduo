#include <mymuduo/TcpServer.h>
#include <mymuduo/Logger.h>
#include <iostream>
#include <string>

using namespace std;
using namespace placeholders;

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
        server_.setThreadNum(12);
    }

    void start(){
        server_.start();
    }
private:
    void onConnection(const TcpConnectionPtr& conn){
        if(conn->connected()){
            LOG_INFO("Client %s -> Server %s status: ONLINE\n", 
                     conn->peerAddress().toIpPort().c_str(),
                     conn->localAddress().toIpPort().c_str());
        }
        else{
            LOG_INFO("Client %s status: OFFLINE\n", conn->peerAddress().toIpPort().c_str());
            conn->shutdown();
        }
    }

    void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp time) 
    {
        string msg = buf->retrieveAllAsString();
        
        // 简单的“伪HTTP”处理：只要收到请求，就回一个 200 OK
        // 这样 wrk 就能识别并统计 QPS 了
        if (msg.find("\r\n\r\n") != string::npos) {
            string response = "HTTP/1.1 200 OK\r\n"
                            "Content-Type: text/plain\r\n"
                            "Content-Length: 11\r\n"
                            "Connection: keep-alive\r\n"
                            "\r\n"
                            "Hello World";
            conn->send(response);
        }
    }

    void onWriteComplete(const TcpConnectionPtr& conn) {
        LOG_INFO("Write complete to %s, shutting down write side\n", conn->peerAddress().toIpPort().c_str());
    }

    TcpServer server_;
    EventLoop* loop_;
};

int main(){
    
    EventLoop loop;
    InetAddress addr(8000,"127.0.0.1");
    EchoServer server(&loop,addr,"QPS_testserver");

    LOG_INFO("QPS_testserver is running...\n");

    server.start();
    loop.loop();


}