#pragma once

#include <vector>
#include <string>
#include <algorithm>

//网络库底层的缓冲区类型定义
class Buffer{
public:
    static const size_t kCheapPrepend=8;
    static const size_t kInitialSize=1024;

    explicit Buffer(size_t initialSize = kInitialSize)
        : buffer_(kCheapPrepend + initialSize)
        , readerIndex_(kCheapPrepend)
        , writerIndex_(kCheapPrepend)
    {}

    size_t readableBytes() const{
        return writerIndex_-readerIndex_;
    }

    size_t writableBytes() const{
        return buffer_.size() - writerIndex_;
    }

    size_t prependableBytes() const{
        return readerIndex_;
    }

    //返回缓冲区中可读数据的起始地址
    const char* peek() const{
        return begin() + readerIndex_;
    }

    void retrieve(size_t len){
        if(len < readableBytes()){
            readerIndex_+=len; //说明应用只读取了可读缓冲区数据的一部分len，还剩下readerIndex_+len -> writerIndex_ 没读
        }
        else{ //len==readableBytes()
            retrieveAll();
        }
    }

    void retrieveAll(){
        readerIndex_=writerIndex_ = kCheapPrepend;
    }

    //把onMessage函数上报的buffer数据转成string类型
    std::string retrieveAllAsString(){
        return retrieveAsString(readableBytes()); //应用可读取数据的长度
    }

    std::string retrieveAsString(size_t len){
        std::string result(peek(),len);
        retrieve(len);
        return result;
    }

    void ensureWritableBytes(size_t len){
        if(writableBytes()<len){
            makeSpace(len); // 扩容函数
        }
    }

    //把[data,data+len]内存上的数据添加到writable缓冲区中
    void append(const char * data,size_t len){
        ensureWritableBytes(len);
        std::copy(data, data+len, beginWrite());
        writerIndex_+=len;
    }

    char* beginWrite(){
        return begin() + writerIndex_;
    }

    const char* beginWrite() const {
        return begin() + writerIndex_;
    }

    //从 fd上读取数据
    ssize_t readFd(int fd, int* saveErrno);
    //通过fd发送数据
    ssize_t writeFd(int fd, int* saveErrno);
private:
    char* begin(){
        return &(*buffer_.begin()); //获取vector数组的首元素地址也就是起始地址
    }
    const char* begin() const{
        return &(*buffer_.begin()); 
    }

    void makeSpace(size_t len){
        //前置可写的空间加上剩下可写的空间确实不够用
        if(writableBytes() + prependableBytes() < len + kCheapPrepend){
            buffer_.resize(writerIndex_+len);
        }
        else{ //把剩下未读的数据挪到kCheapPrepend上来
            size_t readable = readableBytes();
            std::copy(begin() + readerIndex_,
                      begin() + writerIndex_,
                      begin() + kCheapPrepend);
            readerIndex_ = kCheapPrepend;
            writerIndex_ = readable +kCheapPrepend;
        }
    }

    std::vector<char> buffer_;
    size_t readerIndex_;
    size_t writerIndex_;
};