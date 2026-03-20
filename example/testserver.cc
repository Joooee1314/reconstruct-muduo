#include <mymuduo/TcpServer.h>
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
        server_.setThreadNum(3);
    }

    void start(){
        server_.start();
    }
private:
    void onConnection(const TcpConnectionPtr& conn){
        if(conn->connected()){
            cout<<"客户端"<<conn->peerAddress().toIpPort()<<"连接到服务器："
            <<conn->localAddress().toIpPort()<<" 状态为:在线"<<endl;
        }
        else{
            cout<<"客户端"<<conn->peerAddress().toIpPort()<<"断开连接"<<endl;
            conn->shutdown();
        }
    }

    void onMessage(const TcpConnectionPtr& conn,Buffer* buf,Timestamp time){
        string ret=buf->retrieveAllAsString();
        cout<<"recv data:"<<ret<<"time:"<<time.toString()<<endl;
        conn->send(ret);
    }

    void onWriteComplete(const TcpConnectionPtr& conn) {
        cout << "数据已全部发送，关闭连接：" << conn->peerAddress().toIpPort() << endl;
        conn->shutdown(); // 数据发完后再关闭写端
    }

    TcpServer server_;
    EventLoop* loop_;
};

int main(){
    EventLoop loop;
    InetAddress addr(8000,"127.0.0.1");
    EchoServer server(&loop,addr,"muduoserver");
    server.start();
    loop.loop();


}